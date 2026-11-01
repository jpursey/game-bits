// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_CLI_CLI_INTERNAL_H_
#define GB_CLI_CLI_INTERNAL_H_

#include <string>

#include "absl/types/span.h"
#include "gb/cli/cli.h"

namespace gb::internal {

//==============================================================================
// Parts of gb_cli shared by its source files. This is private to gb_cli and its
// tests.
//==============================================================================

// Returns the names of the commands in `path`, separated by spaces, as used to
// say which command an error is for: "program build".
std::string GetCliPathName(absl::Span<const CliCommand* const> path);

}  // namespace gb::internal

#endif  // GB_CLI_CLI_INTERNAL_H_
