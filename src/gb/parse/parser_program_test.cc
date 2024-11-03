// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_program.h"

#include "absl/strings/ascii.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::HasSubstr;

TEST(ParserProgramTest, InvalidLexerConfig) {
  std::string error;
  auto program =
      ParserProgram::Create(LexerConfig{}, "program { %int; }", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr(Lexer::kErrorNoTokenSpec));
}

TEST(ParserProgramTest, NullLexer) {
  std::string error;
  auto program = ParserProgram::Create(nullptr, "program { %int; }", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("lexer"));

  program = ParserProgram::Create(std::unique_ptr<Lexer>(), "program { %int; }",
                                  &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("lexer"));
}

TEST(ParserProgramTest, InvalidRules) {
  std::string error;
  auto program =
      ParserProgram::Create(kCStyleLexerConfig, "program { %int }", &error);
  EXPECT_EQ(program, nullptr);
  //EXPECT_THAT(error, HasSubstr("';'"));

  auto lexer = Lexer::Create(kCStyleLexerConfig, &error);
  ASSERT_NE(lexer, nullptr) << "Error: " << error;

  program = ParserProgram::Create(lexer.get(), "program { %int }", &error);
  EXPECT_EQ(program, nullptr);
  //EXPECT_THAT(error, HasSubstr("';'"));

  program = ParserProgram::Create(std::move(lexer), "program { %int }", &error);
  EXPECT_EQ(program, nullptr);
  //EXPECT_THAT(error, HasSubstr("';'"));
}

TEST(ParserProgramTest, ValidRules) {
  std::string error;
  auto program =
      ParserProgram::Create(kCStyleLexerConfig, "program { %int; }", &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(), "%int");

  auto lexer = Lexer::Create(kCStyleLexerConfig, &error);
  ASSERT_NE(lexer, nullptr) << "Error: " << error;

  auto program2 = ParserProgram::Create(lexer.get(), "program { %float; }", &error);
  const auto& rules2 = program2->GetRules();
  EXPECT_EQ(rules2.GetRule("program")->ToString(), "%float");

  auto program3 =
      ParserProgram::Create(std::move(lexer), "program { %string; }", &error);
  const auto& rules3 = program3->GetRules();
  EXPECT_EQ(rules3.GetRule("program")->ToString(), "%string");
}

}  // namespace
}  // namespace gb
