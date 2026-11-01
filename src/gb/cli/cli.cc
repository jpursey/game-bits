// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli.h"

#include <cmath>
#include <cstdint>
#include <string_view>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
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
    // A word starting with "-" then a digit or "." is a negative number.
    if (fields.short_name != 0 &&
        (!absl::ascii_isgraph(fields.short_name) || fields.short_name == '-' ||
         fields.short_name == '.' || absl::ascii_isdigit(fields.short_name))) {
      return Error(absl::StrCat("flag \"--", fields.name,
                                "\" has an invalid short name"));
    }
    const absl::Status status = flag.ValidateValueFields();
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
  for (const internal::CliArgRef& arg : path_.back()->args) {
    const internal::CliArgFields fields = arg.GetFields();
    if (fields.name.empty()) {
      return Error("argument has no name");
    }
    const absl::Status status = arg.ValidateValueFields();
    if (!status.ok()) {
      return Error(
          absl::StrCat("argument <", fields.name, "> ", status.message()));
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

//==============================================================================
// Value parsing
//==============================================================================

// Returns whether `text` is written as a base 10 integer: an optional sign,
// then digits.
bool IsCliInteger(std::string_view text) {
  if (text.starts_with('+') || text.starts_with('-')) {
    text.remove_prefix(1);
  }
  return !text.empty() && absl::c_all_of(text, absl::ascii_isdigit);
}

}  // namespace

namespace internal {

absl::Status ParseCliText(std::string_view text, std::string_view* value) {
  *value = text;
  return absl::OkStatus();
}

absl::Status ParseCliText(std::string_view text, int64_t* value) {
  if (!IsCliInteger(text)) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", text, "\" is not an integer"));
  }

  // The text is an integer, so it can only fail by being out of range.
  if (!absl::SimpleAtoi(text, value)) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", text, "\" is out of range"));
  }
  return absl::OkStatus();
}

absl::Status ParseCliText(std::string_view text, double* value) {
  // SimpleAtod() accepts surrounding space, which isn't part of a number.
  if (text != absl::StripAsciiWhitespace(text) ||
      !absl::SimpleAtod(text, value)) {
    return absl::InvalidArgumentError(
        absl::StrCat("\"", text, "\" is not a number"));
  }

  // A value out of range is parsed as infinite, and has digits, unlike "inf"
  // and "nan".
  if (!std::isfinite(*value)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "\"", text, "\" is ",
        absl::c_any_of(text, absl::ascii_isdigit) ? "out of range"
                                                  : "not a finite number"));
  }
  return absl::OkStatus();
}

absl::StatusOr<int> FindCliName(absl::Span<const std::string_view> names,
                                std::string_view text) {
  const int count = static_cast<int>(names.size());
  for (int i = 0; i < count; ++i) {
    if (names[i] == text) {
      return i;
    }
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "\"", text, "\" is not one of: ", absl::StrJoin(names, ", ")));
}

absl::Status ValidateCliNames(absl::Span<const std::string_view> names) {
  if (names.empty()) {
    return absl::InvalidArgumentError("is an enumeration with no names");
  }
  absl::flat_hash_set<std::string_view> seen;
  for (const std::string_view name : names) {
    if (name.empty()) {
      return absl::InvalidArgumentError("has an empty enumeration name");
    }

    // A positional argument starting with "-" is parsed as a flag.
    if (name.starts_with('-')) {
      return absl::InvalidArgumentError(absl::StrCat(
          "has enumeration name \"", name, "\", which starts with \"-\""));
    }
    if (!seen.insert(name).second) {
      return absl::InvalidArgumentError(
          absl::StrCat("has duplicate enumeration name \"", name, "\""));
    }
  }
  return absl::OkStatus();
}

}  // namespace internal

//==============================================================================
// ValidateCliCommand
//==============================================================================

absl::Status ValidateCliCommand(const CliCommand* program) {
  return CliCommandValidator().Validate(program);
}

}  // namespace gb
