// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_STDIO_FILE_H_
#define GB_BASE_STDIO_FILE_H_

#include <cstdint>
#include <cstdio>
#include <filesystem>

namespace gb {

//==============================================================================
// This file defines a low level platform independent interface to C stdio
// files, for what the C standard library leaves to each platform: opening a
// file by a Unicode path, and seeking with 64-bit positions.
//==============================================================================

// Opens the file at `path` with a C stdio `mode`, as std::fopen() takes it,
// letting other programs read and write the file while it is open. Returns
// null if it can't be opened. Close the file with std::fclose().
std::FILE* OpenStdioFile(const std::filesystem::path& path, const char* mode);

// Seeks `file` to `offset` from `origin` (SEEK_SET, SEEK_CUR, or SEEK_END), as
// std::fseek() does. Returns false if the seek fails, which it does for an
// offset beyond what the platform's files support.
bool SeekStdioFile(std::FILE* file, int64_t offset, int origin);

// Returns the position of `file`, as std::ftell() does, or -1 on error.
int64_t TellStdioFile(std::FILE* file);

}  // namespace gb

#endif  // GB_BASE_STDIO_FILE_H_
