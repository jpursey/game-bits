// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/win_platform.h"

#include "absl/strings/str_cat.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb {

static_assert(sizeof(uint32_t) >= sizeof(DWORD));

uint32_t GetLastWindowsError() { return GetLastError(); }

std::string GetWindowsError(uint32_t error_code) {
  char* message_buffer = nullptr;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                 nullptr, static_cast<DWORD>(error_code), 0,
                 reinterpret_cast<char*>(&message_buffer), 0, nullptr);
  std::string message;
  if (message_buffer != nullptr) {
    message = absl::StrCat(message_buffer, " (", error_code, ")");
    LocalFree(message_buffer);
  } else {
    message = absl::StrCat("no message (", error_code, ")");
  }
  return message;
}

}  // namespace gb
