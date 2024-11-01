// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;

MATCHER_P3(IsLocation, content, line, column, "") {
  return arg.id == content && arg.line == line && arg.column == column;
}
MATCHER_P2(IsToken, type, value, "") {
  Token token = arg.GetToken();
  return token.GetType() == type && token.ToString() == value;
}

TEST(ParserTest, NoRules) {
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, ParserRules(), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some text");
  ParseResult result = parser->Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("\"rule\""));
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
  EXPECT_THAT(error, HasSubstr("at least one"));
}

TEST(ParserTest, MatchSequenceSingleIdent) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
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
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("some, 42");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected identifier"));
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
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected identifier"));
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
  static constexpr std::string_view keywords[] = {"if", "else", "while"};
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.keywords = keywords;
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenInt));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenFloat));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenChar));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenString));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenLineBreak));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenKeyword));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenIdentifier));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenSymbol));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(config, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content =
      parser->GetLexer().AddContent("42 3.14 'c' \"hello\"\nelse name;");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsInt(42)) << result->GetToken();
  EXPECT_THAT(
      result->GetItem("tokens"),
      ElementsAre(IsToken(kTokenInt, "42"), IsToken(kTokenFloat, "3.14"),
                  IsToken(kTokenChar, "c"), IsToken(kTokenString, "hello"),
                  IsToken(kTokenLineBreak, ""), IsToken(kTokenKeyword, "else"),
                  IsToken(kTokenIdentifier, "name"),
                  IsToken(kTokenSymbol, ";")));
}

TEST(ParserTest, MatchTokenTypeFail) {
  static constexpr std::string_view keywords[] = {"if", "else", "while"};
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.keywords = keywords;
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
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenKeyword));
  rules.AddRule("keyword", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("identifier", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenSymbol));
  rules.AddRule("symbol", std::move(rule));
  std::string error;
  auto parser = Parser::Create(config, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;

  LexerContentId content =
      parser->GetLexer().AddContent("42 3.14 'c' \"hello\"\nelse name;");
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
  const Token keyword_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(keyword_token.GetType(), kTokenKeyword);
  const Token identifier_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(identifier_token.GetType(), kTokenIdentifier);
  const Token symbol_token = parser->GetLexer().NextToken(content);
  EXPECT_EQ(symbol_token.GetType(), kTokenSymbol);

  parser->GetLexer().SetNextToken(float_token);
  ParseResult result = parser->Parse(content, "int");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected integer"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 3));

  parser->GetLexer().SetNextToken(char_token);
  result = parser->Parse(content, "float");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr("Expected floating-point"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 8));

  parser->GetLexer().SetNextToken(string_token);
  result = parser->Parse(content, "char");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected character"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 12));

  parser->GetLexer().SetNextToken(line_break_token);
  result = parser->Parse(content, "string");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr("Expected string value"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 19));

  parser->GetLexer().SetNextToken(keyword_token);
  result = parser->Parse(content, "line_break");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr("Expected end of line"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 1, 0));

  parser->GetLexer().SetNextToken(identifier_token);
  result = parser->Parse(content, "keyword");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected keyword"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 1, 5));

  parser->GetLexer().SetNextToken(symbol_token);
  result = parser->Parse(content, "identifier");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected identifier"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 1, 9));

  parser->GetLexer().SetNextToken(int_token);
  result = parser->Parse(content, "symbol");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected symbol"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 0));
}

TEST(ParserTest, MatchTokenTypeAndValueSuccess) {
  static constexpr std::string_view keywords[] = {"if", "else", "while"};
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.keywords = keywords;
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenInt, 42));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenFloat, 3.14));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenChar, "c"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenString, "hello"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenKeyword, "else"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenIdentifier, "name"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenSymbol, Symbol(";")));
  rules.AddRule("rule", std::move(rule));
  std::string error;
  auto parser = Parser::Create(config, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content =
      parser->GetLexer().AddContent("42 3.14 'c' \"hello\" else name;");

  ParseResult result = parser->Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsInt(42)) << result->GetToken();
  EXPECT_THAT(
      result->GetItem("tokens"),
      ElementsAre(IsToken(kTokenInt, "42"), IsToken(kTokenFloat, "3.14"),
                  IsToken(kTokenChar, "c"), IsToken(kTokenString, "hello"),
                  IsToken(kTokenKeyword, "else"),
                  IsToken(kTokenIdentifier, "name"),
                  IsToken(kTokenSymbol, ";")));
}

TEST(ParserTest, MatchTokenTypeAndValueFail) {
  static constexpr std::string_view keywords[] = {"if", "else", "while"};
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.keywords = keywords;
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenInt, "43"));
  rules.AddRule("int", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenFloat, "3.15"));
  rules.AddRule("float", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenChar, "d"));
  rules.AddRule("char", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenString, "world"));
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
  rule->AddSubItem("token", ParserRuleItem::CreateToken(kTokenSymbol, "+"));
  rules.AddRule("symbol", std::move(rule));
  std::string error;
  auto parser = Parser::Create(config, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;

  LexerContentId content =
      parser->GetLexer().AddContent("42 3.14 'c' \"hello\" else name;");
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
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("'d'"));
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

  parser->GetLexer().SetNextToken(symbol_token);
  result = parser->Parse(content, "symbol");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("'+'"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 29));
}

TEST(ParserTest, MatchRuleNameSuccess) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("ident", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, Symbol("(")));
  rule->AddSubItem("first", ParserRuleItem::CreateRule("ident"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, Symbol(",")));
  rule->AddSubItem("second", ParserRuleItem::CreateRule("ident"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, Symbol(")")));
  rules.AddRule("pair", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("(some, text)");

  ParseResult result = parser->Parse(content, "pair");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_THAT(result->GetItem("first"),
              ElementsAre(IsToken(kTokenIdentifier, "some")));
  EXPECT_THAT(result->GetItem("second"),
              ElementsAre(IsToken(kTokenIdentifier, "text")));
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsEnd())
      << result->GetToken();
}

TEST(ParserTest, MatchRuleNameFail) {
  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("ident", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, Symbol("(")));
  rule->AddSubItem("first", ParserRuleItem::CreateRule("ident"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, Symbol(",")));
  rule->AddSubItem("second", ParserRuleItem::CreateRule("invalid"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, Symbol(")")));
  rules.AddRule("pair", std::move(rule));
  std::string error;
  auto parser = Parser::Create(kCStyleLexerConfig, std::move(rules), &error);
  ASSERT_NE(parser, nullptr) << "Error: " << error;
  LexerContentId content = parser->GetLexer().AddContent("(some, text)");

  ParseResult result = parser->Parse(content, "pair");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("\"invalid\""));
  EXPECT_EQ(result.GetError().GetLocation(), LexerLocation());
  EXPECT_TRUE(parser->GetLexer().NextToken(content, false).IsSymbol('('))
      << result->GetToken();
}

}  // namespace
}  // namespace gb