// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <cstdlib>
#include <limits>

#include "gb/base/aligned_memory.h"

namespace gb::internal {

void* AllocAligned(size_t size, size_t align) {
  // std::aligned_alloc() needs the size to be a multiple of the alignment, and
  // rounding up must not wrap around.
  if (size > std::numeric_limits<size_t>::max() - (align - 1)) {
    return nullptr;
  }
  return std::aligned_alloc(align, (size + align - 1) & ~(align - 1));
}

void FreeAligned(void* ptr) { std::free(ptr); }

}  // namespace gb::internal
