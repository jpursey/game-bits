// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_rules.h"

#include "absl/strings/ascii.h"
#include "gb/parse/lexer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;

std::unique_ptr<ParserGroup> NewValidRule() {
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  return rule;
}

TEST(ParserRulesTest, EmptyRulesAreInvalid) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("no rules"));
}

TEST(ParserRulesTest, EmptyRuleName) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  rules.AddRule("", NewValidRule());
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("rule name"));
}

TEST(ParserRulesTest, EmptyGroupInvalid) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("at least one"));
}

TEST(ParserRulesTest, NullSubItem) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(nullptr);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("null"));
}

TEST(ParserRulesTest, SequenceAllOptional) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt), kParserOptional);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat), kParserZeroOrMore);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString),
                   kParserZeroOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("at least one"));
}

TEST(ParserRulesTest, SequenceOneNotOptional) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt), kParserOptional);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat), kParserZeroOrMore);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString),
                   kParserZeroOrMoreWithComma);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenChar));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, AlternativesOneOptional) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateAlternatives();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat), kParserOptional);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenChar), kParserOneOrMore);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString),
                   kParserOneOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("optional"));
}

TEST(ParserRulesTest, AlternativesOneZeroOrMore) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateAlternatives();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat), kParserZeroOrMore);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenChar), kParserOneOrMore);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString),
                   kParserOneOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("optional"));
}

TEST(ParserRulesTest, AlternativesOneZeroOrMoreWithComma) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateAlternatives();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat),
                   kParserZeroOrMoreWithComma);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenChar), kParserOneOrMore);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString),
                   kParserOneOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("optional"));
}

TEST(ParserRulesTest, AlternativesNoneOptional) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateAlternatives();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenChar), kParserOneOrMore);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString),
                   kParserOneOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, RuleNameItemEmpty) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateRuleName(""));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("empty"));
}

TEST(ParserRulesTest, RuleNameItemNotFound) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateRuleName("other_rule"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("not defined"));
}

TEST(ParserRulesTest, RuleNameValid) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateRuleName("other_rule"));
  rules.AddRule("rule", std::move(rule));
  rules.AddRule("other_rule", NewValidRule());
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenNoneInvalid) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenNone));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenEnd) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenEnd));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenErrorInvalid) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(
      ParserRuleItem::CreateToken(kTokenError, Lexer::kErrorInvalidToken));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenSymbolNoValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("must have a value"));
}

TEST(ParserRulesTest, TokenSymbolWithInvalidValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "123"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenSymbolWithValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "+"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenIntNoValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenIntWithInvalidValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt, "abc"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenIntWithValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt, "123"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenFloatNoValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenFloatWithInvalidValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat, "abc"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenFloatWithValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat, "123.456"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenStringNoValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenStringWithInvalidValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString, "123"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenStringWithValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenString, "\"abc\""));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenCharNoValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenChar));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenCharWithInvalidValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenChar, "123"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenCharWithValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenChar, "'a'"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenKeywordNoValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenKeyword));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("must have a value"));
}

TEST(ParserRulesTest, TokenKeywordWithInvalidValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenKeyword, "123"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenKeywordWithValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenKeyword, "else"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenIdentifierNoValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenIdentifierWithInvalidValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier, "123"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenIdentifierWithValue) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier, "abc"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenUserUndefined) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenUser));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("invalid token"));
}

TEST(ParserRulesTest, TokenUserNoValue) {
  const LexerConfig::UserToken user_tokens[] = {
      {.name = "user-token", .type = kTokenUser, .regex = "\\$([a-z]+)"}};
  LexerConfig config = kCStyleLexerConfig;
  config.user_tokens = user_tokens;
  auto lexer = Lexer::Create(config);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenUser));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenUserWithValue) {
  const LexerConfig::UserToken user_tokens[] = {
      {.name = "user-token", .type = kTokenUser, .regex = "\\$([a-z]+)"}};
  LexerConfig config = kCStyleLexerConfig;
  config.user_tokens = user_tokens;
  auto lexer = Lexer::Create(config);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenUser, "$abc"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, TokenUserWithInvalidValue) {
  const LexerConfig::UserToken user_tokens[] = {
      {.name = "hamburger", .type = kTokenUser, .regex = "\\$([a-z]+)"}};
  LexerConfig config = kCStyleLexerConfig;
  config.user_tokens = user_tokens;
  auto lexer = Lexer::Create(config);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenUser, "$123"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error),
              AllOf(HasSubstr("invalid token"), HasSubstr("hamburger")));
}

TEST(ParserRulesTest, LeftRecursiveSequence) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule1 = ParserRuleItem::CreateSequence();
  rule1->AddSubItem(ParserRuleItem::CreateRuleName("rule2"));
  rules.AddRule("rule1", std::move(rule1));
  auto rule2 = ParserRuleItem::CreateSequence();
  rule2->AddSubItem(ParserRuleItem::CreateRuleName("rule1"));
  rules.AddRule("rule2", std::move(rule2));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("left-recursive"));
}

TEST(ParserRulesTest, RightRecursiveSequence) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule1 = ParserRuleItem::CreateSequence();
  rule1->AddSubItem(ParserRuleItem::CreateRuleName("rule2"));
  rules.AddRule("rule1", std::move(rule1));
  auto rule2 = ParserRuleItem::CreateSequence();
  rule2->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rule2->AddSubItem(ParserRuleItem::CreateRuleName("rule1"));
  rules.AddRule("rule2", std::move(rule2));
  std::string error;
  EXPECT_TRUE(rules.Validate(*lexer, &error)) << "Error: " << error;
  EXPECT_EQ(error, "");
}

TEST(ParserRulesTest, LeftRecursiveSequenceWithLeadingOptional) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule1 = ParserRuleItem::CreateSequence();
  rule1->AddSubItem(ParserRuleItem::CreateToken(kTokenInt), kParserOptional);
  rule1->AddSubItem(ParserRuleItem::CreateToken(kTokenFloat),
                    kParserZeroOrMore);
  rule1->AddSubItem(ParserRuleItem::CreateToken(kTokenString),
                    kParserZeroOrMoreWithComma);
  rule1->AddSubItem(ParserRuleItem::CreateRuleName("rule2"));
  rules.AddRule("rule1", std::move(rule1));
  auto rule2 = ParserRuleItem::CreateSequence();
  rule2->AddSubItem(ParserRuleItem::CreateRuleName("rule1"));
  rules.AddRule("rule2", std::move(rule2));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("left-recursive"));
}

TEST(ParserRulesTest, RecursiveAlternates) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  ParserRules rules;
  auto rule1 = ParserRuleItem::CreateAlternatives();
  rule1->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rule1->AddSubItem(ParserRuleItem::CreateRuleName("rule2"));
  rules.AddRule("rule1", std::move(rule1));
  auto rule2 = ParserRuleItem::CreateAlternatives();
  rule2->AddSubItem(ParserRuleItem::CreateToken(kTokenString),
                    kParserOneOrMore);
  rule2->AddSubItem(ParserRuleItem::CreateRuleName("rule1"));
  rules.AddRule("rule2", std::move(rule2));
  std::string error;
  EXPECT_FALSE(rules.Validate(*lexer, &error));
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("left-recursive"));
}

}  // namespace
}  // namespace gb