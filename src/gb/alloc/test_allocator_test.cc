// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/alloc/test_allocator.h"

#include <cstddef>

#include "gb/test/log_error_guard.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(TestAllocatorTest, CountsAllocations) {
  TestAllocator allocator;
  void* ptr_a = allocator.Alloc(16, 8);
  void* ptr_b = allocator.Alloc(32, 8);
  EXPECT_EQ(allocator.GetAllocCount(), 2);
  EXPECT_EQ(allocator.GetTotalAllocSize(), 48);
  allocator.Free(ptr_a);
  EXPECT_EQ(allocator.GetAllocCount(), 1);
  EXPECT_EQ(allocator.GetTotalAllocSize(), 32);
  allocator.Free(ptr_b);
  EXPECT_EQ(allocator.GetAllocCount(), 0);
  EXPECT_EQ(allocator.GetTotalAllocSize(), 0);
}

TEST(TestAllocatorTest, FailNextAlloc) {
  TestAllocator allocator;
  allocator.FailNextAlloc();
  EXPECT_EQ(allocator.Alloc(16, 8), nullptr);
  void* ptr = allocator.Alloc(16, 8);
  EXPECT_NE(ptr, nullptr);
  allocator.Free(ptr);
}

TEST(TestAllocatorTest, MemoryInAllocationIsValid) {
  TestAllocator allocator;
  std::byte* ptr = static_cast<std::byte*>(allocator.Alloc(16, 8));
  EXPECT_TRUE(allocator.IsValidMemory(ptr, 16, 8));
  EXPECT_TRUE(allocator.IsValidMemory(ptr + 8, 8, 8));
  allocator.Free(ptr);
}

TEST(TestAllocatorTest, MemoryOutsideAllocationsIsInvalid) {
  LogErrorGuard log_error_guard;
  TestAllocator allocator;
  std::byte* ptr = static_cast<std::byte*>(allocator.Alloc(16, 8));
  alignas(8) std::byte other[16];
  EXPECT_FALSE(allocator.IsValidMemory(other, 8, 8));
  EXPECT_FALSE(allocator.IsValidMemory(ptr + 8, 16, 8));
  EXPECT_FALSE(allocator.IsValidMemory(ptr - 8, 8, 8));
  EXPECT_EQ(log_error_guard.TakeMessages().size(), 3);
  allocator.Free(ptr);
}

TEST(TestAllocatorTest, MisalignedMemoryIsInvalid) {
  LogErrorGuard log_error_guard;
  TestAllocator allocator;
  std::byte* ptr = static_cast<std::byte*>(allocator.Alloc(16, 8));
  EXPECT_FALSE(allocator.IsValidMemory(ptr + 4, 4, 8));
  EXPECT_EQ(log_error_guard.TakeMessages().size(), 1);
  allocator.Free(ptr);
}

TEST(TestAllocatorDeathTest, FreeUnknownPointer) {
  TestAllocator allocator;
  int value = 0;
  EXPECT_DEATH(allocator.Free(&value), "");
}

}  // namespace
}  // namespace gb
