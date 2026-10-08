// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/test/test_util.h"

#include <string>

#include "gtest/gtest.h"

namespace gb {
namespace {

constexpr int kTestStringSize = 10000;

TEST(TestUtilTest, GenerateTestStringEmpty) {
  EXPECT_EQ(GenerateTestString(0), "");
}

TEST(TestUtilTest, GenerateTestStringSingleCharacter) {
  const std::string text = GenerateTestString(1);
  ASSERT_EQ(text.size(), 1);
  EXPECT_GE(text[0], ' ');
  EXPECT_LE(text[0], '~');
}

TEST(TestUtilTest, GenerateTestString) {
  const std::string text = GenerateTestString(kTestStringSize);
  ASSERT_EQ(text.size(), kTestStringSize);
  for (int i = 0; i < kTestStringSize; ++i) {
    EXPECT_GE(text[i], ' ') << "at " << i;
    EXPECT_LE(text[i], '~') << "at " << i;
    if (i > 0) {
      EXPECT_NE(text[i], text[i - 1]) << "at " << i;
    }
  }
}

TEST(TestUtilTest, GenerateAlphaTestStringEmpty) {
  EXPECT_EQ(GenerateAlphaTestString(0), "");
}

TEST(TestUtilTest, GenerateAlphaTestStringSingleCharacter) {
  const std::string text = GenerateAlphaTestString(1);
  ASSERT_EQ(text.size(), 1);
  EXPECT_GE(text[0], 'a');
  EXPECT_LE(text[0], 'z');
}

TEST(TestUtilTest, GenerateAlphaTestString) {
  const std::string text = GenerateAlphaTestString(kTestStringSize);
  ASSERT_EQ(text.size(), kTestStringSize);
  for (int i = 0; i < kTestStringSize; ++i) {
    EXPECT_GE(text[i], 'a') << "at " << i;
    EXPECT_LE(text[i], 'z') << "at " << i;
    if (i > 0) {
      EXPECT_NE(text[i], text[i - 1]) << "at " << i;
    }
  }
}

}  // namespace
}  // namespace gb
