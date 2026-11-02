// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_validators.h"

namespace gb {

namespace {

absl::Status ValidateNotEmpty(const ValidatedContext&,
                              const std::string_view& value) {
  if (value.empty()) {
    return absl::InvalidArgumentError("is empty");
  }
  return absl::OkStatus();
}

}  // namespace

const CliValidator<std::string_view> kCliNotEmpty = {
    .validate = ValidateNotEmpty,
};

}  // namespace gb
