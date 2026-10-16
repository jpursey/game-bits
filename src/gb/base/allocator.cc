// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/allocator.h"

#include <cstddef>

#include "absl/log/log.h"
#include "gb/base/aligned_memory.h"

namespace gb {

namespace {

Allocator* DefaultAllocator(Allocator* allocator) {
  static Allocator* default_allocator = allocator;
  return default_allocator;
}

class SystemAllocator : public Allocator {
 public:
  ~SystemAllocator() override = default;

  void* Alloc(size_t size, size_t align) override {
    if (align == 0) {
      align = alignof(std::max_align_t);
    }
    return internal::AllocAligned(size, align);
  }

  void Free(void* ptr) override { internal::FreeAligned(ptr); }
};

}  // namespace

Allocator* GetSystemAllocator() noexcept {
  return GlobalAllocatorTraits<SystemAllocator>::Get();
}

void SetDefaultAllocator(Allocator* allocator) noexcept {
  Allocator* default_allocator = DefaultAllocator(allocator);
  LOG_IF(ERROR, default_allocator != allocator)
      << "SetDefaultAllocator failed because GetDefaultAllocator was already "
         "called.";
}

Allocator* GetDefaultAllocator() noexcept {
  static Allocator* default_allocator = DefaultAllocator(GetSystemAllocator());
  return default_allocator;
}

}  // namespace gb
