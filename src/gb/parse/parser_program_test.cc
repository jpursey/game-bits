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

using ::testing::AllOf;
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
  EXPECT_THAT(error, HasSubstr("';'"));

  auto lexer = Lexer::Create(kCStyleLexerConfig, &error);
  ASSERT_NE(lexer, nullptr) << "Error: " << error;

  program = ParserProgram::Create(lexer.get(), "program { %int }", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("';'"));

  program = ParserProgram::Create(std::move(lexer), "program { %int }", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("';'"));
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

  auto program2 =
      ParserProgram::Create(lexer.get(), "program { %float; }", &error);
  const auto& rules2 = program2->GetRules();
  EXPECT_EQ(rules2.GetRule("program")->ToString(), "%float");

  auto program3 =
      ParserProgram::Create(std::move(lexer), "program { %string; }", &error);
  const auto& rules3 = program3->GetRules();
  EXPECT_EQ(rules3.GetRule("program")->ToString(), "%string");
}

TEST(ParserProgramTest, NoRules) {
  const LexerConfig::UserToken user_tokens[] = {
      {"alpha", kTokenUser + 1, "$([a-z]+)"},
  };
  LexerConfig config = kCStyleLexerConfig;
  config.user_tokens = user_tokens;

  std::string error;
  auto program = ParserProgram::Create(config, "", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("no rules"));

  program = ParserProgram::Create(config, "%alpha = 1;", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("no rules"));
}

TEST(ParserProgramTest, ValidTokenTypes) {
  std::string error;
  std::string program_text = R"(
    %alpha = 1;
    %beta = 2;
    program {
      %int %float %char %string %ident %linebreak %alpha %beta %end;
    }
  )";
  const LexerConfig::UserToken user_tokens[] = {
      {"alpha", kTokenUser + 1, "$([a-z]+)"},
      {"beta", kTokenUser + 2, "@(a-z]+)"},
  };
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.user_tokens = user_tokens;
  auto program = ParserProgram::Create(config, std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(),
            "%int %float %char %string %ident %linebreak %1 %2 %end");
}

TEST(ParserProgramTest, TokenNotValidInLexer) {
  std::string error;
  std::string program_text = R"(
    program {
      %int %float %linebreak;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("%linebreak"));
}

TEST(ParserProgramTest, MismatchedUserTokenValue) {
  std::string error;
  std::string program_text = R"(
    %alpha = 0;
    %beta = 2;
    program {
      %int %float %char %string %ident %linebreak %alpha %beta %end;
    }
  )";
  const LexerConfig::UserToken user_tokens[] = {
      {"alpha", kTokenUser + 1, "$([a-z]+)"},
      {"beta", kTokenUser + 2, "@([a-z]+)"},
  };
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.user_tokens = user_tokens;
  auto program = ParserProgram::Create(config, std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("%alpha"));
}

TEST(ParserProgramTest, DuplicateTokenDefinition) {
  std::string error;
  std::string program_text = R"(
    %alpha = 1;
    %alpha = 2;
    program {
      %int %float %char %string %ident %linebreak %alpha %end;
    }
  )";
  const LexerConfig::UserToken user_tokens[] = {
      {"alpha", kTokenUser + 1, "$([a-z]+)"},
  };
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.user_tokens = user_tokens;
  auto program = ParserProgram::Create(config, std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error),
              AllOf(HasSubstr("%alpha"), HasSubstr("duplicate")));
}

TEST(ParserProgramTest, UndefinedTokenType) {
  std::string error;
  std::string program_text = R"(
    program {
      %int %float %char %string %ident %linebreak %alpha %end;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error),
              AllOf(HasSubstr("%alpha"), HasSubstr("unknown")));
}

TEST(ParserProgramTest, ValidTokenLiterals) {
  std::string error;
  std::string program_text = R"(
    program {
      "15" "0xdeadbeef" "0777" "0b10101" "3.125" "2e3"
      '"some string"' "'c'" "var_name" "while";
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(),
            "\"15\" \"0xdeadbeef\" \"0777\" "
            "\"0b10101\" \"3.125\" \"2e3\" "
            "'\"some string\"' \"'c'\" "
            "\"var_name\" \"while\"");
}

TEST(ParserProgramTest, InvalidTokenLiteral) {
  std::string error;
  std::string program_text = R"(
    program {
      "15" "name" "while" "0zero0";
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error),
              AllOf(HasSubstr("0zero0"), HasSubstr("literal")));
}

}  // namespace
}  // namespace gb
