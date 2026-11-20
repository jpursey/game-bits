// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_PROCESS_MEMORY_H_
#define GB_BASE_PROCESS_MEMORY_H_

#include <cstdint>
#include <optional>

namespace gb {

// Returns the memory the process has committed for its own use, in bytes, or
// nothing where the platform can't report it.
//
// On Windows this is the process's private bytes. Linux commits memory only as
// it is first written, so there it is the anonymous memory the process has
// written, whether resident or swapped out.
std::optional<int64_t> GetProcessMemoryBytes();

}  // namespace gb

#endif  // GB_BASE_PROCESS_MEMORY_H_
