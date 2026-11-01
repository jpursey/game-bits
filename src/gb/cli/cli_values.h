// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_VALUES_H_
#define GB_CLI_CLI_VALUES_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gb/cli/cli.h"

namespace gb {

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

  // Returns the command the command line names, which is the one that runs.
  const CliCommand* GetCommand() const { return path_.back(); }

  // Returns the path from the program to the command the command line names.
  absl::Span<const CliCommand* const> GetCommandPath() const { return path_; }

  // Returns whether a bool flag was given.
  //
  // The flag must belong to a command in the command path (CHECK).
  bool Get(const CliFlag<bool>& flag) const;

  // Returns a flag's value if it was given, else its default value, else
  // std::nullopt.
  //
  // The flag must belong to a command in the command path (CHECK).
  template <typename T>
  std::optional<T> Get(const CliFlag<T>& flag) const;

  // Returns an argument's value if it was given, else its default value, else
  // std::nullopt. A required argument is always given.
  //
  // The argument must belong to the command that runs (CHECK).
  template <typename T>
  std::optional<T> Get(const CliArg<T>& arg) const;

 private:
  friend absl::StatusOr<CliValues> ParseCommandLine(
      const CliCommand* program, absl::Span<const std::string> args);

  CliValues(std::vector<const CliCommand*> path,
            absl::flat_hash_map<const void*, internal::CliValue> values)
      : path_(std::move(path)), values_(std::move(values)) {}

  // Returns the value given for `definition`, or null if none was. CHECKs that
  // `definition` belongs to the command path.
  const internal::CliValue* Find(const void* definition) const;

  // Returns whether `definition` belongs to a command in the command path.
  bool IsInPath(const void* definition) const;

  std::vector<const CliCommand*> path_;
  absl::flat_hash_map<const void*, internal::CliValue> values_;
};

// Parses `args`, the arguments after the program's name, as a command line for
// `program`.
//
// This returns an InvalidArgument error if the command line isn't valid, whose
// message says what is wrong in terms ready to show the user. `program` must be
// valid (see ValidateCliCommand(), which this CHECKs).
//
// String values point into `args`, so `args` must outlive the values.
absl::StatusOr<CliValues> ParseCommandLine(const CliCommand* program,
                                           absl::Span<const std::string> args);

//==============================================================================
// Template implementation
//==============================================================================

template <typename T>
std::optional<T> CliValues::Get(const CliFlag<T>& flag) const {
  const internal::CliValue* value = Find(&flag);
  if (value == nullptr) {
    return flag.default_value;
  }
  return internal::FromCliValue<T>(*value);
}

template <typename T>
std::optional<T> CliValues::Get(const CliArg<T>& arg) const {
  const internal::CliValue* value = Find(&arg);
  if (value == nullptr) {
    return arg.default_value;
  }
  return internal::FromCliValue<T>(*value);
}

}  // namespace gb

#endif  // GB_CLI_CLI_VALUES_H_
