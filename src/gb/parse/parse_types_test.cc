// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parse_types.h"

#include <compare>

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(TokenIndexTest, EqualWhenLineAndTokenMatch) {
  EXPECT_EQ(TokenIndex(3, 4), TokenIndex(3, 4));
  EXPECT_NE(TokenIndex(3, 4), TokenIndex(3, 5));
  EXPECT_NE(TokenIndex(3, 4), TokenIndex(4, 4));
  EXPECT_EQ(TokenIndex(3, 4) <=> TokenIndex(3, 4), std::strong_ordering::equal);
}

TEST(TokenIndexTest, OrderedByTokenWithinALine) {
  EXPECT_LT(TokenIndex(3, 4), TokenIndex(3, 5));
  EXPECT_GT(TokenIndex(3, 5), TokenIndex(3, 4));
}

TEST(TokenIndexTest, OrderedByLineBeforeToken) {
  EXPECT_LT(TokenIndex(3, 9), TokenIndex(4, 0));
  EXPECT_GT(TokenIndex(4, 0), TokenIndex(3, 9));
}

TEST(TokenIndexTest, OrderedAcrossTheFullRange) {
  // The largest value each field holds: the end token sets every token bit.
  const TokenIndex last_line(kMaxLexerLines, 0);
  const TokenIndex end_token(0, kTokenIndexEndToken);
  EXPECT_LT(TokenIndex(0, kMaxTokensPerLine), end_token);
  EXPECT_LT(end_token, TokenIndex(1, 0));
  EXPECT_LT(TokenIndex(kMaxLexerLines - 1, kTokenIndexEndToken), last_line);
  EXPECT_LT(last_line, TokenIndex(kMaxLexerLines, kTokenIndexEndToken));
}

}  // namespace
}  // namespace gb
