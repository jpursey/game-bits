// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_ALIGNED_MEMORY_H_
#define GB_BASE_ALIGNED_MEMORY_H_

#include <cstddef>

namespace gb::internal {

//==============================================================================
// This file defines a low level platform independent interface to aligned
// allocation from the system heap, which not every platform provides through
// std::aligned_alloc().
//==============================================================================

// Allocates `size` bytes aligned to `align`, a power of two. Returns null if
// it can't be allocated. Free it with FreeAligned().
void* AllocAligned(size_t size, size_t align);

// Frees memory returned by AllocAligned(). `ptr` may be null.
void FreeAligned(void* ptr);

}  // namespace gb::internal

#endif  // GB_BASE_ALIGNED_MEMORY_H_
