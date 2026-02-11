// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/config/text_config.h"

namespace gb {

std::string WriteConfigToText(const Config& config, TextConfigFlags flags) {
  return "";
}

absl::StatusOr<Config> ReadConfigFromText(std::string_view text,
                                          TextConfigFlags flags) {
  return absl::UnimplementedError("ReadConfigFromText is not implemented");
}

}  // namespace gb
