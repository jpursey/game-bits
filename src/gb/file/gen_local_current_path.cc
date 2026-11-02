// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <system_error>

#include "gb/file/local_current_path.h"

namespace gb::internal {

bool SetLocalCurrentPath(const std::filesystem::path& path) {
  std::error_code error;
  std::filesystem::current_path(path, error);
  return !error;
}

}  // namespace gb::internal
