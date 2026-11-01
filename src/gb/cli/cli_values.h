// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_VALUES_H_
#define GB_CLI_CLI_VALUES_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gb/cli/cli.h"

namespace gb {

namespace internal {

// A value given on the command line. A bool flag that is given is true.
using CliValue = std::variant<bool, std::string_view, int64_t, double>;

}  // namespace internal

//==============================================================================
// CliValues
//==============================================================================

// The values given on a command line, for the command it names. These are made
// by ParseCommandLine().
//
// String values point into the arguments that were parsed, so the arguments
// must outlive these values.
class CliValues {
 public:
  // Returns the program: the root of the command tree.
  const CliCommand* GetProgram() const { return path_.front(); }

  // Returns the command the command line names, which is the one that runs, or
  // the one help is for.
  const CliCommand* GetCommand() const { return path_.back(); }

  // Returns the path from the program to the command the command line names.
  absl::Span<const CliCommand* const> GetCommandPath() const { return path_; }

  // Runs the command with these values, returning whether it succeeded.
  //
  // If the command line asks for help, with "--help", "-h", or the help command
  // (see cli_help.h), this writes the command's help (FormatCliHelp()) to
  // `write_help` instead, and succeeds. It does the same for a command with no
  // run function.
  bool Run(absl::FunctionRef<void(std::string_view help)> write_help) const;

  // Returns whether a bool flag was given.
  //
  // The flag must belong to a command in the command path, and the command
  // line must not ask for help (CHECK).
  bool Get(const CliFlag<bool>& flag) const;

  // Returns a flag's value if it was given, else its default value, else
  // std::nullopt.
  //
  // The flag must belong to a command in the command path, and the command
  // line must not ask for help (CHECK).
  template <typename T>
  std::optional<T> Get(const CliFlag<T>& flag) const;

  // Returns an argument's value if it was given, else its default value, else
  // std::nullopt. A required argument is always given.
  //
  // The argument must belong to the command that runs, and the command line
  // must not ask for help (CHECK).
  template <typename T>
  std::optional<T> Get(const CliArg<T>& arg) const;

 private:
  friend absl::StatusOr<CliValues> ParseCommandLine(
      const CliCommand* program, absl::Span<const std::string> args);

  CliValues(std::vector<const CliCommand*> path,
            absl::flat_hash_map<const void*, internal::CliValue> values,
            bool is_help)
      : path_(std::move(path)), values_(std::move(values)), is_help_(is_help) {}

  // Returns the value given for `definition`, or null if none was. CHECKs that
  // `definition` belongs to the command path, and that this isn't help.
  const internal::CliValue* Find(const void* definition) const;

  // Returns whether `definition` belongs to a command in the command path.
  bool IsInPath(const void* definition) const;

  std::vector<const CliCommand*> path_;
  absl::flat_hash_map<const void*, internal::CliValue> values_;

  // Whether the command line asks for help. Parsing stops at "--help" or "-h",
  // so values may be missing, including required arguments.
  bool is_help_;
};

// Parses `args`, the arguments after the program's name, as a command line for
// `program`.
//
// This returns an InvalidArgument error if the command line isn't valid, whose
// message says what is wrong in terms ready to show the user. `program` must be
// valid (see ValidateCliCommand(), which this CHECKs).
//
// A command line that is valid may ask for help rather than for the command to
// run, so pass the values to CliValues::Run(), which handles both, rather than
// reading them directly.
//
// String values point into `args`, so `args` must outlive the values.
absl::StatusOr<CliValues> ParseCommandLine(const CliCommand* program,
                                           absl::Span<const std::string> args);

}  // namespace gb

#endif  // GB_CLI_CLI_VALUES_H_
