// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/allocator.h"

#ifdef _WIN32
#include <malloc.h>
#endif  // _WIN32

#include <cstdlib>

#include "glog/logging.h"

namespace gb {

namespace {

Allocator* DefaultAllocator(Allocator* allocator) {
  static Allocator* default_allocator = allocator;
  return allocator;
}

class SystemAllocator : public Allocator {
 public:
  ~SystemAllocator() override = default;

  void* Alloc(size_t size, size_t align) override {
    if (align == 0) {
      align = alignof(std::max_align_t);
    }
#ifdef _WIN32
    return _aligned_malloc(size, align);
#else
    return std::aligned_alloc(align, size);
#endif
  }

  void Free(void* ptr) override {
#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
  }
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
