// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli.h"

#include <string_view>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "gb/cli/cli_internal.h"

namespace gb {

namespace {

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
  if (command->args.size() != 0 && command->commands.size() != 0) {
    return Error("command has both arguments and commands");
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
    for (const internal::CliFlagRef& flag : path_[i]->flags) {
      const internal::CliFlagFields fields = flag.GetFields();
      names.insert(fields.name);
      if (fields.short_name != 0) {
        short_names.insert(fields.short_name);
      }
    }
  }

  for (const internal::CliFlagRef& flag : path_.back()->flags) {
    const internal::CliFlagFields fields = flag.GetFields();
    if (fields.name.empty()) {
      return Error("flag has no name");
    }
    if (fields.name.starts_with('-') ||
        fields.name.find('=') != std::string_view::npos) {
      return Error(absl::StrCat("flag name \"", fields.name,
                                "\" starts with \"-\" or contains \"=\""));
    }
    if (fields.short_name != 0 &&
        (!absl::ascii_isgraph(fields.short_name) || fields.short_name == '-')) {
      return Error(absl::StrCat("flag \"--", fields.name,
                                "\" has an invalid short name"));
    }
    if (!flag.TakesValue()) {
      std::string_view value_field;
      if (!fields.value_name.empty()) {
        value_field = "a value name";
      } else if (fields.has_default_value) {
        value_field = "a default value";
      } else if (fields.has_validators) {
        value_field = "validators";
      }
      if (!value_field.empty()) {
        return Error(absl::StrCat("bool flag \"--", fields.name, "\" has ",
                                  value_field, ", but takes no value"));
      }
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
  for (const internal::CliArgRef& arg : path_.back()->args) {
    const internal::CliArgFields fields = arg.GetFields();
    if (fields.name.empty()) {
      return Error("argument has no name");
    }
    if (!names.insert(fields.name).second) {
      return Error(absl::StrCat("duplicate argument <", fields.name, ">"));
    }
    if (fields.optional) {
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
