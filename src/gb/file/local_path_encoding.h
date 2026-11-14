// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_LOCAL_PATH_ENCODING_H_
#define GB_FILE_LOCAL_PATH_ENCODING_H_

#include <filesystem>
#include <string>
#include <string_view>

namespace gb::internal {

// Converts between UTF-8 paths and std::filesystem paths, in the encoding the
// operating system's paths use. std::filesystem's own conversions throw for a
// string that isn't valid, so these are used instead.

// Returns `path` in generic format (with '/' separators) as UTF-8, or an empty
// string if it isn't valid in the operating system's encoding.
std::string LocalPathToUtf8(const std::filesystem::path& path);

// Returns the UTF-8 `path` as a std::filesystem path, read in generic format,
// or an empty path if `path` isn't valid UTF-8.
std::filesystem::path Utf8ToLocalPath(std::string_view path);

}  // namespace gb::internal

#endif  // GB_FILE_LOCAL_PATH_ENCODING_H_
