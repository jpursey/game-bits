// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_H_
#define GB_CLI_CLI_H_

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace gb {

//==============================================================================
// Command line definitions
//
// A program's command line is defined as constant data: a tree of commands,
// rooted at the program itself, each with its own flags, positional arguments,
// and subcommands. For example:
//
//   const gb::CliFlag<std::string_view> kOutput = {
//       .name = "output",
//       .short_name = 'o',
//       .value_name = "file",
//       .help = "Where to write the result.",
//   };
//   const gb::CliFlag<bool> kVerbose = {
//       .name = "verbose",
//       .help = "Writes what it does as it does it.",
//   };
//   const gb::CliArg<std::string_view> kInput = {
//       .name = "input file",
//       .help = "The file to read.",
//   };
//
//   bool Run(const gb::CliValues& values);
//
//   const gb::CliCommand kProgram = {
//       .name = "convert",
//       .help = "Converts a file.",
//       .flags = {&kOutput, &kVerbose},
//       .args = {&kInput},
//       .run = Run,
//   };
//
// The command that runs reads the values given for them from CliValues (see
// cli_values.h), using the same definitions.
//
// Definitions must be `const` with static storage: at namespace scope, or
// `static` within a function. Their lists are initializer lists, which point
// into constant data, so a definition that is a temporary or a local, or a copy
// of one, is left pointing at nothing once it goes out of scope.
//
// Definitions with lists can't be constexpr (see internal::CliNotConstexpr), so
// they are initialized when the program starts, not at compile time. A
// definition may refer to others by address, but must not be read before
// main() starts.
//==============================================================================

class CliValues;

namespace internal {

// A value given on the command line. A bool flag that is given is true, and an
// enumeration is held as int64_t.
using CliValue = std::variant<bool, std::string_view, int64_t, double>;

template <typename T, typename Variant>
inline constexpr bool kIsVariantAlternative = false;
template <typename T, typename... Types>
inline constexpr bool kIsVariantAlternative<T, std::variant<Types...>> =
    (std::is_same_v<T, Types> || ...);

// Whether `T` is a type a flag or argument value can have: an enumeration, or
// one CliValue holds.
template <typename T>
inline constexpr bool kIsCliValueType =
    std::is_enum_v<T> || kIsVariantAlternative<T, CliValue>;

// The type a CliValue holds a value of type `T` as.
template <typename T>
using CliStoredType = std::conditional_t<std::is_enum_v<T>, int64_t, T>;

// A member that makes a definition with lists fail to compile as constexpr.
// MSVC miscompiles a constexpr aggregate with an initializer list member: the
// list's elements are left zero at run time. The same definition as `const` is
// correct, but is initialized dynamically, at startup.
struct CliNotConstexpr {
  // Not constexpr, and not defaulted, so a definition holding this can't be
  // constexpr.
  CliNotConstexpr() {}
};

}  // namespace internal

// A function that judges whether a flag's or argument's value is acceptable.
// It returns an error saying what is wrong with `value`, or OK if it is
// acceptable. Common ones are in cli_validators.h.
template <typename T>
using CliValidator = absl::Status (*)(const T& value);

// One name an enumeration flag or argument accepts, and the value it gives.
template <typename T>
struct CliName {
  // The name, as given on the command line. This must not be empty.
  std::string_view name;

  // The value the name gives.
  T value;

  // Help for the name.
  std::string_view help;
};

// A definition's `names` for a type that isn't an enumeration: it has none, so
// any names given fail to compile.
struct CliNoNames {};

// The type of a definition's `names`: a list of CliName<T> for an
// enumeration, and CliNoNames for any other type.
template <typename T>
using CliNames =
    std::conditional_t<std::is_enum_v<T>, std::initializer_list<CliName<T>>,
                       CliNoNames>;

// A flag: `--name value`, `--name=value`, or `-n value`.
//
// A CliFlag<bool> takes no value: it is given as `--name` or `-n`, which makes
// it true, and is otherwise false. It must leave `value_name`, `default_value`,
// and `validators` unset.
template <typename T>
struct CliFlag {
  static_assert(internal::kIsCliValueType<T>,
                "Unsupported command line value type");

  // The flag's long name, without the leading "--". This must not be empty,
  // start with "-", or contain "=".
  std::string_view name;

  // The flag's short name, without the leading "-", or 0 if it has none. This
  // must be a visible ASCII character other than "-", ".", or a digit, as a
  // word starting with "-" then a digit or "." is a negative number.
  char short_name = 0;

  // What the flag's value is, as shown in help: "--output <file>". If this is
  // empty, the flag's name is used instead.
  std::string_view value_name;

  // Help for the flag.
  std::string_view help;

  // The names an enumeration flag accepts. There must be at least one, and
  // each must be unique. Only an enumeration has names: for any other type,
  // names fail to compile.
  CliNames<T> names;

  // The value if the flag isn't given, if any. This is not validated.
  std::optional<T> default_value;

  // Validators for the value, run in order until one fails.
  std::initializer_list<CliValidator<T>> validators;

  // Leave unset. This is here only to prevent the definition from being
  // constexpr.
  internal::CliNotConstexpr not_constexpr;
};

// A positional argument.
template <typename T>
struct CliArg {
  static_assert(internal::kIsCliValueType<T>,
                "Unsupported command line value type");
  static_assert(!std::is_same_v<T, bool>,
                "A positional argument can't be bool; use a CliFlag<bool>");

  // The argument's name, as shown in help and errors: "<input file>". This
  // must not be empty.
  std::string_view name;

  // Help for the argument.
  std::string_view help;

  // Whether the argument may be left out. Optional arguments must follow all
  // required ones in a command.
  bool optional = false;

  // The names an enumeration argument accepts. There must be at least one, and
  // each must be unique. Only an enumeration has names: for any other type,
  // names fail to compile.
  CliNames<T> names;

  // The value if the argument isn't given, if any. Only an optional argument
  // can be left out, so a required argument's default is never used. This is
  // not validated.
  std::optional<T> default_value;

  // Validators for the value, run in order until one fails.
  std::initializer_list<CliValidator<T>> validators;

  // Leave unset. This is here only to prevent the definition from being
  // constexpr.
  internal::CliNotConstexpr not_constexpr;
};

namespace internal {

// The fields every flag has, whatever its type.
struct CliFlagFields {
  // The flag's long name, without the leading "--".
  std::string_view name;

  // The flag's short name, without the leading "-", or 0 if it has none.
  char short_name = 0;
};

// The fields every positional argument has.
struct CliArgFields {
  // The argument's name, without the surrounding "<" and ">".
  std::string_view name;

  // Whether the argument may be left out.
  bool optional = false;
};

// Parses `text` into `value`, returning an error saying what is wrong with it
// if it isn't of the type. A number must be only the number, with no
// surrounding space: base 10 for an integer, and finite for floating point,
// in the type's range. Any text is a string.
absl::Status ParseCliText(std::string_view text, std::string_view* value);
absl::Status ParseCliText(std::string_view text, int64_t* value);
absl::Status ParseCliText(std::string_view text, double* value);

// Returns the names in an enumeration's `names`.
template <typename T>
std::vector<std::string_view> GetCliNames(
    std::initializer_list<CliName<T>> names) {
  std::vector<std::string_view> result;
  result.reserve(names.size());
  for (const CliName<T>& name : names) {
    result.push_back(name.name);
  }
  return result;
}

// Returns the index of `text` in `names`, or an error listing them if it
// isn't one.
absl::StatusOr<int> FindCliName(absl::Span<const std::string_view> names,
                                std::string_view text);

// Validates an enumeration's `names`, returning an error worded as for
// ValidateCliValueFields().
absl::Status ValidateCliNames(absl::Span<const std::string_view> names);

template <typename T>
CliValue ToCliValue(const T& value) {
  return CliValue(static_cast<CliStoredType<T>>(value));
}

// `value` must hold the type for `T`.
template <typename T>
T FromCliValue(const CliValue& value) {
  return static_cast<T>(*std::get_if<CliStoredType<T>>(&value));
}

// Returns `definition`'s value parsed from `text` and validated, or an error
// saying what is wrong with it. `Definition` is CliFlag<T> or CliArg<T>.
template <typename T, typename Definition>
absl::StatusOr<CliValue> ParseCliValue(const void* definition,
                                       std::string_view text) {
  const auto* typed = static_cast<const Definition*>(definition);
  T value = {};
  absl::Status status;
  if constexpr (std::is_enum_v<T>) {
    const absl::StatusOr<int> index =
        FindCliName(GetCliNames(typed->names), text);
    if (!index.ok()) {
      return index.status();
    }
    value = typed->names.begin()[*index].value;
  } else {
    status = ParseCliText(text, &value);
    if (!status.ok()) {
      return status;
    }
  }
  for (const CliValidator<T> validator : typed->validators) {
    status = validator(value);
    if (!status.ok()) {
      return status;
    }
  }
  return ToCliValue(value);
}

// Validates the fields of `definition` that depend on its type, returning an
// error saying what is wrong, worded to follow the definition's name ("flag
// \"--mode\" ..."). `Definition` is CliFlag<T> or CliArg<T>.
template <typename T, typename Definition>
absl::Status ValidateCliValueFields([[maybe_unused]] const void* definition) {
  if constexpr (std::is_same_v<T, bool>) {
    const auto* typed = static_cast<const Definition*>(definition);
    if (!typed->value_name.empty()) {
      return absl::InvalidArgumentError(
          "is a bool, so can't have a value name");
    }
    if (typed->default_value.has_value()) {
      return absl::InvalidArgumentError(
          "is a bool, so can't have a default value");
    }
    if (typed->validators.size() != 0) {
      return absl::InvalidArgumentError("is a bool, so can't have validators");
    }
  } else if constexpr (std::is_enum_v<T>) {
    const auto* typed = static_cast<const Definition*>(definition);
    return ValidateCliNames(GetCliNames(typed->names));
  }
  return absl::OkStatus();
}

template <typename T>
CliFlagFields GetCliFlagFields(const void* flag) {
  const auto* typed = static_cast<const CliFlag<T>*>(flag);
  return {.name = typed->name, .short_name = typed->short_name};
}

template <typename T>
CliArgFields GetCliArgFields(const void* arg) {
  const auto* typed = static_cast<const CliArg<T>*>(arg);
  return {.name = typed->name, .optional = typed->optional};
}

// One flag in a command's list.
//
// This is implicitly constructed from a pointer to a CliFlag<T>, so commands
// list their flags as `{&kOutput, &kVerbose}`.
class CliFlagRef {
 public:
  template <typename T>
  constexpr CliFlagRef(const CliFlag<T>* flag)
      : definition_(flag),
        get_fields_(&GetCliFlagFields<T>),
        validate_(&ValidateCliValueFields<T, CliFlag<T>>) {
    if constexpr (!std::is_same_v<T, bool>) {
      parse_ = &ParseCliValue<T, CliFlag<T>>;
    }
  }

  // Returns the CliFlag this refers to.
  const void* GetDefinition() const { return definition_; }

  CliFlagFields GetFields() const { return get_fields_(definition_); }

  // See ValidateCliValueFields().
  absl::Status ValidateValueFields() const { return validate_(definition_); }

  // Returns whether the flag takes a value (it is not a CliFlag<bool>).
  bool TakesValue() const { return parse_ != nullptr; }

  // Returns the value parsed from `text`. The flag must take a value.
  absl::StatusOr<CliValue> Parse(std::string_view text) const {
    return parse_(definition_, text);
  }

 private:
  const void* definition_;
  CliFlagFields (*get_fields_)(const void* flag);
  absl::Status (*validate_)(const void* flag);
  absl::StatusOr<CliValue> (*parse_)(const void* flag,
                                     std::string_view text) = nullptr;
};

// One positional argument in a command's list.
//
// This is implicitly constructed from a pointer to a CliArg<T>, so commands
// list their arguments as `{&kInput, &kOutput}`.
class CliArgRef {
 public:
  template <typename T>
  constexpr CliArgRef(const CliArg<T>* arg)
      : definition_(arg),
        get_fields_(&GetCliArgFields<T>),
        validate_(&ValidateCliValueFields<T, CliArg<T>>),
        parse_(&ParseCliValue<T, CliArg<T>>) {}

  // Returns the CliArg this refers to.
  const void* GetDefinition() const { return definition_; }

  CliArgFields GetFields() const { return get_fields_(definition_); }

  // See ValidateCliValueFields().
  absl::Status ValidateValueFields() const { return validate_(definition_); }

  // Returns the value parsed from `text`.
  absl::StatusOr<CliValue> Parse(std::string_view text) const {
    return parse_(definition_, text);
  }

 private:
  const void* definition_;
  CliArgFields (*get_fields_)(const void* arg);
  absl::Status (*validate_)(const void* arg);
  absl::StatusOr<CliValue> (*parse_)(const void* arg, std::string_view text);
};

}  // namespace internal

// A command: the program itself, or one of its subcommands, named by a bare
// word on the command line (`program build`).
//
// A command has either positional arguments or subcommands, not both. Its
// flags are accepted anywhere after its name, including after its
// subcommands' names.
struct CliCommand {
  // The command's name. For the program, this is the name used in help and
  // errors (rather than `argv[0]`). This must not be empty or start with "-".
  std::string_view name;

  // Help for the command.
  std::string_view help;

  // The command's flags, CliFlag<T>. Their names must be unique among this
  // command's flags and those of the commands above it.
  std::initializer_list<internal::CliFlagRef> flags;

  // The command's positional arguments, CliArg<T>, in order.
  std::initializer_list<internal::CliArgRef> args;

  // The command's subcommands.
  std::initializer_list<const CliCommand*> commands;

  // Runs the command with the values given on the command line, returning
  // whether it succeeded. If a command with subcommands has no run function,
  // one of its subcommands must be named.
  bool (*run)(const CliValues& values) = nullptr;

  // Leave unset. This is here only to prevent the definition from being
  // constexpr.
  internal::CliNotConstexpr not_constexpr;
};

// Validates that `program` can be parsed unambiguously, returning an error
// describing the first problem found if it can't. ParseCommandLine() CHECKs
// this, as a definition that fails it is a programming error.
absl::Status ValidateCliCommand(const CliCommand* program);

}  // namespace gb

#endif  // GB_CLI_CLI_H_
