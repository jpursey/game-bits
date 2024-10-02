#include "gb/alloc/test_allocator.h"

#include <cstddef>

#include "absl/log/check.h"
#include "absl/log/log.h"

namespace gb {

TestAllocator::~TestAllocator() { CHECK(allocs_.empty()); }

void* TestAllocator::Alloc(size_t size, size_t align) {
  CHECK(size > 0);
  if (fail_next_alloc_) {
    fail_next_alloc_ = false;
    return nullptr;
  }
  void* alloc = GetSystemAllocator()->Alloc(size, align);
  CHECK(alloc != nullptr);
  allocs_[alloc] = {size, align};
  total_size_ += size;
  return alloc;
}

void TestAllocator::Free(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  auto it = allocs_.find(ptr);
  CHECK(it != allocs_.end());
  total_size_ -= it->second.size;
  allocs_.erase(it);
  GetSystemAllocator()->Free(ptr);
}

bool TestAllocator::IsValidMemory(void* ptr, size_t size, size_t align) const {
  uintptr_t ptr_address = reinterpret_cast<uintptr_t>(ptr);
  if ((ptr_address & (align - 1)) != 0) {
    LOG(ERROR) << "Pointer address " << ptr_address << " is not aligned to "
               << align;
    return false;
  }
  for (const auto& [alloc_ptr, alloc_info] : allocs_) {
    auto alloc_address = reinterpret_cast<uintptr_t>(ptr);
    if (ptr_address >= alloc_address &&
        ptr_address + size <= alloc_address + alloc_info.size) {
      return true;
    }
  }
  LOG(ERROR) << "Memory at address " << ptr_address << " of size " << size
             << " is not wholly in allocated memory";
  return false;
}

}  // namespace gb
