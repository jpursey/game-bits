// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/container/offset_array.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(OffsetArrayTest, OffsetArray) {
  OffsetArray<int, 2, 5> array;
  for (int i = -2; i < 3; ++i) {
    EXPECT_EQ(array(i), 0);
    array(i) = i;
  }
  const OffsetArray<int, 2, 5>& const_array = array;
  for (int i = -2; i < 3; ++i) {
    EXPECT_EQ(const_array(i), i);
  }
}

TEST(OffsetArrayTest, OffsetArray2d) {
  OffsetArray2d<int, 1, 3> array;
  for (int x = -1; x < 2; ++x) {
    for (int y = -1; y < 2; ++y) {
      EXPECT_EQ(array(x, y), 0);
      array(x, y) = x * 10 + y;
    }
  }
  const OffsetArray2d<int, 1, 3>& const_array = array;
  for (int x = -1; x < 2; ++x) {
    for (int y = -1; y < 2; ++y) {
      EXPECT_EQ(const_array(x, y), x * 10 + y);
      EXPECT_EQ(const_array({x, y}), x * 10 + y);
    }
  }
  array({1, -1}) = 100;
  EXPECT_EQ(array(1, -1), 100);
}

TEST(OffsetArrayTest, OffsetArray3d) {
  OffsetArray3d<int, 1, 3> array;
  for (int x = -1; x < 2; ++x) {
    for (int y = -1; y < 2; ++y) {
      for (int z = -1; z < 2; ++z) {
        EXPECT_EQ(array(x, y, z), 0);
        array(x, y, z) = x * 100 + y * 10 + z;
      }
    }
  }
  const OffsetArray3d<int, 1, 3>& const_array = array;
  for (int x = -1; x < 2; ++x) {
    for (int y = -1; y < 2; ++y) {
      for (int z = -1; z < 2; ++z) {
        EXPECT_EQ(const_array(x, y, z), x * 100 + y * 10 + z);
        EXPECT_EQ(const_array({x, y, z}), x * 100 + y * 10 + z);
      }
    }
  }
  array({1, -1, 0}) = 1000;
  EXPECT_EQ(array(1, -1, 0), 1000);
}

}  // namespace
}  // namespace gb
