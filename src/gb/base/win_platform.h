// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_WIN_PLATFORM_H_
#define GB_BASE_WIN_PLATFORM_H_

#include <cstdint>
#include <string>

namespace gb {

// Simple wrapper around GetLastError() that can be used in headers.
uint32_t GetLastWindowsError();

// This converts the windows error code to a human readable string.
std::string GetWindowsError(uint32_t error_code = GetLastWindowsError());

}  // namespace gb

#endif  // GB_BASE_WIN_PLATFORM_H_
