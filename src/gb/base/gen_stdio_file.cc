// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <limits>

#include "gb/base/stdio_file.h"

namespace gb {

std::FILE* OpenStdioFile(const std::filesystem::path& path, const char* mode) {
  // This assumes the platform's paths are UTF-8.
  return std::fopen(reinterpret_cast<const char*>(path.u8string().c_str()),
                    mode);
}

bool SeekStdioFile(std::FILE* file, int64_t offset, int origin) {
  if (offset < std::numeric_limits<long>::min() ||
      offset > std::numeric_limits<long>::max()) {
    return false;
  }
  return std::fseek(file, static_cast<long>(offset), origin) == 0;
}

int64_t TellStdioFile(std::FILE* file) { return std::ftell(file); }

}  // namespace gb
