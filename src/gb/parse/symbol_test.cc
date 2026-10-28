// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/symbol.h"

#include <string>

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(SymbolTest, LongestSymbol) {
  const std::string text(kMaxSymbolSize, '+');
  const Symbol symbol(text);
  EXPECT_TRUE(symbol.IsValid());
  EXPECT_EQ(symbol.GetString(), text);
  EXPECT_EQ(symbol.GetSize(), kMaxSymbolSize);
}

TEST(SymbolTest, TooLongIsEmpty) {
  const std::string text(kMaxSymbolSize + 1, '+');
  const Symbol symbol(text);
  EXPECT_FALSE(symbol.IsValid());
  EXPECT_EQ(symbol, Symbol());
  EXPECT_NE(symbol, Symbol(text.substr(0, kMaxSymbolSize)));
}

TEST(SymbolTest, ValidCharacters) {
  for (int ch = 1; ch < 256; ++ch) {
    EXPECT_EQ(Symbol(static_cast<char>(ch)).IsValid(), ch >= 33 && ch <= 126)
        << "ch = " << ch;
  }
}

TEST(SymbolTest, ConstantSymbols) {
  // Each of these would fail to compile if it were invalid.
  constexpr Symbol kChar = '+';
  constexpr Symbol kLongest = "1234567";
  static_assert(kChar.GetString() == "+");
  static_assert(kLongest.GetString() == "1234567");
}

}  // namespace
}  // namespace gb
