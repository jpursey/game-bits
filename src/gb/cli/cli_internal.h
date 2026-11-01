// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_INTERNAL_H_
#define GB_CLI_CLI_INTERNAL_H_

#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_values.h"

namespace gb::internal {

//==============================================================================
// Parts of gb_cli shared by its source files. This is private to gb_cli and its
// tests.
//==============================================================================

// The help flag, "--help" or "-h", which every command has. A program's flags
// can't use its names.
extern const CliFlag<bool> kCliHelpFlag;

// The fields every flag has, whatever its value type.
struct CliFlagFields {
  // The CliFlag<T>, which identifies the flag.
  const void* definition = nullptr;

  // The flag's long name, without the leading "--".
  std::string_view name;

  // The flag's short name, without the leading "-", or 0 if it has none.
  char short_name = 0;
};

// The fields every positional argument has, whatever its value type.
struct CliArgFields {
  // The CliArg<T>, which identifies the argument.
  const void* definition = nullptr;

  // The argument's name, without the surrounding "<" and ">".
  std::string_view name;

  // Whether the argument may be left out.
  bool optional = false;
};

CliFlagFields GetCliFlagFields(const CliAnyFlag& flag);
CliArgFields GetCliArgFields(const CliAnyArg& arg);

// Returns the value parsed from `text` and validated, or an error saying what
// is wrong with it. A bool flag takes no value, so `text` is ignored, and its
// value is true.
absl::StatusOr<CliValue> ParseCliValue(const CliAnyFlag& flag,
                                       std::string_view text);
absl::StatusOr<CliValue> ParseCliValue(const CliAnyArg& arg,
                                       std::string_view text);

// Returns the names of the commands in `path`, separated by spaces, as used to
// say which command an error is for: "program build".
std::string GetCliPathName(absl::Span<const CliCommand* const> path);

}  // namespace gb::internal

#endif  // GB_CLI_CLI_INTERNAL_H_
