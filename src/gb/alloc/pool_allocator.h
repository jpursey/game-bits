// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_ALLOC_POOL_ALLOCATOR_H_
#define GB_ALLOC_POOL_ALLOCATOR_H_

#include <cstddef>

#include "gb/base/allocator.h"

namespace gb {

// This allocator is implemented by managing a pool of fixed size allocations.
//
// Allocations are grouped into "buckets" which are allocated out of a separate
// bucket allocator. Allocations from a pool allocator are much faster than the
// system allocator or other general purpose allocator (especially with larger
// bucket sizes).
//
// This class is thread-compatible. Use the TsPoolAllocator alias for a
// thread-safe variant.
class PoolAllocator : public Allocator {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a pool allocator with the specified bucket size (number of pool
  // allocations per bucket) and the individual allocation size and alignment.
  // Buckets are allocated from the default allocator.
  PoolAllocator(size_t bucket_size, size_t alloc_size, size_t alloc_align = 0);

  // Creates a pool allocator that allocates its blocks from the specified
  // bucket allocator. The bucket allocator must outlive this allocator.
  PoolAllocator(Allocator* bucket_allocator, size_t bucket_size,
                size_t alloc_size, size_t alloc_align = 0);

  PoolAllocator(const PoolAllocator&) = delete;
  PoolAllocator(PoolAllocator&&) = delete;
  PoolAllocator& operator=(const PoolAllocator&) = delete;
  PoolAllocator& operator=(PoolAllocator&&) = delete;
  ~PoolAllocator() override;

  //----------------------------------------------------------------------------
  // Allocator overrides
  //----------------------------------------------------------------------------

  void* Alloc(size_t size, size_t align) override;
  void Free(void* ptr) override;

 private:
  struct FreeNode {
    FreeNode* next;
  };
  struct Bucket {
    Bucket* next;
  };

  Allocator* const bucket_allocator_;
  const size_t bucket_size_;
  const size_t alloc_size_;
  const size_t alloc_align_;
  Bucket* buckets_ = nullptr;
  FreeNode* free_ = nullptr;
  size_t unused_ = 0;
};

inline PoolAllocator::PoolAllocator(size_t bucket_size, size_t alloc_size,
                                    size_t alloc_align)
    : PoolAllocator(GetDefaultAllocator(), bucket_size, alloc_size,
                    alloc_align) {}

// Thread-safe variant of PoolAllocator.
using TsPoolAllocator = TsAllocator<PoolAllocator>;

}  // namespace gb

#endif  // GB_ALLOC_POOL_ALLOCATOR_H_
