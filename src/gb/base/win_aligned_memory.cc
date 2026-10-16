// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <malloc.h>

#include "gb/base/aligned_memory.h"

namespace gb::internal {

void* AllocAligned(size_t size, size_t align) {
  return _aligned_malloc(size, align);
}

void FreeAligned(void* ptr) { _aligned_free(ptr); }

}  // namespace gb::internal
