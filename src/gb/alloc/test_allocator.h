// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_ALLOC_TEST_ALLOCATOR_H_
#define GB_ALLOC_TEST_ALLOCATOR_H_

#include "absl/container/flat_hash_map.h"
#include "gb/base/allocator.h"

namespace gb {

// The test allocator tracks and validated every allocation call made for use in
// allocator tests.
//
// The class uses CHECK to validate all access are valid.
class TestAllocator : public Allocator {
 public:
  TestAllocator() = default;
  TestAllocator(const TestAllocator&) = delete;
  TestAllocator(TestAllocator&&) = delete;
  TestAllocator& operator=(const TestAllocator&) = delete;
  TestAllocator& operator=(TestAllocator&&) = delete;
  ~TestAllocator();

  void FailNextAlloc() { fail_next_alloc_ = true; }

  void* Alloc(size_t size, size_t align) override;
  void Free(void* ptr) override;

  size_t GetTotalAllocSize() const { return total_size_; }
  int GetAllocCount() const { return static_cast<int>(allocs_.size()); }
  bool IsValidMemory(void* ptr, size_t size, size_t align) const;

 private:
  struct AllocInfo {
    size_t size;
    size_t align;
  };
  size_t total_size_ = 0;
  bool fail_next_alloc_ = false;
  absl::flat_hash_map<void*, AllocInfo> allocs_;
};

}  // namespace gb

#endif  // GB_ALLOC_TEST_ALLOCATOR_H_
