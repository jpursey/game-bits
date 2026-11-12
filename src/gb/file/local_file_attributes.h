// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_LOCAL_FILE_ATTRIBUTES_H_
#define GB_FILE_LOCAL_FILE_ATTRIBUTES_H_

#include <filesystem>

#include "gb/file/file_types.h"

namespace gb::internal {

// Returns the attributes local files support on this operating system.
FileAttributes GetLocalAttributeFlags();

// Returns the attributes of the file or folder at `path`, following links, or
// an empty set if they can't be read.
FileAttributes GetLocalAttributes(const std::filesystem::path& path);

// Sets the attributes of the file or folder at `path`, following links, to
// `attributes`, which must all be in GetLocalAttributeFlags(). Returns false if
// it can't.
bool SetLocalAttributes(const std::filesystem::path& path,
                        FileAttributes attributes);

}  // namespace gb::internal

#endif  // GB_FILE_LOCAL_FILE_ATTRIBUTES_H_
