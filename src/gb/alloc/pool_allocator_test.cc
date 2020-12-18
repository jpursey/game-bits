// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/alloc/pool_allocator.h"

#include "absl/container/flat_hash_set.h"
#include "gb/alloc/test_allocator.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(PoolAllocatorTest, EmptyPoolAllocatorDoesNotAllocate) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 100, sizeof(int));
  EXPECT_EQ(heap.GetAllocCount(), 0);
}

TEST(PoolAllocatorTest, Alloc) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 100, sizeof(int));
  void* ptr = allocator.Alloc(sizeof(int), 0);
  EXPECT_EQ(heap.GetAllocCount(), 1);
  EXPECT_GE(heap.GetTotalAllocSize(), 100 * sizeof(int));
  EXPECT_TRUE(heap.IsValidMemory(ptr, sizeof(int), alignof(int)));
}

TEST(PoolAllocatorTest, AlignLargerThanSize) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 100, sizeof(int), 1024);
  void* ptr = allocator.Alloc(sizeof(int), 0);
  EXPECT_EQ(heap.GetAllocCount(), 1);
  EXPECT_GE(heap.GetTotalAllocSize(), 100 * sizeof(int));
  EXPECT_TRUE(heap.IsValidMemory(ptr, sizeof(int), 1024));
}

TEST(PoolAllocatorTest, FreeNull) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 100, sizeof(int));
  allocator.Free(nullptr);
  EXPECT_EQ(heap.GetAllocCount(), 0);
}

TEST(PoolAllocatorTest, FreeAndRealloc) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 1, sizeof(int));
  void* ptr_1 = allocator.Alloc(sizeof(int), 0);
  allocator.Free(ptr_1);
  void* ptr_2 = allocator.Alloc(sizeof(int), 0);
  EXPECT_EQ(ptr_1, ptr_2);
  EXPECT_EQ(heap.GetAllocCount(), 1);
}

TEST(PoolAllocatorTest, MultipleFreeAndRealloc) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 100, sizeof(int));
  for (int i = 0; i < 10; ++i) {
    absl::flat_hash_set<void*> ptrs;
    for (int k = 0; k < 100; ++k) {
      void* ptr = allocator.Alloc(sizeof(int), 0);
      ptrs.insert(ptr);
      EXPECT_TRUE(heap.IsValidMemory(ptr, sizeof(int), alignof(int)));
    }
    for (int k = 0; k < 50; ++k) {
      allocator.Free(*ptrs.begin());
      ptrs.erase(ptrs.begin());
    }
    for (int k = 0; k < 50; ++k) {
      void* ptr = allocator.Alloc(sizeof(int), 0);
      ptrs.insert(ptr);
      EXPECT_TRUE(heap.IsValidMemory(ptr, sizeof(int), alignof(int)));
    }
    while (!ptrs.empty()) {
      allocator.Free(*ptrs.begin());
      ptrs.erase(ptrs.begin());
    }
  }
  EXPECT_EQ(heap.GetAllocCount(), 1);
}

TEST(PoolAllocatorTest, AddBucket) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 10, sizeof(int));
  for (int i = 0; i < 100; ++i) {
    void* ptr = allocator.Alloc(sizeof(int), 0);
    EXPECT_TRUE(heap.IsValidMemory(ptr, sizeof(int), alignof(int)));
    EXPECT_EQ(heap.GetAllocCount(), i / 10 + 1);
  }
}

TEST(PoolAllocatorTest, HeapAllocFail) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 10, sizeof(int));
  heap.FailNextAlloc();
  EXPECT_EQ(allocator.Alloc(sizeof(int), 0), nullptr);
}

TEST(PoolAllocatorTest, AllocTooLarge) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 10, 1000);
  EXPECT_EQ(allocator.Alloc(1001, 0), nullptr);
}

TEST(PoolAllocatorTest, AlignToLarge) {
  TestAllocator heap;
  PoolAllocator allocator(&heap, 10, sizeof(int), 1);
  EXPECT_EQ(allocator.Alloc(sizeof(int), 2), nullptr);
}

}  // namespace
}  // namespace gb
