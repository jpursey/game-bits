// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/alloc/pool_allocator.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "absl/log/check.h"

namespace gb {

namespace {

size_t GetDefaultAlignment(size_t size) {
  size_t align = alignof(std::max_align_t);
  while (align > 1) {
    if ((size & (align - 1)) == 0) {
      return align;
    }
    align >>= 1;
  }
  return align;
}

// Every slot must be aligned for its allocation, and for the FreeNode (a
// pointer) it holds while it is free.
size_t GetSlotAlignment(size_t alloc_size, size_t alloc_align) {
  if (alloc_align == 0) {
    alloc_align = GetDefaultAlignment(alloc_size);
  }
  return std::max(alloc_align, alignof(void*));
}

// Slots are a whole number of alignments, so every slot in a bucket is aligned.
size_t GetSlotSize(size_t alloc_size, size_t slot_align) {
  const size_t size = std::max(alloc_size, sizeof(void*));
  return (size + slot_align - 1) & ~(slot_align - 1);
}

}  // namespace

PoolAllocator::PoolAllocator(Allocator* bucket_allocator, size_t bucket_size,
                             size_t alloc_size, size_t alloc_align)
    : bucket_allocator_(bucket_allocator),
      bucket_size_(bucket_size),
      alloc_align_(GetSlotAlignment(alloc_size, alloc_align)),
      alloc_size_(GetSlotSize(alloc_size, alloc_align_)) {
  DCHECK(bucket_size_ > 0);
  DCHECK(alloc_size_ > 0);
  DCHECK((alloc_align_ & (alloc_align_ - 1)) == 0)
      << "Alignment must be a power of two";
}

PoolAllocator::~PoolAllocator() {
  while (buckets_ != nullptr) {
    bucket_allocator_->Free(std::exchange(buckets_, buckets_->next));
  }
}

void* PoolAllocator::Alloc(size_t size, size_t align) {
  if (size == 0 || size > alloc_size_ || align > alloc_align_) {
    return nullptr;
  }
  void* alloc = free_;
  if (alloc != nullptr) {
    free_ = free_->next;
    return alloc;
  }

  const size_t bucket_header_size = std::max(sizeof(Bucket), alloc_align_);

  if (unused_ == 0) {
    Bucket* bucket = static_cast<Bucket*>(bucket_allocator_->Alloc(
        bucket_header_size + bucket_size_ * alloc_size_,
        std::max(alignof(Bucket), alloc_align_)));
    if (bucket == nullptr) {
      return nullptr;
    }
    bucket->next = buckets_;
    buckets_ = bucket;
    unused_ = bucket_size_;
  }

  alloc = reinterpret_cast<std::byte*>(buckets_) + bucket_header_size +
          (bucket_size_ - unused_) * alloc_size_;
  --unused_;
  return alloc;
}

void PoolAllocator::Free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  FreeNode* node = static_cast<FreeNode*>(ptr);
  node->next = std::exchange(free_, node);
}

}  // namespace gb
