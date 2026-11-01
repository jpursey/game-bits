// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_internal.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace gb::internal {

std::string GetCliPathName(absl::Span<const CliCommand* const> path) {
  return absl::StrJoin(path, " ",
                       [](std::string* out, const CliCommand* command) {
                         absl::StrAppend(out, command->name);
                       });
}

}  // namespace gb::internal
