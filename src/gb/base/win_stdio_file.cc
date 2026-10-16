// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <share.h>

#include <cstring>
#include <string>

#include "gb/base/stdio_file.h"

namespace gb {

std::FILE* OpenStdioFile(const std::filesystem::path& path, const char* mode) {
  // Unlike _wfopen_s(), this lets other programs read and write the file while
  // it is open.
  const std::wstring wide_mode(mode, mode + std::strlen(mode));
  return _wfsopen(path.c_str(), wide_mode.c_str(), _SH_DENYNO);
}

bool SeekStdioFile(std::FILE* file, int64_t offset, int origin) {
  return _fseeki64(file, offset, origin) == 0;
}

int64_t TellStdioFile(std::FILE* file) { return _ftelli64(file); }

}  // namespace gb
