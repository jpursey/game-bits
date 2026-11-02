// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_internal.h"

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace gb::internal {

const CliFlag<bool> kCliHelpFlag = {
    .name = "help",
    .short_name = 'h',
    .help = "Writes help for this command.",
};

namespace {

//==============================================================================
// Fields
//==============================================================================

template <typename T>
CliFlagFields GetFields(const CliFlag<T>& flag) {
  return {
      .definition = &flag,
      .name = flag.name,
      .short_name = flag.short_name,
  };
}

template <typename T>
CliArgFields GetFields(const CliArg<T>& arg) {
  return {.definition = &arg, .name = arg.name, .optional = arg.optional};
}

//==============================================================================
// Value parsing
//==============================================================================

// Returns whether `text` is written as a base 10 integer: an optional sign,
// then digits.
bool IsInteger(std::string_view text) {
  if (text.starts_with('+') || text.starts_with('-')) {
    text.remove_prefix(1);
  }
  return !text.empty() && absl::c_all_of(text, absl::ascii_isdigit);
}

// Parses `text` into `value`, returning an error saying what is wrong with it
// if it isn't of the type. A number must be only the number, with no
// surrounding space: base 10 for an integer, and finite for floating point,
// in the type's range. Any text is a string. A bool flag takes no value, and is
// true when given.
absl::Status ParseText(std::string_view, bool* value) {
  *value = true;
  return absl::OkStatus();
}

absl::Status ParseText(std::string_view text, std::string_view* value) {
  *value = text;
  return absl::OkStatus();
}

absl::Status ParseText(std::string_view text, int64_t* value) {
  if (!IsInteger(text)) {
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

absl::Status ParseText(std::string_view text, double* value) {
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

// Validates that `text` is one of `choices`, if there are any.
absl::Status ValidateChoice(std::string_view text,
                            std::initializer_list<CliChoice> choices) {
  if (choices.size() == 0 ||
      absl::c_any_of(choices, [text](const CliChoice& choice) {
        return choice.name == text;
      })) {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(
      absl::StrCat("\"", text, "\" is not one of: ",
                   absl::StrJoin(choices, ", ",
                                 [](std::string* out, const CliChoice& choice) {
                                   absl::StrAppend(out, choice.name);
                                 })));
}

template <typename T>
absl::StatusOr<CliValue> ParseValue(
    const ValidatedContext& context, std::string_view text,
    std::initializer_list<CliChoice> choices,
    std::initializer_list<const CliValidator<T>*> validators) {
  T value = {};
  absl::Status status = ParseText(text, &value);
  if (!status.ok()) {
    return status;
  }
  status = ValidateChoice(text, choices);
  if (!status.ok()) {
    return status;
  }
  for (const CliValidator<T>* validator : validators) {
    status = validator->validate(context, value);
    if (!status.ok()) {
      return status;
    }
  }
  return CliValue(value);
}

// Parses the value of `definition`, a CliFlag<T> or CliArg<T>, from `text`.
template <typename Definition>
absl::StatusOr<CliValue> Parse(const ValidatedContext& context,
                               const Definition& definition,
                               std::string_view text) {
  return ParseValue(context, text, definition.choices, definition.validators);
}

// Appends the constraints of `definition`'s validators, for a CliFlag<T> or
// CliArg<T>.
template <typename Definition>
void AppendConstraints(const Definition& definition,
                       std::vector<ContextConstraint>* constraints) {
  for (const auto* validator : definition.validators) {
    for (const ContextConstraint* constraint : validator->constraints) {
      constraints->push_back(*constraint);
    }
  }
}

}  // namespace

//==============================================================================
// Shared functions
//==============================================================================

CliFlagFields GetCliFlagFields(const CliAnyFlag& flag) {
  return std::visit([](const auto* typed) { return GetFields(*typed); }, flag);
}

CliArgFields GetCliArgFields(const CliAnyArg& arg) {
  return std::visit([](const auto* typed) { return GetFields(*typed); }, arg);
}

absl::StatusOr<CliValue> ParseCliValue(const ValidatedContext& context,
                                       const CliAnyFlag& flag,
                                       std::string_view text) {
  return std::visit(
      [&context, text](const auto* typed) {
        return Parse(context, *typed, text);
      },
      flag);
}

absl::StatusOr<CliValue> ParseCliValue(const ValidatedContext& context,
                                       const CliAnyArg& arg,
                                       std::string_view text) {
  return std::visit(
      [&context, text](const auto* typed) {
        return Parse(context, *typed, text);
      },
      arg);
}

void AppendCliValidatorConstraints(
    const CliAnyFlag& flag, std::vector<ContextConstraint>* constraints) {
  std::visit([constraints](
                 const auto* typed) { AppendConstraints(*typed, constraints); },
             flag);
}

void AppendCliValidatorConstraints(
    const CliAnyArg& arg, std::vector<ContextConstraint>* constraints) {
  std::visit([constraints](
                 const auto* typed) { AppendConstraints(*typed, constraints); },
             arg);
}

std::string GetCliPathName(absl::Span<const CliCommand* const> path) {
  return absl::StrJoin(path, " ",
                       [](std::string* out, const CliCommand* command) {
                         absl::StrAppend(out, command->name);
                       });
}

}  // namespace gb::internal
