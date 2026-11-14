// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/unicode.h"
#include "gb/file/local_path_encoding.h"

namespace gb::internal {

namespace fs = std::filesystem;

namespace {

bool IsValidUtf8(std::string_view string) {
  switch (GetStringEncoding(string)) {
    case StringEncoding::kAscii:
    case StringEncoding::kUtf8:
    case StringEncoding::kUtf8WithBom:
      return true;
    default:
      return false;
  }
}

}  // namespace

// Paths are bytes, which are taken as UTF-8.

std::string LocalPathToUtf8(const fs::path& path) {
  std::string string = path.generic_string();
  if (!IsValidUtf8(string)) {
    return {};
  }
  return string;
}

fs::path Utf8ToLocalPath(std::string_view path) {
  if (!IsValidUtf8(path)) {
    return {};
  }
  return fs::path(path, fs::path::generic_format);
}

}  // namespace gb::internal
