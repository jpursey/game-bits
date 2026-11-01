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

#include "absl/status/status.h"

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
// A flag's or argument's value is a std::string_view, int64_t, or double, and a
// flag may also be a bool.
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

// One value a string flag or argument accepts, when it lists its choices.
struct CliChoice {
  // The value, as given on the command line. This must not be empty or start
  // with "-".
  std::string_view name;

  // Help for the value.
  std::string_view help;
};

// A flag: `--name value`, `--name=value`, or `-n value`.
//
// A CliFlag<bool> takes no value: it is given as `--name` or `-n`, which makes
// it true, and is otherwise false. It must leave `value_name`, `choices`,
// `default_value`, and `validators` unset.
template <typename T>
struct CliFlag {
  static_assert(std::is_same_v<T, bool> ||
                    std::is_same_v<T, std::string_view> ||
                    std::is_same_v<T, int64_t> || std::is_same_v<T, double>,
                "A flag's value is a bool, std::string_view, int64_t, or "
                "double");

  // The flag's long name, without the leading "--". This must not be empty,
  // start with "-", or contain "=", and must not be "help", which every
  // command has.
  std::string_view name;

  // The flag's short name, without the leading "-", or 0 if it has none. This
  // must be a visible ASCII character other than "-", ".", or a digit, as a
  // word starting with "-" then a digit or "." is a negative number, and must
  // not be "h", which every command has for help.
  char short_name = 0;

  // What the flag's value is, as shown in help: "--output <file>". If this is
  // empty, the flag's name is used instead.
  std::string_view value_name;

  // Help for the flag.
  std::string_view help;

  // The values a string flag accepts, if it only accepts some. Each must be
  // unique. Only a string flag can have choices.
  std::initializer_list<CliChoice> choices;

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
  static_assert(std::is_same_v<T, std::string_view> ||
                    std::is_same_v<T, int64_t> || std::is_same_v<T, double>,
                "An argument's value is a std::string_view, int64_t, or "
                "double (use a CliFlag<bool> for a bool)");

  // The argument's name, as shown in help and errors: "<input file>". This
  // must not be empty.
  std::string_view name;

  // Help for the argument.
  std::string_view help;

  // Whether the argument may be left out. Optional arguments must follow all
  // required ones in a command.
  bool optional = false;

  // The values a string argument accepts, if it only accepts some. Each must
  // be unique. Only a string argument can have choices.
  std::initializer_list<CliChoice> choices;

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

// A flag of any value type, as a command lists it: `{&kOutput, &kVerbose}`.
using CliAnyFlag =
    std::variant<const CliFlag<bool>*, const CliFlag<std::string_view>*,
                 const CliFlag<int64_t>*, const CliFlag<double>*>;

// A positional argument of any value type, as a command lists it:
// `{&kInput, &kCount}`.
using CliAnyArg = std::variant<const CliArg<std::string_view>*,
                               const CliArg<int64_t>*, const CliArg<double>*>;

// A command: the program itself, or one of its subcommands, named by a bare
// word on the command line (`program build`).
//
// A command has either positional arguments or subcommands, not both. Its
// flags are accepted anywhere after its name, including after its
// subcommands' names. Every command also takes "--help" and "-h", which ask for
// its help (see CliValues::Run()).
struct CliCommand {
  // The command's name. For the program, this is the name used in help and
  // errors (rather than `argv[0]`). This must not be empty or start with "-".
  std::string_view name;

  // Help for the command.
  std::string_view help;

  // The command's flags. Their names must be unique among this command's flags
  // and those of the commands above it.
  std::initializer_list<CliAnyFlag> flags;

  // The command's positional arguments, in order.
  std::initializer_list<CliAnyArg> args;

  // The command's subcommands.
  std::initializer_list<const CliCommand*> commands;

  // Runs the command with the values given on the command line, returning
  // whether it succeeded. A command with no run function writes its help
  // instead (see CliValues::Run()), so one with subcommands but no run
  // function writes its help if none of them is named.
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
