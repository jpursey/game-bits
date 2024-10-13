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
    {LexerFlag::kInt64, LexerFlag::kBinaryIntegers},
    {LexerFlag::kFloat64, LexerFlag::kExponentFloats},
    {LexerFlag::kDoubleQuoteString},
    {LexerFlag::kSingleQuoteString},
    {LexerFlag::kDoubleQuoteCharacter},
    {LexerFlag::kSingleQuoteCharacter},
    {LexerFlag::kDoubleQuoteString, LexerFlag::kTabInQuotes},
    {LexerFlag::kDoubleQuoteString, LexerFlag::kQuoteQuoteEscape},
    {LexerFlag::kDoubleQuoteString, LexerFlag::kEscapeCharacter},
    {LexerFlag::kDoubleQuoteString, LexerFlag::kDecodeEscape},
    {LexerFlag::kSingleQuoteCharacter, LexerFlag::kTabInQuotes},
    {LexerFlag::kSingleQuoteCharacter, LexerFlag::kQuoteQuoteEscape},
    {LexerFlag::kSingleQuoteCharacter, LexerFlag::kEscapeCharacter},
    {LexerFlag::kSingleQuoteCharacter, LexerFlag::kDecodeEscape},
    {LexerFlag::kIdentLower, LexerFlag::kIdentForceUpper},
    {LexerFlag::kIdentLower, LexerFlag::kIdentForceLower},
    {LexerFlag::kIdentLower, LexerFlag::kLineBreak},
    {LexerFlag::kIdentLower, LexerFlag::kLineIndent},
    {LexerFlag::kIdentLower, LexerFlag::kLeadingTabs},
    {LexerFlag::kIdentLower, LexerFlag::kEscapeNewline},
    {LexerFlag::kIdentLower, LexerFlag::kLineComments},
    {LexerFlag::kIdentLower, LexerFlag::kBlockComments},
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

TEST(LexerTest, KeywordsUnimplemented) {
  LexerConfig config;
  config.keywords = {
      "if", "else", "while", "for", "return",
  };
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
  EXPECT_EQ(token.GetIndex(), -1);
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
  for (int ch = 0; ch < 256; ++ch, prefix_count = (prefix_count + 1) % 8) {
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

TEST(LexerTest, ParsePositiveIntegers) {
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

TEST(LexerTest, ParsePositiveIntegersWithNegative) {
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
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
  EXPECT_EQ(lexer->GetTokenText(token), "-456");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseNegativeIntegersWithNegative) {
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

TEST(LexerTest, ParseMaxSizeInteger64bit) {
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

TEST(LexerTest, ParseHexIntegerWithoutHexSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
      .hex_prefix = "0x",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0x123 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
      .hex_prefix = "",
      .hex_suffix = "",
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
      .hex_prefix = "",
      .hex_suffix = "",
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

TEST(LexerTest, ParseHexIntegerUpperOnly) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kHexUpperIntegers},
      .hex_prefix = "",
      .hex_suffix = "",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123abc FD0E 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
      .hex_prefix = "",
      .hex_suffix = "",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123abc FD0E 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123ABC);
  EXPECT_EQ(lexer->GetTokenText(token), "123abc");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
      .hex_prefix = "",
      .hex_suffix = "",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123ABC -FD0E 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123ABC);
  EXPECT_EQ(lexer->GetTokenText(token), "123ABC");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
      .hex_prefix = "",
      .hex_suffix = "",
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
      .hex_suffix = "",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("0x123 123A 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0x123);
  EXPECT_EQ(lexer->GetTokenText(token), "0x123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
      .hex_prefix = "",
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
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
  EXPECT_EQ(lexer->GetTokenText(token), "0x123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
      .octal_prefix = "",
      .octal_suffix = "",
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
      .octal_prefix = "",
      .octal_suffix = "",
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

TEST(LexerTest, ParseOctalIntegerNegativeNotSupported) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kOctalIntegers,
                LexerFlag::kNegativeIntegers},
      .octal_prefix = "",
      .octal_suffix = "",
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123 -456 42");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 0123);
  EXPECT_EQ(lexer->GetTokenText(token), "123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
      .octal_prefix = "",
      .octal_suffix = "",
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
      .octal_suffix = "",
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
      .octal_prefix = "",
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
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
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
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
  EXPECT_EQ(lexer->GetTokenText(token), "0b1010");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParsePositiveFloat) {
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

TEST(LexerTest, ParsePositiveFloatWithLeadingPeriod) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent(".25 4.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
  EXPECT_EQ(lexer->GetTokenText(token), ".25");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 4.125);
  EXPECT_EQ(lexer->GetTokenText(token), "4.125");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParsePositiveFloatWithTrailingPeriod) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1. 4.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
  EXPECT_EQ(lexer->GetTokenText(token), "1.");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 4.125);
  EXPECT_EQ(lexer->GetTokenText(token), "4.125");
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParsePositiveFloatsWithNegative) {
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
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
  EXPECT_EQ(lexer->GetTokenText(token), "-4.125");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 42);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParseNegativeFloatsWithNegative) {
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

TEST(LexerTest, ParseExponentFloatWithoutExponentSupport) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kFloat64, LexerFlag::kDecimalFloats},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("1.25e2 42.125");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
  EXPECT_EQ(lexer->GetTokenText(token), "1.25e2");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenFloat);
  EXPECT_EQ(token.GetFloat(), 42.125);
  EXPECT_EQ(lexer->GetTokenText(token), "42.125");
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

}  // namespace
}  // namespace gb