// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <string>

#include "gb/file/platform_file.h"

namespace gb::internal {

// Names are bytes that aren't checked, so a byte that can't start a UTF-8
// character is accepted.
std::filesystem::path GetNonUtf8Name() {
  return std::filesystem::path(std::string(1, '\xFF'));
}

}  // namespace gb::internal
