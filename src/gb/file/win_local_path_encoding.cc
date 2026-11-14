// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/unicode.h"
#include "gb/file/local_path_encoding.h"

namespace gb::internal {

namespace fs = std::filesystem;

// Windows paths are UTF-16.

std::string LocalPathToUtf8(const fs::path& path) {
  return ToUtf8(path.generic_u16string());
}

fs::path Utf8ToLocalPath(std::string_view path) {
  return fs::path(ToUtf16(path), fs::path::generic_format);
}

}  // namespace gb::internal
