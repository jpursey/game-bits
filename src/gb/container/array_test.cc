// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/container/array.h"

#include <string>
#include <utility>

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(ArrayTest, ZeroInitializesTrivialTypes) {
  Array<int, 4> array;
  for (int value : array) {
    EXPECT_EQ(value, 0);
  }
}

TEST(ArrayTest, DefaultConstructsOtherTypes) {
  Array<std::string, 2> array;
  for (const std::string& value : array) {
    EXPECT_EQ(value, "");
  }
}

struct ExplicitDefault {
  explicit ExplicitDefault() = default;
  int value;
};

TEST(ArrayTest, ElementsWithExplicitDefaultConstructor) {
  Array<ExplicitDefault, 2> array;
  EXPECT_EQ(array[1].value, 0);
}

TEST(ArrayTest, ConstantInitialized) {
  constexpr Array<int, 3> kArray;
  static_assert(kArray[2] == 0);
  constexpr Array3d<int, 2> kArray3d;
  static_assert(kArray3d(1, 1, 1) == 0);
}

TEST(ArrayTest, Array) {
  Array<int, 3> array;
  EXPECT_EQ(array.dim(), 3);
  EXPECT_EQ(array.size(), 3);
  array[1] = 5;
  EXPECT_EQ(array(1), 5);
  EXPECT_EQ(array.data()[array.int_index(1)], 5);
}

TEST(ArrayTest, ArrayXYIsXMajor) {
  ArrayXY<int, 2, 3> array;
  EXPECT_EQ(array.dim(), glm::ivec2(2, 3));
  EXPECT_EQ(array.size(), 6);
  EXPECT_EQ(array.int_index(0, 1), 1);
  EXPECT_EQ(array.int_index(1, 0), 3);
  array(1, 2) = 5;
  EXPECT_EQ(array[glm::ivec2(1, 2)], 5);
  EXPECT_EQ(array.data()[5], 5);
}

TEST(ArrayTest, ArrayYXIsYMajor) {
  ArrayYX<int, 2, 3> array;
  EXPECT_EQ(array.dim(), glm::ivec2(2, 3));
  EXPECT_EQ(array.size(), 6);
  EXPECT_EQ(array.int_index(1, 0), 1);
  EXPECT_EQ(array.int_index(0, 1), 2);
  array(1, 2) = 5;
  EXPECT_EQ(array[glm::ivec2(1, 2)], 5);
  EXPECT_EQ(array.data()[5], 5);
}

TEST(ArrayTest, ArrayXYZIsXMajor) {
  ArrayXYZ<int, 2, 3, 4> array;
  EXPECT_EQ(array.dim(), glm::ivec3(2, 3, 4));
  EXPECT_EQ(array.size(), 24);
  EXPECT_EQ(array.int_index(0, 0, 1), 1);
  EXPECT_EQ(array.int_index(0, 1, 0), 4);
  EXPECT_EQ(array.int_index(1, 0, 0), 12);
  array(1, 2, 3) = 5;
  EXPECT_EQ(array[glm::ivec3(1, 2, 3)], 5);
  EXPECT_EQ(array.data()[23], 5);
}

TEST(ArrayTest, ArrayZYXIsZMajor) {
  ArrayZYX<int, 2, 3, 4> array;
  EXPECT_EQ(array.dim(), glm::ivec3(2, 3, 4));
  EXPECT_EQ(array.size(), 24);
  EXPECT_EQ(array.int_index(1, 0, 0), 1);
  EXPECT_EQ(array.int_index(0, 1, 0), 2);
  EXPECT_EQ(array.int_index(0, 0, 1), 6);
  array(1, 2, 3) = 5;
  EXPECT_EQ(array[glm::ivec3(1, 2, 3)], 5);
  EXPECT_EQ(array.data()[23], 5);
}

TEST(ArrayTest, Array3dHasEachSize) {
  Array3d<int, 2, 3, 4> array;
  EXPECT_EQ(array.dim(), glm::ivec3(2, 3, 4));
  EXPECT_EQ(array.size(), 24);
}

TEST(ArrayTest, Fill) {
  Array2d<int, 2> array;
  array.fill(3);
  for (int value : array) {
    EXPECT_EQ(value, 3);
  }
}

TEST(ArrayTest, Swap) {
  Array2d<int, 2> array_1;
  array_1.fill(1);
  Array2d<int, 2> array_2;
  array_2.fill(2);

  array_1.swap(array_2);
  EXPECT_EQ(array_1(1, 1), 2);
  EXPECT_EQ(array_2(1, 1), 1);

  using std::swap;
  swap(array_1, array_2);
  EXPECT_EQ(array_1(1, 1), 1);
  EXPECT_EQ(array_2(1, 1), 2);
}

// Arrays can only be swapped or compared with arrays of the same layout.
template <typename A, typename B>
concept Swappable = requires(A& a, B& b) { a.swap(b); };
template <typename A, typename B>
concept Comparable = requires(const A& a, const B& b) { a == b; };
static_assert(Swappable<ArrayXY<int, 2, 3>, ArrayXY<int, 2, 3>>);
static_assert(!Swappable<ArrayXY<int, 2, 3>, ArrayYX<int, 2, 3>>);
static_assert(!Swappable<ArrayXY<int, 2, 3>, ArrayXY<int, 3, 2>>);
static_assert(Comparable<ArrayXY<int, 2, 3>, ArrayXY<int, 2, 3>>);
static_assert(!Comparable<ArrayXY<int, 2, 3>, ArrayYX<int, 2, 3>>);
static_assert(!Comparable<ArrayXY<int, 2, 3>, ArrayXY<int, 3, 2>>);

TEST(ArrayTest, Equality) {
  Array3d<int, 2> array_1;
  Array3d<int, 2> array_2;
  EXPECT_TRUE(array_1 == array_2);
  array_2(1, 0, 1) = 1;
  EXPECT_FALSE(array_1 == array_2);
  EXPECT_TRUE(array_1 != array_2);
  constexpr Array<int, 2> kArray_1;
  constexpr Array<int, 2> kArray_2;
  static_assert(kArray_1 == kArray_2);
}

}  // namespace
}  // namespace gb
