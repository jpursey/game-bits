// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser.h"

#include "absl/strings/ascii.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

MATCHER_P3(IsLocation, content, line, column, "") {
  return arg.id == content && arg.line == line && arg.column == column;
}
MATCHER_P2(IsToken, type, value, "") {
  Token token = arg.GetToken();
  return token.GetType() == type && token.ToString() == value;
}

ParserRules ValidParserRules() {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("rule", std::move(rule));
  return rules;
}

TEST(ParserTest, InvalidLexerConfig) {
  std::string error;
  auto parser = Parser::Create(LexerConfig(), ValidParserRules(), &error);
  ASSERT_EQ(parser, nullptr);
  EXPECT_THAT(error, HasSubstr(Lexer::kErrorNoTokenSpec));
}

TEST(ParserTest, NullLexer) {
  std::string error;
  auto parser = Parser::Create(nullptr, ValidParserRules(), &error);
  ASSERT_EQ(parser, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("lexer is null"));
}

TEST(ParserTest, NoRules) {
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, ParserRules(), &error);
  ASSERT_EQ(parser, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("no rules"));
}

TEST(ParserTest, InvalidRulesWithSharedLexer) {
  ParserRules rules;
  rules.AddRule("rule", ParserRuleItem::CreateSequence());
  std::string error;
  auto lexer = Lexer::Create(kCStyleLexerConfig, &error);
  ASSERT_NE(lexer, nullptr) << "Error: " << error;
  auto parser = Parser::Create(*lexer, std::move(rules), &error);
  ASSERT_EQ(parser, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("at least one"));
}

TEST(ParserTest, NoContent) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some text");

  ParseResult result = parser->Parse(content + 1, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr(Lexer::kErrorInvalidTokenContent));

  result = parser->Parse(kNoLexerContent, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr(Lexer::kErrorInvalidTokenContent));
}

TEST(ParserTest, EmptySequenceInvalid) {
  ParserRules rules;
  rules.AddRule("rule", ParserRuleItem::CreateSequence());
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_EQ(parser, nullptr);
  EXPECT_THAT(absl::AsciiStrToLower(error), HasSubstr("at least one"));
}

TEST(ParserTest, UndefinedInitialRule) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some text");

  ParseResult result = parser->Parse(content, "undefined");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("\"undefined\""));
}

TEST(ParserTest, MatchSequenceSingleIdent) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto lexer = Lexer::Create(kCStyleLexerConfig, &error);
  ASSERT_NE(lexer, nullptr) << "Error: " << error;
  auto parser = Parser::Create(*lexer, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some text");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsIdent("text"))
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceOptionalIdent) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOptional);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some 42 text");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsIdent("text"))
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceOneOrMore) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOneOrMore);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some text");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd())
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceZeroOrMore) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserZeroOrMore);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some text 42");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd())
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceOneOrMoreWithComma) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOneOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some, text");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd())
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceOneOrMoreWithCommaFails) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOneOrMoreWithComma);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenEnd));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some, 42");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected identifier"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 6));
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsIdent("some"))
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceZeroOrMoreWithComma) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserZeroOrMoreWithComma);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some, text 42");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd())
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceZeroOrMoreWithCommaFails) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserZeroOrMoreWithComma);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some, 42");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected identifier"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 6));
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsIdent("some"))
      << result->GetToken();
}

TEST(ParserTest, EmptyAlternativesMatches) {
  ParserRules rules;
  rules.AddRule("rule", ParserRuleItem::CreateAlternatives());
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_EQ(parser, nullptr);
  EXPECT_THAT(error, HasSubstr("at least one"));
}

TEST(ParserTest, MatchTokenTypeSuccess) {
  const LexerConfig::UserToken user_tokens[] = {
      {.name = "forty-two", .type = kTokenUser + 42, .regex = "\\$(42)"},
  };
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.user_tokens = user_tokens;
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenInt));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenFloat));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenChar));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenString));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenLineBreak));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenIdentifier));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenUser + 42));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(config, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content =
      parser->GetLexer().AddContent("42 3.14 'c' \"hello\"\nname $42");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsInt(42)) << result->GetToken();
  EXPECT_THAT(
      result->GetItems("tokens"),
      ElementsAre(IsToken(kTokenInt, "42"), IsToken(kTokenFloat, "3.14"),
                  IsToken(kTokenChar, "c"), IsToken(kTokenString, "hello"),
                  IsToken(kTokenLineBreak, ""),
                  IsToken(kTokenIdentifier, "name"),
                  IsToken(kTokenUser + 42, "42")));
}

TEST(ParserTest, MatchTokenTypeFail) {
  const LexerConfig::UserToken user_tokens[] = {
      {.name = "forty-two", .type = kTokenUser + 42, .regex = "\\$(42)"},
  };
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.user_tokens = user_tokens;
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("int", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenFloat));
  rules.AddRule("float", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenChar));
  rules.AddRule("char", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenString));
  rules.AddRule("string", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenLineBreak));
  rules.AddRule("line_break", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("identifier", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenUser + 42));
  rules.AddRule("user", std::move(rule));
  std::string error;
  auto parser = Parser::Create(config, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;

  LexerContentId content =
      parser->GetLexer().AddContent("42 3.14 'c' \"hello\"\nname $42");
  const Token int_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(int_token.GetType(), kTokenInt);
  const Token float_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(float_token.GetType(), kTokenFloat);
  const Token char_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(char_token.GetType(), kTokenChar);
  const Token string_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(string_token.GetType(), kTokenString);
  const Token line_break_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(line_break_token.GetType(), kTokenLineBreak);
  const Token identifier_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(identifier_token.GetType(), kTokenIdentifier);
  const Token user_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(user_token.GetType(), kTokenUser + 42);

  parser->GetLexer().SetNextToken(float_token);
  ParseResult result = parser->Parse(content, "int");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected integer"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 3));

  parser->GetLexer().SetNextToken(char_token);
  result = parser->Parse(content, "float");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected floating-point"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 8));

  parser->GetLexer().SetNextToken(string_token);
  result = parser->Parse(content, "char");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected character"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 12));

  parser->GetLexer().SetNextToken(line_break_token);
  result = parser->Parse(content, "string");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected string value"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 19));

  parser->GetLexer().SetNextToken(identifier_token);
  result = parser->Parse(content, "line_break");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected end of line"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 1, 0));

  parser->GetLexer().SetNextToken(user_token);
  result = parser->Parse(content, "identifier");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected identifier"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 1, 5));

  parser->GetLexer().SetNextToken(int_token);
  result = parser->Parse(content, "user");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected forty-two"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 0));
}

TEST(ParserTest, MatchTokenTypeAndValueSuccess) {
  const LexerConfig::UserToken user_tokens[] = {
      {.name = "forty-two", .type = kTokenUser + 42, .regex = "\\$(42)"},
  };
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.user_tokens = user_tokens;
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenInt, "42"));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenFloat, "3.14"));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenChar, "'c'"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenString, "\"hello\""));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenKeyword, "else"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenIdentifier, "name"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenUser + 42, "$42"));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenSymbol, ";"));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(config, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content =
      parser->GetLexer().AddContent("42 3.14 'c' \"hello\" else name $42;");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsInt(42)) << result->GetToken();
  EXPECT_THAT(
      result->GetItems("tokens"),
      ElementsAre(IsToken(kTokenInt, "42"), IsToken(kTokenFloat, "3.14"),
                  IsToken(kTokenChar, "c"), IsToken(kTokenString, "hello"),
                  IsToken(kTokenKeyword, "else"),
                  IsToken(kTokenIdentifier, "name"),
                  IsToken(kTokenUser + 42, "42"), IsToken(kTokenSymbol, ";")));
}

TEST(ParserTest, MatchTokenTypeAndValueFail) {
  const LexerConfig::UserToken user_tokens[] = {
      {.name = "forty-something", .type = kTokenUser, .regex = "\\$(4[0-9])"},
  };
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.user_tokens = user_tokens;
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenInt, "43"));
  rules.AddRule("int", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenFloat, "3.15"));
  rules.AddRule("float", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenChar, "'d'"));
  rules.AddRule("char", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token",
                   ParserRuleItem::CreateToken(kTokenString, "\"world\""));
  rules.AddRule("string", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token",
                   ParserRuleItem::CreateToken(kTokenKeyword, "while"));
  rules.AddRule("keyword", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token",
                   ParserRuleItem::CreateToken(kTokenIdentifier, "grape"));
  rules.AddRule("identifier", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenUser, "$42"));
  rules.AddRule("user", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenSymbol, "+"));
  rules.AddRule("symbol", std::move(rule));
  std::string error;
  auto parser = Parser::Create(config, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;

  LexerContentId content =
      parser->GetLexer().AddContent("42 3.14 'c' \"hello\" else name $43;");
  const Token int_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(int_token.GetType(), kTokenInt);
  const Token float_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(float_token.GetType(), kTokenFloat);
  const Token char_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(char_token.GetType(), kTokenChar);
  const Token string_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(string_token.GetType(), kTokenString);
  const Token keyword_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(keyword_token.GetType(), kTokenKeyword);
  const Token identifier_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(identifier_token.GetType(), kTokenIdentifier);
  const Token user_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(user_token.GetType(), kTokenUser);
  const Token symbol_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(symbol_token.GetType(), kTokenSymbol);

  parser->GetLexer().SetNextToken(int_token);
  ParseResult result = parser->Parse(content, "int");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("43"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 0));

  parser->GetLexer().SetNextToken(float_token);
  result = parser->Parse(content, "float");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("3.15"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 3));

  parser->GetLexer().SetNextToken(char_token);
  result = parser->Parse(content, "char");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("'d'"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 8));

  parser->GetLexer().SetNextToken(string_token);
  result = parser->Parse(content, "string");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("\"world\""));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 12));

  parser->GetLexer().SetNextToken(keyword_token);
  result = parser->Parse(content, "keyword");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("while"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 20));

  parser->GetLexer().SetNextToken(identifier_token);
  result = parser->Parse(content, "identifier");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("grape"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 25));

  parser->GetLexer().SetNextToken(user_token);
  result = parser->Parse(content, "user");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("$43"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 30));

  parser->GetLexer().SetNextToken(symbol_token);
  result = parser->Parse(content, "symbol");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("'+'"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 33));
}

TEST(ParserTest, MatchErrorTokenAsInt) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenIdentifier));
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("name 4rty2");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr(Lexer::kErrorInvalidToken));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 5));
}

TEST(ParserTest, MatchRuleNameSuccess) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("ident", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "("));
  rule->AddSubItem("first", ParserRuleItem::CreateRuleName("ident"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ","));
  rule->AddSubItem("second", ParserRuleItem::CreateRuleName("ident"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ")"));
  rules.AddRule("pair", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("(some, text)");

  ParseResult result = parser->Parse(content, "pair");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_THAT(result->GetItems("first"),
              ElementsAre(IsToken(kTokenIdentifier, "some")));
  EXPECT_THAT(result->GetItems("second"),
              ElementsAre(IsToken(kTokenIdentifier, "text")));
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd())
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceFirstItemOptional) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOptional);
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("42");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsInt(42)) << result->GetToken();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd())
      << result->GetToken();
}

TEST(ParserTest, MatchAlternatesFirstItemInvalid) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateAlternatives();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier));
  rule->AddSubItem("int", ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("42");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk());
  EXPECT_TRUE(result->GetToken().IsInt(42)) << result->GetToken();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd())
      << result->GetToken();
}

TEST(ParserTest, MatchAlternatesAllItemsInvalid) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateAlternatives();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier));
  rule->AddSubItem("int", ParserRuleItem::CreateToken(kTokenInt));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("while");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(absl::AsciiStrToLower(result.GetError().GetMessage()),
              HasSubstr("expected integer"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 0));
}

TEST(ParserTest, MatchAlternativesCommaList) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateAlternatives();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier));
  rule->AddSubItem("list", ParserRuleItem::CreateToken(kTokenInt),
                   kParserOneOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("fun 42, 3, 25 hello");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_THAT(result->GetItems("list"), IsEmpty());
  EXPECT_THAT(result->GetItems("ident"),
              ElementsAre(IsToken(kTokenIdentifier, "fun")));
  result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_THAT(result->GetItems("list"),
              ElementsAre(IsToken(kTokenInt, "42"), IsToken(kTokenInt, "3"),
                          IsToken(kTokenInt, "25")));
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsIdent("hello"))
      << result->GetToken();
}

TEST(ParserTest, InlineGroupsMergeNamedSubItems) {
  auto numbers = ParserRuleItem::CreateAlternatives();
  numbers->AddSubItem("value", ParserRuleItem::CreateToken(kTokenInt));
  numbers->AddSubItem("value", ParserRuleItem::CreateToken(kTokenFloat));

  auto int_assign = ParserRuleItem::CreateSequence();
  int_assign->AddSubItem("name", ParserRuleItem::CreateToken(kTokenIdentifier));
  int_assign->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "="));
  int_assign->AddSubItem(std::move(numbers));

  auto function_call = ParserRuleItem::CreateSequence();
  function_call->AddSubItem("function",
                            ParserRuleItem::CreateToken(kTokenIdentifier));
  function_call->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "("));
  function_call->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ")"));

  auto statement_select = ParserRuleItem::CreateAlternatives();
  statement_select->AddSubItem(std::move(int_assign));
  statement_select->AddSubItem(std::move(function_call));

  auto statement = ParserRuleItem::CreateSequence();
  statement->AddSubItem("statements", std::move(statement_select));
  statement->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ";"));

  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(std::move(statement), kParserOneOrMore);

  ParserRules rules;
  rules.AddRule("rule", std::move(rule));

  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent(
      "a = 42;\n"
      "fun();\n"
      "b = 3.14;\n");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd());
  auto parsed_statements = result->GetItems("statements");
  ASSERT_THAT(parsed_statements, ElementsAre(IsToken(kTokenIdentifier, "a"),
                                             IsToken(kTokenIdentifier, "fun"),
                                             IsToken(kTokenIdentifier, "b")));
  EXPECT_EQ(parsed_statements[0].GetString("name"), "a");
  EXPECT_EQ(parsed_statements[0].GetInt("value"), 42);
  EXPECT_EQ(parsed_statements[1].GetString("function"), "fun");
  EXPECT_EQ(parsed_statements[2].GetString("name"), "b");
  EXPECT_EQ(parsed_statements[2].GetFloat("value"), 3.14);
}

TEST(ParserTest, ParserProgram) {
  const std::string_view kProgram = R"---(
    Program {
      $statements=Statement+ %end;
    }
    Statement {
      $if=("if" "(" $condition=Expression ")" $then=Statement
          ["else" $else=Statement]);
      $while=("while" "(" $condition=Expression ")" $body=Statement);
      $assign=($lvalue=%ident "=" $rvalue=Expression ";");
      $call=($function=%ident "(" $arguments=Expression,* ")" ";");
      "{" $statements=Statement* "}";
      ";";
    }
    Expression {
      $expr=Expression2
      [$op=("==" | "!=" | "<" | "<=" | ">" | ">=") $expr=Expression2]+;
    }
    Expression2 {
      $expr=Expression3 
      [$op=("+" | "-") $expr=Expression3]+;
    }
    Expression3 {
      $expr=Term
      [$op=("*" | "/" | "%") $expr=Term]+;
    }
    Term {
      $var=%ident;
      $value=(%int | %float | %string | %char);
      "(" $expr=Expression ")";
    }
  )---";
  std::string error;
  auto parser = Parser::Create(
      ParserProgram::Create(kCStyleLexerConfig, kProgram, &error));
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent(R"---(
    i = 0;
    while (i < 10) {
      if (i % 2 == 0) print(i);
      else if (b * c + e * f / g - h == 42) print("woah");
      else print("odd");
      i = i + 1;
    }
  )---");

  ParseResult result = parser->Parse(content, "Program");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd());
  auto statements = result->GetItems("statements");
  ASSERT_EQ(statements.size(), 2);
  auto statement = statements[0].GetItem("assign");
  ASSERT_NE(statement, nullptr);
  EXPECT_EQ(statement->GetString("lvalue"), "i");
  EXPECT_EQ(statement->GetInt("rvalue"), 0);

  statement = statements[1].GetItem("while");
  ASSERT_NE(statement, nullptr);

  auto condition = statement->GetItem("condition");
  ASSERT_NE(condition, nullptr);
  auto expr2 = condition->GetItems("expr");  // ((i)) < ((10))
  ASSERT_EQ(expr2.size(), 2);
  auto expr3 = expr2[0].GetItems("expr");  // (i)
  ASSERT_EQ(expr3.size(), 1);
  auto term = expr3[0].GetItems("expr");  // i
  ASSERT_EQ(term.size(), 1);
  EXPECT_EQ(term[0].GetString("var"), "i");
  expr3 = expr2[1].GetItems("expr");  // (10)
  ASSERT_EQ(expr3.size(), 1);
  term = expr3[0].GetItems("expr");  // 10
  ASSERT_EQ(term.size(), 1);
  EXPECT_EQ(term[0].GetInt("value"), 10);
  auto op = condition->GetItems("op");  // <
  ASSERT_EQ(op.size(), 1);
  EXPECT_TRUE(op[0].GetToken().IsSymbol("<"));

  auto body = statement->GetItem("body");
  ASSERT_NE(body, nullptr);
  auto body_statements = body->GetItems("statements");
  ASSERT_EQ(body_statements.size(), 2);

  auto if_statement = body_statements[0].GetItem("if");
  ASSERT_NE(if_statement, nullptr);
  condition = if_statement->GetItem("condition");
  ASSERT_NE(condition, nullptr);
  expr2 = condition->GetItems("expr");  // ((i % 2)) == ((0))
  ASSERT_EQ(expr2.size(), 2);
  expr3 = expr2[0].GetItems("expr");  // (i % 2) == (0)
  ASSERT_EQ(expr3.size(), 1);
  term = expr3[0].GetItems("expr");  // i % 2
  ASSERT_EQ(term.size(), 2);
  EXPECT_EQ(term[0].GetString("var"), "i");
  EXPECT_EQ(term[1].GetInt("value"), 2);
  op = expr3[0].GetItems("op");  // %
  ASSERT_EQ(op.size(), 1);
  EXPECT_TRUE(op[0].GetToken().IsSymbol("%"));
  expr3 = expr2[1].GetItems("expr");  // (0)
  ASSERT_EQ(expr3.size(), 1);
  term = expr3[0].GetItems("expr");  // 0
  ASSERT_EQ(term.size(), 1);
  EXPECT_EQ(term[0].GetInt("value"), 0);
  op = condition->GetItems("op");
  ASSERT_EQ(op.size(), 1);
  EXPECT_TRUE(op[0].GetToken().IsSymbol("=="));

  statement = if_statement->GetItem("then");
  ASSERT_NE(statement, nullptr);
  auto call = statement->GetItem("call");
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->GetString("function"), "print");
  auto arguments = call->GetItems("arguments");
  ASSERT_EQ(arguments.size(), 1);

  statement = if_statement->GetItem("else");
  ASSERT_NE(statement, nullptr);
  if_statement = statement->GetItem("if");
  ASSERT_NE(if_statement, nullptr);
  condition = if_statement->GetItem("condition");
  ASSERT_NE(condition, nullptr);
  expr2 =
      condition->GetItems("expr");  // ((b * c) + (e * f / g) - (h)) == ((42))
  ASSERT_EQ(expr2.size(), 2);
  expr3 = expr2[0].GetItems("expr");  // (b * c) + (e * f / g) - (h)
  ASSERT_EQ(expr3.size(), 3);
  term = expr3[0].GetItems("expr");  // b * c
  ASSERT_EQ(term.size(), 2);
  EXPECT_EQ(term[0].GetString("var"), "b");
  EXPECT_EQ(term[1].GetString("var"), "c");
  op = expr3[0].GetItems("op");  // *
  ASSERT_EQ(op.size(), 1);
  EXPECT_TRUE(op[0].GetToken().IsSymbol("*"));
  term = expr3[1].GetItems("expr");  // e * f / g
  ASSERT_EQ(term.size(), 3);
  EXPECT_EQ(term[0].GetString("var"), "e");
  EXPECT_EQ(term[1].GetString("var"), "f");
  EXPECT_EQ(term[2].GetString("var"), "g");
  op = expr3[1].GetItems("op");  // * /
  ASSERT_EQ(op.size(), 2);
  EXPECT_TRUE(op[0].GetToken().IsSymbol("*"));
  EXPECT_TRUE(op[1].GetToken().IsSymbol("/"));
  term = expr3[2].GetItems("expr");  // h
  ASSERT_EQ(term.size(), 1);
  EXPECT_EQ(term[0].GetString("var"), "h");
  expr3 = expr2[1].GetItems("expr");  // (42)
  ASSERT_EQ(expr3.size(), 1);
  term = expr3[0].GetItems("expr");  // 42
  ASSERT_EQ(term.size(), 1);
  EXPECT_EQ(term[0].GetInt("value"), 42);
  op = condition->GetItems("op");
  ASSERT_EQ(op.size(), 1);
  EXPECT_TRUE(op[0].GetToken().IsSymbol("=="));

  statement = if_statement->GetItem("then");
  ASSERT_NE(statement, nullptr);
  call = statement->GetItem("call");
  ASSERT_NE(call, nullptr);

  statement = if_statement->GetItem("else");
  ASSERT_NE(statement, nullptr);
  call = statement->GetItem("call");
  ASSERT_NE(call, nullptr);

  statement = body_statements[1].GetItem("assign");
  ASSERT_NE(statement, nullptr);
}

}  // namespace
}  // namespace gb