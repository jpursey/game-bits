// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli.h"

#include <initializer_list>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "gb/cli/cli_internal.h"

namespace gb {

namespace {

//==============================================================================
// Value fields
//
// These validate the fields of a definition that depend on its value type,
// returning an error worded to follow the definition's name ("flag \"--mode\"
// ...").
//==============================================================================

template <typename T>
absl::Status ValidateChoices(std::initializer_list<CliChoice> choices) {
  if (choices.size() == 0) {
    return absl::OkStatus();
  }
  if constexpr (!std::is_same_v<T, std::string_view>) {
    return absl::InvalidArgumentError("isn't a string, so can't have choices");
  } else {
    absl::flat_hash_set<std::string_view> names;
    for (const CliChoice& choice : choices) {
      if (choice.name.empty()) {
        return absl::InvalidArgumentError("has an empty choice");
      }

      // A positional argument starting with "-" is parsed as a flag.
      if (choice.name.starts_with('-')) {
        return absl::InvalidArgumentError(absl::StrCat(
            "has choice \"", choice.name, "\", which starts with \"-\""));
      }
      if (!names.insert(choice.name).second) {
        return absl::InvalidArgumentError(
            absl::StrCat("has duplicate choice \"", choice.name, "\""));
      }
    }
    return absl::OkStatus();
  }
}

template <typename T>
absl::Status ValidateValidators(
    std::initializer_list<const CliValidator<T>*> validators) {
  for (const CliValidator<T>* validator : validators) {
    if (validator == nullptr) {
      return absl::InvalidArgumentError("has a null validator");
    }
    if (validator->validate == nullptr) {
      return absl::InvalidArgumentError("has a validator with no function");
    }
    for (const ContextConstraint* constraint : validator->constraints) {
      if (constraint == nullptr) {
        return absl::InvalidArgumentError(
            "has a validator with a null constraint");
      }

      // Validators only read the context.
      if (constraint->presence != ContextConstraint::kInRequired &&
          constraint->presence != ContextConstraint::kInOptional) {
        return absl::InvalidArgumentError(
            "has a validator with a constraint that isn't an input");
      }
    }
  }
  return absl::OkStatus();
}

template <typename T>
absl::Status ValidateValueFields(const CliFlag<T>& flag) {
  if constexpr (std::is_same_v<T, bool>) {
    if (!flag.value_name.empty()) {
      return absl::InvalidArgumentError(
          "is a bool, so can't have a value name");
    }
    if (!flag.optional) {
      return absl::InvalidArgumentError("is a bool, so can't be required");
    }
    if (flag.default_value.has_value()) {
      return absl::InvalidArgumentError(
          "is a bool, so can't have a default value");
    }
    if (flag.validators.size() != 0) {
      return absl::InvalidArgumentError("is a bool, so can't have validators");
    }
  }
  if (!flag.optional && flag.default_value.has_value()) {
    return absl::InvalidArgumentError(
        "is required, so can't have a default value");
  }
  const absl::Status status = ValidateValidators<T>(flag.validators);
  if (!status.ok()) {
    return status;
  }
  return ValidateChoices<T>(flag.choices);
}

template <typename T>
absl::Status ValidateValueFields(const CliArg<T>& arg) {
  const absl::Status status = ValidateValidators<T>(arg.validators);
  if (!status.ok()) {
    return status;
  }
  return ValidateChoices<T>(arg.choices);
}

//==============================================================================
// CliCommandValidator
//==============================================================================

// Validates a command tree, keeping the path to the command being validated.
class CliCommandValidator {
 public:
  absl::Status Validate(const CliCommand* command);

 private:
  // Validate the flags and arguments of the command at the end of the path.
  absl::Status ValidateFlags() const;
  absl::Status ValidateArgs() const;

  // Returns an error for the command at the end of the path.
  absl::Status Error(std::string_view message) const;

  std::vector<const CliCommand*> path_;
};

absl::Status CliCommandValidator::Validate(const CliCommand* command) {
  if (command == nullptr) {
    return Error("null command");
  }
  if (absl::c_linear_search(path_, command)) {
    return Error(
        absl::StrCat("command \"", command->name, "\" contains itself"));
  }
  path_.push_back(command);
  if (command->name.empty()) {
    return Error("command has no name");
  }
  if (command->name.starts_with('-')) {
    return Error("command name starts with \"-\"");
  }
  if (absl::c_linear_search(command->constraints, nullptr)) {
    return Error("command has a null constraint");
  }
  absl::Status status = ValidateFlags();
  if (!status.ok()) {
    return status;
  }
  status = ValidateArgs();
  if (!status.ok()) {
    return status;
  }
  absl::flat_hash_set<std::string_view> names;
  for (const CliCommand* subcommand : command->commands) {
    if (subcommand != nullptr && !names.insert(subcommand->name).second) {
      return Error(
          absl::StrCat("duplicate command \"", subcommand->name, "\""));
    }
    status = Validate(subcommand);
    if (!status.ok()) {
      return status;
    }
  }
  path_.pop_back();
  return absl::OkStatus();
}

absl::Status CliCommandValidator::ValidateFlags() const {
  // Names must be unique across the command and the commands above it. The
  // commands above were already validated, so only this command's flags can be
  // duplicates.
  absl::flat_hash_set<std::string_view> names;
  absl::flat_hash_set<char> short_names;
  for (int i = 0; i + 1 < static_cast<int>(path_.size()); ++i) {
    for (const CliAnyFlag& flag : path_[i]->flags) {
      const internal::CliFlagFields fields = internal::GetCliFlagFields(flag);
      names.insert(fields.name);
      if (fields.short_name != 0) {
        short_names.insert(fields.short_name);
      }
    }
  }

  for (const CliAnyFlag& flag : path_.back()->flags) {
    const internal::CliFlagFields fields = internal::GetCliFlagFields(flag);
    if (fields.name.empty()) {
      return Error("flag has no name");
    }
    if (fields.name.starts_with('-') ||
        fields.name.find('=') != std::string_view::npos) {
      return Error(absl::StrCat("flag name \"", fields.name,
                                "\" starts with \"-\" or contains \"=\""));
    }
    // A word starting with "-" then a digit or "." is a negative number.
    if (fields.short_name != 0 &&
        (!absl::ascii_isgraph(fields.short_name) || fields.short_name == '-' ||
         fields.short_name == '.' || absl::ascii_isdigit(fields.short_name))) {
      return Error(absl::StrCat("flag \"--", fields.name,
                                "\" has an invalid short name"));
    }
    const CliFlag<bool>& help = internal::kCliHelpFlag;
    if (fields.name == help.name) {
      return Error(
          absl::StrCat("flag \"--", fields.name, "\" is reserved for help"));
    }
    if (fields.short_name == help.short_name) {
      return Error(absl::StrCat("flag \"--", fields.name,
                                "\" has short name \"-",
                                std::string_view(&fields.short_name, 1),
                                "\", which is reserved for "
                                "help"));
    }
    const absl::Status status = std::visit(
        [](const auto* typed) { return ValidateValueFields(*typed); }, flag);
    if (!status.ok()) {
      return Error(
          absl::StrCat("flag \"--", fields.name, "\" ", status.message()));
    }
    if (!names.insert(fields.name).second) {
      return Error(absl::StrCat("duplicate flag \"--", fields.name, "\""));
    }
    if (fields.short_name != 0 &&
        !short_names.insert(fields.short_name).second) {
      return Error(absl::StrCat("duplicate flag \"-",
                                std::string_view(&fields.short_name, 1), "\""));
    }
  }
  return absl::OkStatus();
}

absl::Status CliCommandValidator::ValidateArgs() const {
  absl::flat_hash_set<std::string_view> names;
  bool after_optional = false;
  for (const CliAnyArg& arg : path_.back()->args) {
    const internal::CliArgFields fields = internal::GetCliArgFields(arg);
    if (fields.name.empty()) {
      return Error("argument has no name");
    }
    const absl::Status status = std::visit(
        [](const auto* typed) { return ValidateValueFields(*typed); }, arg);
    if (!status.ok()) {
      return Error(
          absl::StrCat("argument <", fields.name, "> ", status.message()));
    }
    if (!names.insert(fields.name).second) {
      return Error(absl::StrCat("duplicate argument <", fields.name, ">"));
    }
    if (fields.optional) {
      // A command's subcommand follows its arguments, so it must always take
      // the same number of words for them.
      if (path_.back()->commands.size() != 0) {
        return Error(absl::StrCat("optional argument <", fields.name,
                                  "> is in a command with commands"));
      }
      after_optional = true;
    } else if (after_optional) {
      return Error(absl::StrCat("required argument <", fields.name,
                                "> follows an optional one"));
    }
  }
  return absl::OkStatus();
}

absl::Status CliCommandValidator::Error(std::string_view message) const {
  return absl::InvalidArgumentError(
      absl::StrCat(internal::GetCliPathName(path_), ": ", message));
}

}  // namespace

//==============================================================================
// ValidateCliCommand
//==============================================================================

absl::Status ValidateCliCommand(const CliCommand* program) {
  return CliCommandValidator().Validate(program);
}

}  // namespace gb
