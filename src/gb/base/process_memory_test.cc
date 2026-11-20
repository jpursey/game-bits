// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/process_memory.h"

#include <cstdint>
#include <memory>
#include <optional>

#include "gtest/gtest.h"

namespace gb {
namespace {

// These are built only where GetProcessMemoryBytes() reports memory.

TEST(ProcessMemoryTest, ReportsPositiveBytes) {
  const std::optional<int64_t> bytes = GetProcessMemoryBytes();
  ASSERT_TRUE(bytes.has_value());
  EXPECT_GT(*bytes, 0);
}

TEST(ProcessMemoryTest, GrowsWithWrittenAllocation) {
  const std::optional<int64_t> before = GetProcessMemoryBytes();
  ASSERT_TRUE(before.has_value());

  // Every page is written, as Linux commits a page only when it is first
  // written. The writes are volatile so the compiler can't remove them, or the
  // allocation with them. The allocation is large, and only half of it needs
  // to show, as Linux counts memory per CPU and reports an approximate sum,
  // which can be tens of megabytes off on a machine with many cores.
  const int kBytes = 256 * 1024 * 1024;
  const int kPageBytes = 4096;
  auto memory = std::make_unique_for_overwrite<char[]>(kBytes);
  volatile char* const pages = memory.get();
  for (int i = 0; i < kBytes; i += kPageBytes) {
    pages[i] = 1;
  }

  const std::optional<int64_t> after = GetProcessMemoryBytes();
  ASSERT_TRUE(after.has_value());
  EXPECT_GE(*after - *before, kBytes / 2);
}

}  // namespace
}  // namespace gb
