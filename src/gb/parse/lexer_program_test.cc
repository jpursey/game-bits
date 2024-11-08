// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer_program.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(LexerProgramTest, DefaultConfigIsInvalid) {
  LexerConfig config;
  std::string error;
  auto program = LexerProgram::Create(config, &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorNoTokenSpec);
}

TEST(LexerProgramTest, SuccessfulCreateClearsError) {
  LexerConfig config;
  config.flags = {
      LexerFlag::kInt64,
      LexerFlag::kDecimalIntegers,
  };
  std::string error = "test error";
  auto program = LexerProgram::Create(config, &error);
  EXPECT_NE(program, nullptr);
  EXPECT_EQ(error, "");
}

TEST(LexerProgramTest, InvalidSymbolCharacters) {
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
    auto program = LexerProgram::Create({.symbols = {symbol}}, &error);
    EXPECT_EQ(program, nullptr) << context;
    EXPECT_EQ(error, LexerProgram::kErrorInvalidSymbolSpec) << context;
  }
}

TEST(LexerProgramTest, DuplicateSymbols) {
  std::string error;
  auto program =
      LexerProgram::Create({.symbols = {"*", "++", "++", "-"}}, &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorDuplicateSymbolSpec);
}

TEST(LexerProgramTest, ConflictingStringAndCharSpecs) {
  std::string error;
  auto program =
      LexerProgram::Create({.flags = {LexerFlag::kDoubleQuoteString,
                                      LexerFlag::kDoubleQuoteCharacter}},
                           &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorConflictingStringAndCharSpec);

  program = LexerProgram::Create({.flags = {LexerFlag::kSingleQuoteString,
                                            LexerFlag::kSingleQuoteCharacter}},
                                 &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorConflictingStringAndCharSpec);
}

TEST(LexerProgramTest, EmptyStringKeywordSpecifications) {
  std::string error;
  auto program =
      LexerProgram::Create({.keywords = {"if", "", "while"}}, &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorEmptyKeywordSpec);
}

TEST(LexerProgramTest, DuplicateKeywordSpecifications) {
  std::string error;
  auto program = LexerProgram::Create(
      {.keywords = {"if", "else", "else", "while"}}, &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorDuplicateKeywordSpec);
}

TEST(LexerProgramTest, ConflictingForceUpperAndLower) {
  std::string error;
  auto program = LexerProgram::Create(
      {.flags = {LexerFlag::kIdentForceUpper, LexerFlag::kIdentForceLower}},
      &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorConflictingIdentifierSpec);
}

TEST(LexerProgramTest, ConflictingCommentSpecifications) {
  std::string error;

  auto program = LexerProgram::Create(
      {.flags = kLexerFlags_CIdentifiers, .line_comments = {"//", "#", "//"}},
      &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorConflictingCommentSpec);

  program = LexerProgram::Create(
      {.flags = kLexerFlags_CIdentifiers,
       .block_comments = {{"/*", "*/"}, {"$", "$"}, {"/*", "*/"}}},
      &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorConflictingCommentSpec);

  program = LexerProgram::Create({.flags = kLexerFlags_CIdentifiers,
                                  .line_comments = {"#"},
                                  .block_comments = {{"/*", "*/"}, {"#", "#"}}},
                                 &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorConflictingCommentSpec);
}

TEST(LexerProgramTest, EmptyStringCommentSpecifications) {
  std::string error;

  auto program = LexerProgram::Create(
      {.flags = kLexerFlags_CIdentifiers, .line_comments = {"//", ""}}, &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorEmptyCommentSpec);

  program = LexerProgram::Create({.flags = kLexerFlags_CIdentifiers,
                                  .block_comments = {{"/*", "*/"}, {""}}},
                                 &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_EQ(error, LexerProgram::kErrorEmptyCommentSpec);
}

}  // namespace
}  // namespace gb
