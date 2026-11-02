// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_FILE_LOCAL_CURRENT_PATH_H_
#define GB_FILE_LOCAL_CURRENT_PATH_H_

#include <filesystem>

namespace gb::internal {

// Sets the process's current folder to `path`, returning false if it can't.
//
// Where the operating system keeps a current folder for each drive, this also
// sets it for the drive of `path`, as a command shell does, so it is still the
// drive's current folder once another drive's is set.
bool SetLocalCurrentPath(const std::filesystem::path& path);

}  // namespace gb::internal

#endif  // GB_FILE_LOCAL_CURRENT_PATH_H_
