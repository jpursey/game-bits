// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <string>

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

// Names are UTF-16 code units that aren't checked, so a lone surrogate is
// accepted.
fs::path GetNonUtf8Name() { return fs::path(std::wstring(1, wchar_t{0xD800})); }

}  // namespace gb::internal
