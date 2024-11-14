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
  EXPECT_THAT(error, HasSubstr(LexerProgram::kErrorNoTokenSpec));
}

TEST(ParserProgramTest, NullLexerProgram) {
  std::string error;
  auto program = ParserProgram::Create(nullptr, "program { %int; }", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("lexer"));

  program = ParserProgram::Create(std::unique_ptr<LexerProgram>(),
                                  "program { %int; }", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("lexer"));
}

TEST(ParserProgramTest, InvalidRules) {
  std::string error;
  auto program =
      ParserProgram::Create(kCStyleLexerConfig, "program { %int }", &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("';'"));

  auto lexer_program = LexerProgram::Create(kCStyleLexerConfig, &error);
  ASSERT_NE(lexer_program, nullptr) << "Error: " << error;

  program = ParserProgram::Create(std::move(lexer_program), "program { %int }",
                                  &error);
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

  auto lexer_program = LexerProgram::Create(kCStyleLexerConfig, &error);
  ASSERT_NE(lexer_program, nullptr) << "Error: " << error;

  auto program2 = ParserProgram::Create(std::move(lexer_program),
                                        "program { %string; }", &error);
  const auto& rules3 = program2->GetRules();
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

TEST(ParserProgramTest, ValidRuleName) {
  std::string error;
  std::string program_text = R"(
    program {
      rule1 <rule2>;
    }
    rule1 {
      %int %float;
    }
    rule2 {
      %ident "=" %string;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(), "rule1 rule2");
  EXPECT_EQ(rules.GetRule("rule1")->ToString(), "%int %float");
  EXPECT_EQ(rules.GetRule("rule2")->ToString(), "%ident \"=\" %string");
}

TEST(ParserProgramTest, InvalidRuleName) {
  std::string error;
  std::string program_text = R"(
    program {
      rule1 rule2;
    }
    rule1 {
      %int %float;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error),
              AllOf(HasSubstr("rule2"), HasSubstr("not defined")));
}

TEST(ParserProgramTest, OptionalItem) {
  std::string error;
  std::string program_text = R"(
    program {
      [%char] %int;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(), "[%char] %int");
}

TEST(ParserProgramTest, OptionalItemInvalid) {
  std::string error;
  std::string program_text = R"(
    program {
      [%unknown];
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("%unknown"));
}

TEST(ParserProgramTest, GroupItem) {
  std::string error;
  std::string program_text = R"(
    program {
      (%int %float) %char;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(), "(%int %float) %char");
}

TEST(ParserProgramTest, GroupItemInvalid) {
  std::string error;
  std::string program_text = R"(
    program {
      (%int %unknown);
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("%unknown"));
}

TEST(ParserProgramTest, RepeatOptions) {
  std::string error;
  std::string program_text = R"(
    program {
      %int* %float+ %char,* %string,+;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(),
            "%int* %float+ %char,* %string,+");
}

TEST(ParserProgramTest, OptionalOverridesOneOrMore) {
  std::string error;
  std::string program_text = R"(
    program {
      [%int]+ [%float],+ %string;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(), "(%int)* (%float),* %string");
}

TEST(ParserProgramTest, MultipleOptions) {
  std::string error;
  std::string program_text = R"(
    program {
      %int %float;
      %char %string;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(),
            "(%int %float) | (%char %string)");
}

TEST(ParserProgramTest, SecondOptionInvalid) {
  std::string error;
  std::string program_text = R"(
    program {
      %int %float;
      %char %unknown;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("%unknown"));
}

TEST(ParserProgramTest, Alternatives) {
  std::string error;
  std::string program_text = R"(
    program {
      %int | %float | (%char %string) | %ident "=" %string;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(),
            "%int | %float | (%char %string) | (%ident \"=\" %string)");
}

TEST(ParserProgramTest, AlternativesWithErrorInSequence) {
  std::string error;
  std::string program_text = R"(
    program {
      %int | %float | (%char %string) | %ident "=" %unknown;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  EXPECT_EQ(program, nullptr);
  EXPECT_THAT(error, HasSubstr("%unknown"));
}

TEST(ParserProgramTest, AlternativesAsOptions) {
  std::string error;
  std::string program_text = R"(
    program {
      %int | %float;
      %char | %string;
    }
  )";
  auto program = ParserProgram::Create(kCStyleLexerConfig,
                                       std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(),
            "(%int | %float) | (%char | %string)");
}

TEST(ParserProgramTest, SelfParse) {
  std::string error;
  std::string program_text = R"---(
    %token_type = 0;
    %match_name = 1;
    program {
      ($tokens=token_def | $rules=rule_def)* %end;
    }
    token_def {
      $name=%token_type "=" $value=%int ";";
    }
    rule_def {
      $name=%ident "{" ($options=group_alternative ";")+ "}";
    }
    group_alternative {
      $items=group_sequence ("|" $items=group_sequence)*;
    }
    group_sequence {
      $items=group_item+;
    }
    group_item {
      [$match_name=%match_name "="]
      $item=group_item_inner
      $repeat=["+" | "*" | ",+" | ",*"];
    }
    group_item_inner {
      $token=%token_type;
      $literal=%string;
      $scoped_rule=%ident;
      '<' $unscoped_rule=%ident '>';
      '[' $optional=group_alternative ']';
      '(' $group=group_alternative ')';
    }
   )---";
  const LexerConfig::UserToken kProgramUserTokens[] = {
      {.name = "token type",
       .type = kTokenUser + 0,
       .regex = R"-(\%([a-zA-Z]\w*))-"},
      {.name = "match name",
       .type = kTokenUser + 1,
       .regex = R"-(\$([a-zA-Z]\w*))-"},
  };
  const Symbol kProgramSymbols[] = {
      '+', '-', '*', '/', '~', '&', '|', '^', '!', '<', '>', '=',  '.', ',',
      ';', ':', '?', '#', '@', '(', ')', '[', ']', '{', '}', ",*", ",+"};
  LexerConfig config = {
      .flags = {kLexerFlags_CIdentifiers, LexerFlag::kInt8,
                LexerFlag::kDecimalIntegers, LexerFlag::kDoubleQuoteString,
                LexerFlag::kSingleQuoteString, LexerFlag::kDecodeEscape},
      .escape = '\\',
      .escape_newline = 'n',
      .escape_tab = 't',
      .escape_hex = 'x',
      .line_comments = kCStyleLineComments,
      .symbols = kProgramSymbols,
      .user_tokens = kProgramUserTokens,
  };
  auto program = ParserProgram::Create(config, std::move(program_text), &error);
  ASSERT_NE(program, nullptr) << "Error: " << error;
  const auto& rules = program->GetRules();
  EXPECT_EQ(rules.GetRule("program")->ToString(),
            "($tokens=token_def | $rules=rule_def)* %end");
  EXPECT_EQ(rules.GetRule("token_def")->ToString(),
            "$name=%0 \"=\" $value=%int \";\"");
  EXPECT_EQ(rules.GetRule("rule_def")->ToString(),
            "$name=%ident \"{\" ($options=group_alternative \";\")+ \"}\"");
  EXPECT_EQ(rules.GetRule("group_alternative")->ToString(),
            "$items=group_sequence (\"|\" $items=group_sequence)*");
  EXPECT_EQ(rules.GetRule("group_sequence")->ToString(), "$items=group_item+");
  EXPECT_EQ(rules.GetRule("group_item")->ToString(),
            "[$match_name=%1 \"=\"] $item=group_item_inner "
            "$repeat=[\"+\" | \"*\" | \",+\" | \",*\"]");
  EXPECT_EQ(rules.GetRule("group_item_inner")->ToString(),
            "$token=%0 | $literal=%string | $scoped_rule=%ident | "
            "(\"<\" $unscoped_rule=%ident \">\") | "
            "(\"[\" $optional=group_alternative \"]\") | "
            "(\"(\" $group=group_alternative \")\")");
}

}  // namespace
}  // namespace gb
