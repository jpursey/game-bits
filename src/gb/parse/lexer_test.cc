// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer.h"

#include "absl/strings/ascii.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

LexerConfig WholeNumbers() {
  LexerConfig config;
  config.flags = {
      LexerFlag::kInt64,
      LexerFlag::kDecimalIntegers,
  };
  return config;
}

const LexerFlags kUnimplementedFlags[] = {
    {LexerFlag::kIdentLower, LexerFlag::kLineBreak},
};

using UnimplementedFlags = testing::TestWithParam<LexerFlags>;
INSTANTIATE_TEST_SUITE_P(LexerTest, UnimplementedFlags,
                         testing::ValuesIn(kUnimplementedFlags));

TEST_P(UnimplementedFlags, Test) {
  LexerConfig config;
  config.flags = GetParam();
  std::string error;
  auto lexer = Lexer::Create(config, &error);
  EXPECT_EQ(lexer, nullptr);
  EXPECT_EQ(error, Lexer::kErrorNotImplemented);
}

TEST(LexerTest, DefaultConfigIsInvalid) {
  LexerConfig config;
  std::string error;
  auto lexer = Lexer::Create(config, &error);
  EXPECT_EQ(lexer, nullptr);
  EXPECT_EQ(error, Lexer::kErrorNoTokenSpec);
}

TEST(LexerTest, AddContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("test content");
  EXPECT_NE(content, kNoLexerContent);
  EXPECT_EQ(lexer->GetFileContentId(""), kNoLexerContent);
  EXPECT_EQ(lexer->GetContentFilename(content), "");
  EXPECT_EQ(lexer->GetContentText(content), "test content");
  EXPECT_EQ(lexer->GetLineCount(content), 1);
}

TEST(LexerTest, AddFileContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddFileContent("test.txt", "test content");
  EXPECT_NE(content, kNoLexerContent);
  EXPECT_EQ(lexer->GetFileContentId("test.txt"), content);
  EXPECT_EQ(lexer->GetContentFilename(content), "test.txt");
  EXPECT_THAT(lexer->GetContentText(content), "test content");
  EXPECT_EQ(lexer->GetLineCount(content), 1);
}

TEST(LexerTest, GetInvalidContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("test content");
  EXPECT_EQ(lexer->GetContentFilename(kNoLexerContent), "");
  EXPECT_EQ(lexer->GetContentText(kNoLexerContent), "");
  EXPECT_EQ(lexer->GetLineCount(kNoLexerContent), 0);
  EXPECT_EQ(lexer->GetLineText(kNoLexerContent, 0), "");
  EXPECT_EQ(lexer->GetContentFilename(content + 1), "");
  EXPECT_EQ(lexer->GetContentText(content + 1), "");
  EXPECT_EQ(lexer->GetLineCount(content + 1), 0);
  EXPECT_EQ(lexer->GetLineText(content + 1, 0), "");
}

TEST(LexerTest, GetInvalidLine) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("test content");
  EXPECT_EQ(lexer->GetLineText(content, -1), "");
  EXPECT_EQ(lexer->GetLineText(content, 1), "");
}

TEST(LexerTest, EmptyContentHasOneLine) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("");
  EXPECT_EQ(lexer->GetLineCount(content), 1);
  EXPECT_EQ(lexer->GetLineText(content, 0), "");
}

TEST(LexerTest, ContentWithNoTrailingNewlineHasOneLine) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2");
  EXPECT_EQ(lexer->GetLineCount(content), 2);
  EXPECT_EQ(lexer->GetLineText(content, 0), "line 1");
  EXPECT_EQ(lexer->GetLineText(content, 1), "line 2");
}

TEST(LexerTest, ContentWithTrailingNewlineHasOneLine) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  EXPECT_EQ(lexer->GetLineCount(content), 2);
  EXPECT_EQ(lexer->GetLineText(content, 0), "line 1");
  EXPECT_EQ(lexer->GetLineText(content, 1), "line 2");
}

TEST(LexerTest, ContentWithEmptyLines) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content =
      lexer->AddContent("\nline 1\n\nline 2\n\n\nline 3\n\n");
  EXPECT_EQ(lexer->GetLineCount(content), 8);
  EXPECT_EQ(lexer->GetLineText(content, 0), "");
  EXPECT_EQ(lexer->GetLineText(content, 1), "line 1");
  EXPECT_EQ(lexer->GetLineText(content, 2), "");
  EXPECT_EQ(lexer->GetLineText(content, 3), "line 2");
  EXPECT_EQ(lexer->GetLineText(content, 4), "");
  EXPECT_EQ(lexer->GetLineText(content, 5), "");
  EXPECT_EQ(lexer->GetLineText(content, 6), "line 3");
  EXPECT_EQ(lexer->GetLineText(content, 7), "");
}

TEST(LexerTest, GetLineLocation) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetLineLocation(content, 0),
            (LexerLocation{.id = content, .line = 0, .column = 0}));
  EXPECT_EQ(lexer->GetLineLocation(content, 1),
            (LexerLocation{.id = content, .line = 1, .column = 0}));
  EXPECT_EQ(lexer->GetLineLocation(content, 2),
            (LexerLocation{.id = content, .line = 2, .column = 0}));
}

TEST(LexerTest, GetLineLocationWithFilename) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content =
      lexer->AddFileContent("test.txt", "line 1\nline 2\nline 3\n");
  EXPECT_EQ(
      lexer->GetLineLocation(content, 0),
      (LexerLocation{
          .id = content, .filename = "test.txt", .line = 0, .column = 0}));
  EXPECT_EQ(
      lexer->GetLineLocation(content, 1),
      (LexerLocation{
          .id = content, .filename = "test.txt", .line = 1, .column = 0}));
  EXPECT_EQ(
      lexer->GetLineLocation(content, 2),
      (LexerLocation{
          .id = content, .filename = "test.txt", .line = 2, .column = 0}));
}

TEST(LexerTest, GetLineLocationForInvalidLine) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetLineLocation(content, -2),
            (LexerLocation{.id = kNoLexerContent}));
  EXPECT_EQ(lexer->GetLineLocation(content, 3),
            (LexerLocation{.id = kNoLexerContent}));
}

TEST(LexerTest, GetLineLocationForInvalidContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetLineLocation(kNoLexerContent, 0),
            (LexerLocation{.id = kNoLexerContent}));
  EXPECT_EQ(lexer->GetLineLocation(content + 1, 0),
            (LexerLocation{.id = kNoLexerContent}));
}

TEST(LexerTest, NextLine) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetCurrentLine(content), 0);
  EXPECT_EQ(lexer->NextLine(content), "line 1");
  EXPECT_EQ(lexer->GetCurrentLine(content), 1);
  EXPECT_EQ(lexer->NextLine(content), "line 2");
  EXPECT_EQ(lexer->GetCurrentLine(content), 2);
  EXPECT_EQ(lexer->NextLine(content), "line 3");
  EXPECT_EQ(lexer->GetCurrentLine(content), 3);
  EXPECT_EQ(lexer->NextLine(content), "");
  EXPECT_EQ(lexer->GetCurrentLine(content), 3);
  EXPECT_EQ(lexer->NextLine(content), "");
}

TEST(LexerTest, NextLineForInvalidContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetCurrentLine(kNoLexerContent), -1);
  EXPECT_EQ(lexer->NextLine(kNoLexerContent), "");
  EXPECT_EQ(lexer->GetCurrentLine(content + 1), -1);
  EXPECT_EQ(lexer->NextLine(content + 1), "");
}

TEST(LexerTest, RewindLine) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  EXPECT_EQ(lexer->NextLine(content), "line 1");
  EXPECT_EQ(lexer->NextLine(content), "line 2");
  EXPECT_EQ(lexer->GetCurrentLine(content), 2);
  EXPECT_TRUE(lexer->RewindLine(content));
  EXPECT_EQ(lexer->GetCurrentLine(content), 1);
  EXPECT_EQ(lexer->NextLine(content), "line 2");
  EXPECT_TRUE(lexer->RewindLine(content));
  EXPECT_TRUE(lexer->RewindLine(content));
  EXPECT_EQ(lexer->GetCurrentLine(content), 0);
  EXPECT_FALSE(lexer->RewindLine(content));
  EXPECT_EQ(lexer->GetCurrentLine(content), 0);
}

TEST(LexerTest, RewindLineForInvalidContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  EXPECT_FALSE(lexer->RewindLine(kNoLexerContent));
  EXPECT_FALSE(lexer->RewindLine(content + 1));
}

TEST(LexerTest, RewindContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  EXPECT_EQ(lexer->NextLine(content), "line 1");
  EXPECT_EQ(lexer->NextLine(content), "line 2");
  EXPECT_EQ(lexer->GetCurrentLine(content), 2);
  lexer->RewindContent(content);
  EXPECT_EQ(lexer->GetCurrentLine(content), 0);
  EXPECT_EQ(lexer->NextLine(content), "line 1");
  EXPECT_EQ(lexer->GetCurrentLine(content), 1);
}

TEST(LexerTest, RewindContentForInvalidContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  lexer->RewindContent(kNoLexerContent);
  lexer->RewindContent(content + 1);
}

TEST(LexerTest, DefaultToken) {
  Token token;
  EXPECT_EQ(token.GetTokenIndex(), TokenIndex());
  EXPECT_EQ(token.GetType(), kTokenNone);
  EXPECT_EQ(token.GetInt(), 0);
  EXPECT_EQ(token.GetFloat(), 0);
  EXPECT_EQ(token.GetString(), "");
  EXPECT_EQ(token.GetSymbol(), Symbol());
}

TEST(LexerTest, GetTokenLocationForDefaultToken) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  Token token;
  EXPECT_EQ(lexer->GetTokenLocation(token),
            (LexerLocation{.id = kNoLexerContent}));
}

TEST(LexerTest, GetTokenLocationForDefaultTokenIndex) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  Token token;
  EXPECT_EQ(lexer->GetTokenLocation(token.GetTokenIndex()),
            (LexerLocation{.id = kNoLexerContent}));
}

TEST(LexerTest, GetTokenTextForDefaultToken) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  Token token;
  EXPECT_EQ(lexer->GetTokenText(token), "");
}

TEST(LexerTest, ParseDefaultTokenIndex) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  Token default_token;
  Token parsed_token = lexer->ParseToken(default_token.GetTokenIndex());
  EXPECT_NE(parsed_token, default_token);
  EXPECT_EQ(parsed_token.GetTokenIndex(), default_token.GetTokenIndex());
  EXPECT_EQ(parsed_token.GetType(), kTokenError);
  EXPECT_EQ(parsed_token.GetString(), Lexer::kErrorInvalidTokenContent);
}

TEST(LexerTest, NextTokenForInvalidContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("");
  Token token = lexer->NextToken(content + 1);
  EXPECT_EQ(lexer->GetTokenLocation(token),
            (LexerLocation{.id = kNoLexerContent}));
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidTokenContent);
}

TEST(LexerTest, RewindTokenForInvalidContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("");
  EXPECT_FALSE(lexer->RewindToken(content + 1));
}

TEST(LexerTest, NextTokenForEmptyContent) {
  auto lexer = Lexer::Create(WholeNumbers());
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(lexer->GetTokenLocation(token),
            (LexerLocation{.id = content, .line = 0, .column = 0}));
  EXPECT_EQ(token.GetType(), kTokenEnd);
}

TEST(LexerTest, InvalidSymbolCharacters) {
  int prefix_count = 0;
  for (int ch = 0; ch < 256;
       ++ch, prefix_count = (prefix_count + 1) % kMaxSymbolSize) {
    if (absl::ascii_isgraph(ch)) {
      continue;
    }
    std::string context = absl::StrCat("Context: ch = ", ch);
    std::string symbol(prefix_count, '+');
    symbol.push_back(ch);
    std::string error;
    auto lexer = Lexer::Create({.symbols = {symbol}}, &error);
    EXPECT_EQ(lexer, nullptr) << context;
    EXPECT_EQ(error, Lexer::kErrorInvalidSymbolSpec) << context;
  }
}

TEST(LexerTest, ParseSymbols) {
  auto lexer =
      Lexer::Create({.symbols = {"+", "-", "*", "/", "++", "--"}}, nullptr);
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("++ * -- / + -");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "++");
  EXPECT_EQ(lexer->GetTokenText(token), "++");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), '*');
  EXPECT_EQ(lexer->GetTokenText(token), "*");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "--");
  EXPECT_EQ(lexer->GetTokenText(token), "--");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), '/');
  EXPECT_EQ(lexer->GetTokenText(token), "/");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), '+');
  EXPECT_EQ(lexer->GetTokenText(token), "+");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), '-');
  EXPECT_EQ(lexer->GetTokenText(token), "-");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerPositive) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123 456 789");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  EXPECT_EQ(lexer->GetTokenText(token), "123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 456);
  EXPECT_EQ(lexer->GetTokenText(token), "456");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 789);
  EXPECT_EQ(lexer->GetTokenText(token), "789");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerNegativeWithoutSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123 -456 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "-456");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerNegativeWithSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kNegativeIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123 -456");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  EXPECT_EQ(lexer->GetTokenText(token), "123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), -456);
  EXPECT_EQ(lexer->GetTokenText(token), "-456");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerMaxSize64bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kNegativeIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "9223372036854775807 -9223372036854775808 "
      "9223372036854775808 -9223372036854775809 "
      "42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int64_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int64_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "9223372036854775808");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "-9223372036854775809");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerMaxSize32bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt32, LexerFlag::kDecimalIntegers,
                LexerFlag::kNegativeIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "2147483647 -2147483648 "
      "2147483648 -2147483649 "
      "42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int32_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int32_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "2147483648");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "-2147483649");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerMaxSize16bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt16, LexerFlag::kDecimalIntegers,
                LexerFlag::kNegativeIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "32767 -32768 "
      "32768 -32769 "
      "42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int16_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int16_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "32768");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "-32769");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerMaxSize8bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt8, LexerFlag::kDecimalIntegers,
                LexerFlag::kNegativeIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "127 -128 "
      "128 -129 "
      "42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int8_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int8_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "128");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "-129");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerWithPrefix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
      .decimal_prefix = "0d",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0d123 123");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  EXPECT_EQ(lexer->GetTokenText(token), "0d123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "123");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerWithSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
      .decimal_suffix = "d",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123d 123");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  EXPECT_EQ(lexer->GetTokenText(token), "123d");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "123");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseDecimalIntegerWithPrefixAndSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
      .decimal_prefix = "0d",
      .decimal_suffix = "d",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0d123d 0d123 123d");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  EXPECT_EQ(lexer->GetTokenText(token), "0d123d");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "0d123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "123d");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerWithoutHexSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
      .hex_prefix = "0x",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0x123 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "0x123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerWithHexSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kHexUpperIntegers,
                LexerFlag::kHexLowerIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123abc FD0e 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123ABC);
  EXPECT_EQ(lexer->GetTokenText(token), "123abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0xFD0E);
  EXPECT_EQ(lexer->GetTokenText(token), "FD0e");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerMaxSize64bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kHexUpperIntegers,
                LexerFlag::kHexLowerIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "7fffffffffffffff 8000000000000000 10000000000000000 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int64_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int64_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "10000000000000000");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerMaxSize32bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt32, LexerFlag::kHexUpperIntegers,
                LexerFlag::kHexLowerIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent("7fffffff 80000000 100000000 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int32_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int32_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "100000000");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerMaxSize16bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt16, LexerFlag::kHexUpperIntegers,
                LexerFlag::kHexLowerIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("7fff 8000 10000 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int16_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int16_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "10000");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerMaxSize8bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt8, LexerFlag::kHexUpperIntegers,
                LexerFlag::kHexLowerIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("7f 80 100 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int8_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int8_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "100");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerUpperOnly) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kHexUpperIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123abc FD0E 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "123abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0xFD0E);
  EXPECT_EQ(lexer->GetTokenText(token), "FD0E");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerLowerOnly) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kHexLowerIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123abc FD0E 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123ABC);
  EXPECT_EQ(lexer->GetTokenText(token), "123abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "FD0E");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerNegativeNotSupported) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kHexUpperIntegers,
                LexerFlag::kNegativeIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123ABC -FD0E 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123ABC);
  EXPECT_EQ(lexer->GetTokenText(token), "123ABC");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "-FD0E");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerMatchedAfterDecimal) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kHexUpperIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123A 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123A);
  EXPECT_EQ(lexer->GetTokenText(token), "123A");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerWithPrefix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kHexUpperIntegers},
      .hex_prefix = "0x",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0x123 123A 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123);
  EXPECT_EQ(lexer->GetTokenText(token), "0x123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "123A");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerWithSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kHexUpperIntegers},
      .hex_suffix = "h",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123h 123A 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123);
  EXPECT_EQ(lexer->GetTokenText(token), "123h");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "123A");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseHexIntegerWithPrefixAndSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kHexUpperIntegers},
      .hex_prefix = "0x",
      .hex_suffix = "h",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0x123h 0x123 123h 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123);
  EXPECT_EQ(lexer->GetTokenText(token), "0x123h");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "0x123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "123h");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerWithoutOctalSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
      .octal_prefix = "0",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0123 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  EXPECT_EQ(lexer->GetTokenText(token), "0123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerWithOctalSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kOctalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123 0456 77");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0123);
  EXPECT_EQ(lexer->GetTokenText(token), "123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0456);
  EXPECT_EQ(lexer->GetTokenText(token), "0456");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 077);
  EXPECT_EQ(lexer->GetTokenText(token), "77");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerMaxSize64Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kOctalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "777777777777777777777 1000000000000000000000 "
      "2000000000000000000000 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int64_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int64_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "2000000000000000000000");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 042);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerMaxSize32Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt32, LexerFlag::kOctalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "17777777777 20000000000 "
      "40000000000 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int32_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int32_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "40000000000");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 042);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerMaxSize16Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt16, LexerFlag::kOctalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "77777 100000 "
      "200000 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int16_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int16_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "200000");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 042);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerMaxSize8Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt8, LexerFlag::kOctalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "177 200 "
      "400 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int8_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int8_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "400");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 042);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerNegativeNotSupported) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kOctalIntegers,
                LexerFlag::kNegativeIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123 -456 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0123);
  EXPECT_EQ(lexer->GetTokenText(token), "123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "-456");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 042);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerMatchedBeforeDecimal) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kOctalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0123 42 81");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0123);
  EXPECT_EQ(lexer->GetTokenText(token), "0123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 042);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 81);
  EXPECT_EQ(lexer->GetTokenText(token), "81");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerWithPrefix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kOctalIntegers},
      .octal_prefix = "0",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0123 0456 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0123);
  EXPECT_EQ(lexer->GetTokenText(token), "0123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0456);
  EXPECT_EQ(lexer->GetTokenText(token), "0456");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerWithSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kOctalIntegers},
      .octal_suffix = "o",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123o 0456 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0123);
  EXPECT_EQ(lexer->GetTokenText(token), "123o");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 456);
  EXPECT_EQ(lexer->GetTokenText(token), "0456");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseOctalIntegerWithPrefixAndSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kOctalIntegers},
      .octal_prefix = "0",
      .octal_suffix = "o",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0123o 0123 456o 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0123);
  EXPECT_EQ(lexer->GetTokenText(token), "0123o");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  EXPECT_EQ(lexer->GetTokenText(token), "0123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "456o");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerWithoutBinarySupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
      .binary_prefix = "0b",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0b1010 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "0b1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerWithBinarySupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kBinaryIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1010 1101 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b1010);
  EXPECT_EQ(lexer->GetTokenText(token), "1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b1101);
  EXPECT_EQ(lexer->GetTokenText(token), "1101");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerMaxSize64Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kBinaryIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "111111111111111111111111111111111111111111111111111111111111111 "
      "1000000000000000000000000000000000000000000000000000000000000000 "
      "10000000000000000000000000000000000000000000000000000000000000000");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int64_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int64_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(
      lexer->GetTokenText(token),
      "10000000000000000000000000000000000000000000000000000000000000000");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerMaxSize32Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt32, LexerFlag::kBinaryIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "1111111111111111111111111111111 "
      "10000000000000000000000000000000 "
      "100000000000000000000000000000000");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int32_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int32_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "100000000000000000000000000000000");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerMaxSize16Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt16, LexerFlag::kBinaryIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "111111111111111 "
      "1000000000000000 "
      "10000000000000000");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int16_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int16_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "10000000000000000");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerMaxSize8Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt8, LexerFlag::kBinaryIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "1111111 "
      "10000000 "
      "100000000");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int8_t>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), std::numeric_limits<int8_t>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
  EXPECT_EQ(lexer->GetTokenText(token), "100000000");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerNegativeNotSupported) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kBinaryIntegers,
                LexerFlag::kNegativeIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1010 -1101 11");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b1010);
  EXPECT_EQ(lexer->GetTokenText(token), "1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "-1101");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b11);
  EXPECT_EQ(lexer->GetTokenText(token), "11");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerMatchedBeforeDecimal) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kBinaryIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1010 1101 12");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b1010);
  EXPECT_EQ(lexer->GetTokenText(token), "1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b1101);
  EXPECT_EQ(lexer->GetTokenText(token), "1101");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 12);
  EXPECT_EQ(lexer->GetTokenText(token), "12");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerWithPrefix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kBinaryIntegers},
      .binary_prefix = "0b",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0b1010 1010 12");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b1010);
  EXPECT_EQ(lexer->GetTokenText(token), "0b1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 1010);
  EXPECT_EQ(lexer->GetTokenText(token), "1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 12);
  EXPECT_EQ(lexer->GetTokenText(token), "12");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerWithSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kBinaryIntegers},
      .binary_suffix = "b",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1010b 1010 12");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b1010);
  EXPECT_EQ(lexer->GetTokenText(token), "1010b");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 1010);
  EXPECT_EQ(lexer->GetTokenText(token), "1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 12);
  EXPECT_EQ(lexer->GetTokenText(token), "12");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBinaryIntegerWithPrefixAndSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kBinaryIntegers},
      .binary_prefix = "0b",
      .binary_suffix = "b",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0b1010b 0b1010 1010b 12");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b1010);
  EXPECT_EQ(lexer->GetTokenText(token), "0b1010b");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "0b1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "1010b");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 12);
  EXPECT_EQ(lexer->GetTokenText(token), "12");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, MatchOrderAllIntegerFormats) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kHexUpperIntegers, LexerFlag::kOctalIntegers,
                LexerFlag::kBinaryIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("101 170 190 1F0");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b101);
  EXPECT_EQ(lexer->GetTokenText(token), "101");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0170);
  EXPECT_EQ(lexer->GetTokenText(token), "170");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 190);
  EXPECT_EQ(lexer->GetTokenText(token), "190");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x1F0);
  EXPECT_EQ(lexer->GetTokenText(token), "1F0");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, IntegerSpecialCharacterPrefixAndSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers,
                LexerFlag::kHexUpperIntegers, LexerFlag::kOctalIntegers,
                LexerFlag::kBinaryIntegers},
      .binary_prefix = ".",
      .binary_suffix = "$",
      .octal_prefix = "[",
      .octal_suffix = "]",
      .decimal_prefix = "\\",
      .decimal_suffix = "^",
      .hex_prefix = "(",
      .hex_suffix = ")",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(".101$ [170] \\190^ (1F0)");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0b101);
  EXPECT_EQ(lexer->GetTokenText(token), ".101$");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0170);
  EXPECT_EQ(lexer->GetTokenText(token), "[170]");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 190);
  EXPECT_EQ(lexer->GetTokenText(token), "\\190^");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x1F0);
  EXPECT_EQ(lexer->GetTokenText(token), "(1F0)");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatPositive) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1.25 42.125 7");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25);
  EXPECT_EQ(lexer->GetTokenText(token), "1.25");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 42.125);
  EXPECT_EQ(lexer->GetTokenText(token), "42.125");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 7);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
  EXPECT_EQ(lexer->GetTokenText(token), "7");
}

TEST(LexerTest, ParseFloatPositiveWithLeadingPeriod) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(".25 4.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), ".25");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 4.125);
  EXPECT_EQ(lexer->GetTokenText(token), "4.125");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatPositiveWithTrailingPeriod) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1. 4.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "1.");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 4.125);
  EXPECT_EQ(lexer->GetTokenText(token), "4.125");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatNegativeWithoutSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1.25 -4.125 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25);
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "-4.125");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatNegativeWithSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats,
                LexerFlag::kNegativeFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1.25 -4.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25);
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), -4.125);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatWithPrefix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats,
                LexerFlag::kNegativeFloats},
      .float_prefix = "$",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("$1.25 $-4.125 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25);
  EXPECT_EQ(lexer->GetTokenText(token), "$1.25");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), -4.125);
  EXPECT_EQ(lexer->GetTokenText(token), "$-4.125");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatWithSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats,
                LexerFlag::kNegativeFloats},
      .float_suffix = "$",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1.25$ -4.125$ 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25);
  EXPECT_EQ(lexer->GetTokenText(token), "1.25$");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), -4.125);
  EXPECT_EQ(lexer->GetTokenText(token), "-4.125$");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "42");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatWithPrefixAndSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats,
                LexerFlag::kNegativeFloats},
      .float_prefix = "$",
      .float_suffix = "f",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("$1.25f $-4.125 42f");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25);
  EXPECT_EQ(lexer->GetTokenText(token), "$1.25f");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "$-4.125");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "42f");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseExponentFloatWithoutSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1.25e2 42.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "1.25e2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 42.125);
  EXPECT_EQ(lexer->GetTokenText(token), "42.125");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseExponentFloatPositiveWithSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kExponentFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent("1.25e2 1e+2 1.0E-2 -4.5e1 42.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25e2);
  EXPECT_EQ(lexer->GetTokenText(token), "1.25e2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1e+2);
  EXPECT_EQ(lexer->GetTokenText(token), "1e+2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.e-2);
  EXPECT_EQ(lexer->GetTokenText(token), "1.0E-2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "-4.5e1");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "42.125");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseExponentFloatNegativeWithSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kExponentFloats,
                LexerFlag::kNegativeFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent("1.25e2 1e+2 1.0E-2 -4.5e1 42.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25e2);
  EXPECT_EQ(lexer->GetTokenText(token), "1.25e2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1e+2);
  EXPECT_EQ(lexer->GetTokenText(token), "1e+2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.e-2);
  EXPECT_EQ(lexer->GetTokenText(token), "1.0E-2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), -4.5e1);
  EXPECT_EQ(lexer->GetTokenText(token), "-4.5e1");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "42.125");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatAllFormats) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats,
                LexerFlag::kExponentFloats, LexerFlag::kNegativeFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent("1.25e2 1e+2 1.0E-2 -4.5e1 42.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.25e2);
  EXPECT_EQ(lexer->GetTokenText(token), "1.25e2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1e+2);
  EXPECT_EQ(lexer->GetTokenText(token), "1e+2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 1.e-2);
  EXPECT_EQ(lexer->GetTokenText(token), "1.0E-2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), -4.5e1);
  EXPECT_EQ(lexer->GetTokenText(token), "-4.5e1");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 42.125);
  EXPECT_EQ(lexer->GetTokenText(token), "42.125");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatMaxSize64Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats,
                LexerFlag::kExponentFloats, LexerFlag::kNegativeFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "1.7976931348623157e+308 1e309 "
      "2.2250738585072014e-308 1e-309 "
      "-1.7976931348623157e+308 -1e309");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), std::numeric_limits<double>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidFloat);
  EXPECT_EQ(lexer->GetTokenText(token), "1e309");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), std::numeric_limits<double>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidFloat);
  EXPECT_EQ(lexer->GetTokenText(token), "1e-309");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), std::numeric_limits<double>::lowest());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidFloat);
  EXPECT_EQ(lexer->GetTokenText(token), "-1e309");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseFloatMaxSize32Bit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat32, LexerFlag::kDecimalFloats,
                LexerFlag::kExponentFloats, LexerFlag::kNegativeFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(
      "3.40282347e+38 1e39 "
      "1.17549435e-38 1e-39 "
      "-3.40282347e+38 -1e39");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), std::numeric_limits<float>::max());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidFloat);
  EXPECT_EQ(lexer->GetTokenText(token), "1e39");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), std::numeric_limits<float>::min());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidFloat);
  EXPECT_EQ(lexer->GetTokenText(token), "1e-39");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), std::numeric_limits<float>::lowest());
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidFloat);
  EXPECT_EQ(lexer->GetTokenText(token), "-1e39");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ConflictingStringAndCharSpecs) {
  std::string error;
  auto lexer = Lexer::Create({.flags = {LexerFlag::kDoubleQuoteString,
                                        LexerFlag::kDoubleQuoteCharacter}},
                             &error);
  EXPECT_EQ(lexer, nullptr);
  EXPECT_EQ(error, Lexer::kErrorConflictingStringAndCharSpec);

  lexer = Lexer::Create({.flags = {LexerFlag::kSingleQuoteString,
                                   LexerFlag::kSingleQuoteCharacter}},
                        &error);
  EXPECT_EQ(lexer, nullptr);
  EXPECT_EQ(error, Lexer::kErrorConflictingStringAndCharSpec);
}

TEST(LexerTest, ParseCharSingleQuote) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteCharacter},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('a' ' ' '\' "b" '' '\x4B' '\t' '\n' '\'' '''' )"
                        "'\t' '\nx'");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "a");
  EXPECT_EQ(lexer->GetTokenText(token), "'a'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), " ");
  EXPECT_EQ(lexer->GetTokenText(token), "' '");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"b\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'\\x4B'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'\\t'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'\\n'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'\\''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "''''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\t");
  EXPECT_EQ(lexer->GetTokenText(token), "'\t'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "x'");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseCharDoubleQuote) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kDoubleQuoteCharacter},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"("a" " " "\" 'b' "" "\x4B" "\t" "\n" "\"" """" )"
                        "\"\t\" \"\nx\"");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "a");
  EXPECT_EQ(lexer->GetTokenText(token), "\"a\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), " ");
  EXPECT_EQ(lexer->GetTokenText(token), "\" \"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'b'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\x4B\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\t\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\n\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\"\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\t");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\t\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "x\"");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseCharBothQuoteTypes) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteCharacter,
                LexerFlag::kDoubleQuoteCharacter},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("'\"' \"'\"");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\"");
  EXPECT_EQ(lexer->GetTokenText(token), "'\"'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "'");
  EXPECT_EQ(lexer->GetTokenText(token), "\"'\"");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseCharQuoteQuoteEscape) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteCharacter,
                LexerFlag::kDoubleQuoteCharacter, LexerFlag::kQuoteQuoteEscape},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(R"('''' """" '""' "''")");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "''");
  EXPECT_EQ(lexer->GetTokenText(token), "''''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\"\"");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\"\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'\"\"'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"''\"");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseCharWithEscapeChar) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteCharacter,
                LexerFlag::kDoubleQuoteCharacter, LexerFlag::kEscapeCharacter},
      .escape = '\\',
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('\\' "\\" '\'' "\"" '\n' '\t' '\#' '\' "\")");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\\\");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\\\'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\\\");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\\\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\'");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\\"");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\n");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\n'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\t");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\t'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\\#");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\#'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'\\'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\"");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseCharWithDecodeNoSpecialCodes) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteCharacter,
                LexerFlag::kDoubleQuoteCharacter, LexerFlag::kEscapeCharacter,
                LexerFlag::kDecodeEscape},
      .escape = '$',
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('$$' '$'' '$n' '$t' '$x4B')");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "$");
  EXPECT_EQ(lexer->GetTokenText(token), "'$$'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "'");
  EXPECT_EQ(lexer->GetTokenText(token), "'$''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "n");
  EXPECT_EQ(lexer->GetTokenText(token), "'$n'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "t");
  EXPECT_EQ(lexer->GetTokenText(token), "'$t'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'$x4B'");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseCharWithDecodeAndSpecialCodes) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteCharacter,
                LexerFlag::kDoubleQuoteCharacter, LexerFlag::kEscapeCharacter,
                LexerFlag::kDecodeEscape},
      .escape = '$',
      .escape_newline = 'n',
      .escape_tab = 't',
      .escape_hex = 'x',
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('$$' '$'' '$n' '$t' '$x4B')");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "$");
  EXPECT_EQ(lexer->GetTokenText(token), "'$$'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "'");
  EXPECT_EQ(lexer->GetTokenText(token), "'$''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\n");
  EXPECT_EQ(lexer->GetTokenText(token), "'$n'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "\t");
  EXPECT_EQ(lexer->GetTokenText(token), "'$t'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenChar);
  EXPECT_EQ(token.GetString(), "K");
  EXPECT_EQ(lexer->GetTokenText(token), "'$x4B'");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseStringSingleQuote) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteString},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('abc' ' ' '\' "def" '' '\x4B\t\n' '\'' '''' )"
                        "' \t ' '\nx'");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "abc");
  EXPECT_EQ(lexer->GetTokenText(token), "'abc'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), " ");
  EXPECT_EQ(lexer->GetTokenText(token), "' '");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "\\");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"def\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "");
  EXPECT_EQ(lexer->GetTokenText(token), "''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "\\x4B\\t\\n");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\x4B\\t\\n'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'\\''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "''''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), " \t ");
  EXPECT_EQ(lexer->GetTokenText(token), "' \t '");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "x'");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseStringDoubleQuote) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kDoubleQuoteString},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"("abc" " " "\" 'def' "" "\x4B\t\n" "\"" """" )"
                        "\" \t \" \"\nx\"");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "abc");
  EXPECT_EQ(lexer->GetTokenText(token), "\"abc\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), " ");
  EXPECT_EQ(lexer->GetTokenText(token), "\" \"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "\\");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'def'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "\\x4B\\t\\n");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\x4B\\t\\n\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\"\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), " \t ");
  EXPECT_EQ(lexer->GetTokenText(token), "\" \t \"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "x\"");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseStringBothQuoteTypes) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteString, LexerFlag::kDoubleQuoteString},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent("'\"hello\"' \"'good-bye!'\"");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "\"hello\"");
  EXPECT_EQ(lexer->GetTokenText(token), "'\"hello\"'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "'good-bye!'");
  EXPECT_EQ(lexer->GetTokenText(token), "\"'good-bye!'\"");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseStringQuoteQuoteEscape) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteString, LexerFlag::kDoubleQuoteString,
                LexerFlag::kQuoteQuoteEscape},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('''hello''' """good-bye!""" ' '' ''' " "" """)"
                        "\n'\n''\n'''");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "''hello''");
  EXPECT_EQ(lexer->GetTokenText(token), "'''hello'''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "\"\"good-bye!\"\"");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\"\"good-bye!\"\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), " '' ''");
  EXPECT_EQ(lexer->GetTokenText(token), "' '' '''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), " \"\" \"\"");
  EXPECT_EQ(lexer->GetTokenText(token), "\" \"\" \"\"\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "");
  EXPECT_EQ(lexer->GetTokenText(token), "''");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'''");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseStringWithEscapeChar) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteString, LexerFlag::kDoubleQuoteString,
                LexerFlag::kEscapeCharacter},
      .escape = '\\',
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('\\\'\"\n\t\#' "\\\"\"\n\t\#")"
                        "\n'\\'"
                        "\n\"\\\"");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "\\\\\\'\\\"\\n\\t\\#");
  EXPECT_EQ(lexer->GetTokenText(token), "'\\\\\\'\\\"\\n\\t\\#'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "\\\\\\\"\\\"\\n\\t\\#");
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\\\\\"\\\"\\n\\t\\#\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "'\\'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "\"\\\"");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseStringWithDecodeNoSpecialCodes) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteString, LexerFlag::kDoubleQuoteString,
                LexerFlag::kEscapeCharacter, LexerFlag::kDecodeEscape},
      .escape = '$',
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('$$$'$x4B$n$t' "$$$"$x4B$n$t" 'start$#mid$*end')");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "$'x4Bnt");
  EXPECT_EQ(lexer->GetTokenText(token), "'$$$'$x4B$n$t'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "$\"x4Bnt");
  EXPECT_EQ(lexer->GetTokenText(token), "\"$$$\"$x4B$n$t\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "start#mid*end");
  EXPECT_EQ(lexer->GetTokenText(token), "'start$#mid$*end'");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseStringWithDecodeAndSpecialCodes) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kSingleQuoteString, LexerFlag::kDoubleQuoteString,
                LexerFlag::kEscapeCharacter, LexerFlag::kDecodeEscape},
      .escape = '$',
      .escape_newline = 'n',
      .escape_tab = 't',
      .escape_hex = 'x',
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content =
      lexer->AddContent(R"('$$$'$x4B$n$t' "$$$"$x4B$n$t" 'start$nmid$tend')");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "$'K\n\t");
  EXPECT_EQ(lexer->GetTokenText(token), "'$$$'$x4B$n$t'");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "$\"K\n\t");
  EXPECT_EQ(lexer->GetTokenText(token), "\"$$$\"$x4B$n$t\"");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "start\nmid\tend");
  EXPECT_EQ(lexer->GetTokenText(token), "'start$nmid$tend'");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseKeyword) {
  auto lexer = Lexer::Create({
      .keywords = {"if", "else", "while"},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("else while if whiles");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "else");
  EXPECT_EQ(lexer->GetTokenText(token), "else");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "while");
  EXPECT_EQ(lexer->GetTokenText(token), "while");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "if");
  EXPECT_EQ(lexer->GetTokenText(token), "if");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "whiles");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseKeywordWithSpecialCharacters) {
  auto lexer = Lexer::Create({
      .keywords = {"$if", "else\\", "wh|ile"},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("else\\ wh|ile $if while");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "else\\");
  EXPECT_EQ(lexer->GetTokenText(token), "else\\");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "wh|ile");
  EXPECT_EQ(lexer->GetTokenText(token), "wh|ile");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "$if");
  EXPECT_EQ(lexer->GetTokenText(token), "$if");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "while");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseKeywordCaseInsensitive) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kKeywordCaseInsensitive},
      .keywords = {"if", "Else", "wHiLe"},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("ELSE WhIlE iF WhIlEs");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "Else");
  EXPECT_EQ(lexer->GetTokenText(token), "ELSE");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "wHiLe");
  EXPECT_EQ(lexer->GetTokenText(token), "WhIlE");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "if");
  EXPECT_EQ(lexer->GetTokenText(token), "iF");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "WhIlEs");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ConflictingForceUpperAndLower) {
  std::string error;
  auto lexer = Lexer::Create(
      {.flags = {LexerFlag::kIdentForceUpper, LexerFlag::kIdentForceLower}},
      &error);
  EXPECT_EQ(lexer, nullptr);
  EXPECT_EQ(error, Lexer::kErrorConflictingIdentifierSpec);
}

TEST(LexerTest, ParseIdentLower) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentLower},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc DEF gHi x");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "abc");
  EXPECT_EQ(lexer->GetTokenText(token), "abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "DEF");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "gHi");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "x");
  EXPECT_EQ(lexer->GetTokenText(token), "x");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentUpper) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc DEF gHi X");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "DEF");
  EXPECT_EQ(lexer->GetTokenText(token), "DEF");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "gHi");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "X");
  EXPECT_EQ(lexer->GetTokenText(token), "X");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentDigit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower,
                LexerFlag::kIdentDigit},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc123 D45E 6HIj 5");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "abc123");
  EXPECT_EQ(lexer->GetTokenText(token), "abc123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "D45E");
  EXPECT_EQ(lexer->GetTokenText(token), "D45E");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "6HIj");
  EXPECT_EQ(lexer->GetTokenText(token), "6HIj");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "5");
  EXPECT_EQ(lexer->GetTokenText(token), "5");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentNonLeadDigit) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower,
                LexerFlag::kIdentNonLeadDigit},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc123 D45E 6HIj 5");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "abc123");
  EXPECT_EQ(lexer->GetTokenText(token), "abc123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "D45E");
  EXPECT_EQ(lexer->GetTokenText(token), "D45E");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "6HIj");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "5");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentNonLeadDigitTakesPrecedence) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower,
                LexerFlag::kIdentNonLeadDigit, LexerFlag::kIdentDigit},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("6HIj");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "6HIj");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentUnderscore) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower,
                LexerFlag::kIdentUnderscore},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc_ D__E _HIj _");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "abc_");
  EXPECT_EQ(lexer->GetTokenText(token), "abc_");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "D__E");
  EXPECT_EQ(lexer->GetTokenText(token), "D__E");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "_HIj");
  EXPECT_EQ(lexer->GetTokenText(token), "_HIj");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "_");
  EXPECT_EQ(lexer->GetTokenText(token), "_");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentNonLeadUnderscore) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower,
                LexerFlag::kIdentNonLeadUnderscore},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc_ D__E _HIj _");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "abc_");
  EXPECT_EQ(lexer->GetTokenText(token), "abc_");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "D__E");
  EXPECT_EQ(lexer->GetTokenText(token), "D__E");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "_HIj");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "_");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentNonLeadUnderscoreTakesPrecedence) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower,
                LexerFlag::kIdentNonLeadUnderscore,
                LexerFlag::kIdentUnderscore},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("_HIj");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "_HIj");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentWithPrefix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower},
      .ident_prefix = "id_",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("id_abc id_DEF Id_gHi id_x");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "id_abc");
  EXPECT_EQ(lexer->GetTokenText(token), "id_abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "id_DEF");
  EXPECT_EQ(lexer->GetTokenText(token), "id_DEF");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "Id_gHi");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "id_x");
  EXPECT_EQ(lexer->GetTokenText(token), "id_x");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentWithSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower},
      .ident_suffix = "_id",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc_id DEF_id gHi_ID x_id");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "abc_id");
  EXPECT_EQ(lexer->GetTokenText(token), "abc_id");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "DEF_id");
  EXPECT_EQ(lexer->GetTokenText(token), "DEF_id");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "gHi_ID");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "x_id");
  EXPECT_EQ(lexer->GetTokenText(token), "x_id");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentWithPrefixAndSuffix) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentUpper, LexerFlag::kIdentLower},
      .ident_prefix = "$",
      .ident_suffix = "*",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("$abc* $DEF gHi* $x*");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "$abc*");
  EXPECT_EQ(lexer->GetTokenText(token), "$abc*");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "$DEF");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidToken);
  EXPECT_EQ(lexer->GetTokenText(token), "gHi*");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "$x*");
  EXPECT_EQ(lexer->GetTokenText(token), "$x*");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentForceUpper) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentForceUpper},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc DEF gHi");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "ABC");
  EXPECT_EQ(lexer->GetTokenText(token), "abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "DEF");
  EXPECT_EQ(lexer->GetTokenText(token), "DEF");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "GHI");
  EXPECT_EQ(lexer->GetTokenText(token), "gHi");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentForceLower) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentForceLower},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("abc DEF gHi");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "abc");
  EXPECT_EQ(lexer->GetTokenText(token), "abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "def");
  EXPECT_EQ(lexer->GetTokenText(token), "DEF");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "ghi");
  EXPECT_EQ(lexer->GetTokenText(token), "gHi");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentMatchesAfterKeyword) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentLower},
      .keywords = {"if", "else", "while"},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("ifs if els else while");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "ifs");
  EXPECT_EQ(lexer->GetTokenText(token), "ifs");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "if");
  EXPECT_EQ(lexer->GetTokenText(token), "if");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "els");
  EXPECT_EQ(lexer->GetTokenText(token), "els");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "else");
  EXPECT_EQ(lexer->GetTokenText(token), "else");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "while");
  EXPECT_EQ(lexer->GetTokenText(token), "while");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseIdentMatchesAfterKeywordCaseInsensitive) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kIdentForceUpper,
                LexerFlag::kKeywordCaseInsensitive},
      .keywords = {"if", "else", "while"},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("ifs IF els Else while");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "IFS");
  EXPECT_EQ(lexer->GetTokenText(token), "ifs");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "if");
  EXPECT_EQ(lexer->GetTokenText(token), "IF");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "ELS");
  EXPECT_EQ(lexer->GetTokenText(token), "els");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "else");
  EXPECT_EQ(lexer->GetTokenText(token), "Else");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "while");
  EXPECT_EQ(lexer->GetTokenText(token), "while");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseLineComments) {
  auto lexer = Lexer::Create({
      .flags = {kLexerFlags_AllIntegers, kLexerFlags_CStrings,
                kLexerFlags_CIdentifiers},
      .line_comments = {"//", "$"},
      .symbols = kCStyleSymbols,
      .keywords = {"int", "return"},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(R"---(
// Comment at the beginning of a line
int Add(x, y) {// Comment after a symbol
  $ Multiple comments $ later ones don't matter
  // of different $types$ after whitespace
  z = "// comment $ inside a string";
  return x$Comment after an identifier
         + y; // Comment at the end of a line
}
)---");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "int");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "Add");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "(");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "x");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), ",");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "y");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), ")");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "{");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "z");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "=");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "// comment $ inside a string");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), ";");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "return");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "x");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "+");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "y");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), ";");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "}");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseBlockComments) {
  auto lexer = Lexer::Create({
      .flags = {kLexerFlags_AllIntegers, kLexerFlags_CStrings,
                kLexerFlags_CIdentifiers},
      .block_comments = {{"/*", "*/"}, {"$", "$"}},
      .symbols = kCStyleSymbols,
      .keywords = {"int", "return"},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(R"---(
/* Comment at the beginning of a line */
int Add(x, y) {/* Comment after a symbol */
  $ Multiple comments later ones don't matter $
  /* of different $types$ after whitespace */ z = "/*inside a string*/";
  return x$Comment after an identifier$+ y; /* Comment at the end of a line */
}
)---");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "int");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "Add");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "(");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "x");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), ",");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "y");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), ")");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "{");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "z");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "=");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenString);
  EXPECT_EQ(token.GetString(), "/*inside a string*/");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), ";");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenKeyword);
  EXPECT_EQ(token.GetString(), "return");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "x");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "+");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenIdentifier);
  EXPECT_EQ(token.GetString(), "y");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), ";");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "}");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

}  // namespace
}  // namespace gb