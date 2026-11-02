// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <direct.h>

#include "gb/file/local_current_path.h"

namespace gb::internal {

bool SetLocalCurrentPath(const std::filesystem::path& path) {
  // Unlike SetCurrentDirectoryW(), this also sets the drive's current folder
  // (the "=C:" environment variable), which a drive on its own ("c:")
  // resolves to while another drive is current.
  return _wchdir(path.c_str()) == 0;
}

}  // namespace gb::internal
