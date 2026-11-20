// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/process_memory.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Must be after windows.h.
#include <psapi.h>

namespace gb {

std::optional<int64_t> GetProcessMemoryBytes() {
  PROCESS_MEMORY_COUNTERS_EX counters = {};
  if (!K32GetProcessMemoryInfo(
          GetCurrentProcess(),
          reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
          sizeof(counters))) {
    return std::nullopt;
  }
  return static_cast<int64_t>(counters.PrivateUsage);
}

}  // namespace gb
