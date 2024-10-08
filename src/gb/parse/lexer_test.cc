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
    {LexerFlag::kInt64, LexerFlag::kHexIntegers},
    {LexerFlag::kInt64, LexerFlag::kOctalIntegers},
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
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), '*');
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), "--");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), '/');
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), '+');
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenSymbol);
  EXPECT_EQ(token.GetSymbol(), '-');
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
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 456);
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 789);
  EXPECT_EQ(lexer->NextToken(content).GetType(), kTokenEnd);
}

TEST(LexerTest, ParsePositiveIntegersWithNegative) {
  auto lexer = Lexer::Create({
      .flags = {LexerFlag::kInt64, LexerFlag::kDecimalIntegers},
  });
  ASSERT_NE(lexer, nullptr);
  const LexerContentId content = lexer->AddContent("123 -456");
  Token token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 123);
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorUnexpectedCharacter);
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 456);
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
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), -456);
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
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenError);
  EXPECT_EQ(token.GetString(), Lexer::kErrorInvalidInteger);
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
  EXPECT_EQ(lexer->GetTokenText(token.GetTokenIndex()), "0x123");
  token = lexer->NextToken(content);
  EXPECT_EQ(token.GetType(), kTokenInt);
  EXPECT_EQ(token.GetInt(), 42);
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