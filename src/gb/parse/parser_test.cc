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
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some text");
  Parser parser(*lexer, ParserRules());
  ParseResult result = parser.Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("\"rule\""));
}

TEST(ParserTest, NoContent) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some text");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content + 1, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr(Lexer::kErrorInvalidTokenContent));

  result = parser.Parse(kNoLexerContent, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr(Lexer::kErrorInvalidTokenContent));
}

TEST(ParserTest, EmptySequenceMatches) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some text");

  ParserRules rules;
  rules.AddRule("rule", ParserRuleItem::CreateSequence());
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsNone()) << result->GetToken();
  EXPECT_TRUE(lexer->NextToken(content, false).IsIdent("some"))
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceSingleIdent) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some text");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(lexer->NextToken(content, false).IsIdent("text"))
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceOptionalIdent) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some text");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOptional);
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(lexer->NextToken(content, false).IsIdent("text"))
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceOneOrMore) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some text");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOneOrMore);
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(lexer->NextToken(content, false).IsEnd()) << result->GetToken();
}

TEST(ParserTest, MatchSequenceZeroOrMore) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some text");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserZeroOrMore);
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(lexer->NextToken(content, false).IsEnd()) << result->GetToken();
}

TEST(ParserTest, MatchSequenceOneOrMoreWithComma) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some, text");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOneOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(lexer->NextToken(content, false).IsEnd()) << result->GetToken();
}

TEST(ParserTest, MatchSequenceOneOrMoreWithCommaFails) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some, 42");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserOneOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected identifier"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 6));
  EXPECT_TRUE(lexer->NextToken(content, false).IsIdent("some"))
      << result->GetToken();
}

TEST(ParserTest, MatchSequenceZeroOrMoreWithComma) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some, text");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserZeroOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsIdent("some")) << result->GetToken();
  EXPECT_TRUE(lexer->NextToken(content, false).IsEnd()) << result->GetToken();
}

TEST(ParserTest, MatchSequenceZeroOrMoreWithCommaFails) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some, 42");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenIdentifier),
                   kParserZeroOrMoreWithComma);
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected identifier"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 6));
  EXPECT_TRUE(lexer->NextToken(content, false).IsIdent("some"))
      << result->GetToken();
}

TEST(ParserTest, EmptyAlternativesMatches) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("some text");

  ParserRules rules;
  rules.AddRule("rule", ParserRuleItem::CreateAlternatives());
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_TRUE(result->GetToken().IsNone()) << result->GetToken();
  EXPECT_TRUE(lexer->NextToken(content, false).IsIdent("some"))
      << result->GetToken();
}

TEST(ParserTest, MatchTokenTypeSuccess) {
  static constexpr std::string_view keywords[] = {"if", "else", "while"};
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.keywords = keywords;
  auto lexer = Lexer::Create(config);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content =
      lexer->AddContent("42 3.14 'c' \"hello\"\nelse name;");

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
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
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
  auto lexer = Lexer::Create(config);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content =
      lexer->AddContent("42 3.14 'c' \"hello\"\nelse name;");
  const Token int_token = lexer->NextToken(content);
  EXPECT_EQ(int_token.GetType(), kTokenInt);
  const Token float_token = lexer->NextToken(content);
  EXPECT_EQ(float_token.GetType(), kTokenFloat);
  const Token char_token = lexer->NextToken(content);
  EXPECT_EQ(char_token.GetType(), kTokenChar);
  const Token string_token = lexer->NextToken(content);
  EXPECT_EQ(string_token.GetType(), kTokenString);
  const Token line_break_token = lexer->NextToken(content);
  EXPECT_EQ(line_break_token.GetType(), kTokenLineBreak);
  const Token keyword_token = lexer->NextToken(content);
  EXPECT_EQ(keyword_token.GetType(), kTokenKeyword);
  const Token identifier_token = lexer->NextToken(content);
  EXPECT_EQ(identifier_token.GetType(), kTokenIdentifier);
  const Token symbol_token = lexer->NextToken(content);
  EXPECT_EQ(symbol_token.GetType(), kTokenSymbol);

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
  Parser parser(*lexer, std::move(rules));

  lexer->SetNextToken(float_token);
  ParseResult result = parser.Parse(content, "int");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected integer"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 3));

  lexer->SetNextToken(char_token);
  result = parser.Parse(content, "float");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr("Expected floating-point"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 8));

  lexer->SetNextToken(string_token);
  result = parser.Parse(content, "char");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected character"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 12));

  lexer->SetNextToken(line_break_token);
  result = parser.Parse(content, "string");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr("Expected string value"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 19));

  lexer->SetNextToken(keyword_token);
  result = parser.Parse(content, "line_break");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(),
              HasSubstr("Expected end of line"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 1, 0));

  lexer->SetNextToken(identifier_token);
  result = parser.Parse(content, "keyword");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected keyword"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 1, 5));

  lexer->SetNextToken(symbol_token);
  result = parser.Parse(content, "identifier");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected identifier"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 1, 9));

  lexer->SetNextToken(int_token);
  result = parser.Parse(content, "symbol");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("Expected symbol"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 0));
}

TEST(ParserTest, MatchTokenTypeAndValueSuccess) {
  static constexpr std::string_view keywords[] = {"if", "else", "while"};
  LexerConfig config = kCStyleLexerConfig;
  config.flags.Set(LexerFlag::kLineBreak);
  config.keywords = keywords;
  auto lexer = Lexer::Create(config);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content =
      lexer->AddContent("42 3.14 'c' \"hello\" else name;");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenInt, "42"));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenFloat, "3.14"));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenChar, "c"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenString, "hello"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenKeyword, "else"));
  rule->AddSubItem("tokens",
                   ParserRuleItem::CreateToken(kTokenIdentifier, "name"));
  rule->AddSubItem("tokens", ParserRuleItem::CreateToken(kTokenSymbol, ";"));
  rules.AddRule("rule", std::move(rule));
  Parser parser(*lexer, std::move(rules));

  ParseResult result = parser.Parse(content, "rule");
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
  auto lexer = Lexer::Create(config);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content =
      lexer->AddContent("42 3.14 'c' \"hello\" else name;");
  const Token int_token = lexer->NextToken(content);
  EXPECT_EQ(int_token.GetType(), kTokenInt);
  const Token float_token = lexer->NextToken(content);
  EXPECT_EQ(float_token.GetType(), kTokenFloat);
  const Token char_token = lexer->NextToken(content);
  EXPECT_EQ(char_token.GetType(), kTokenChar);
  const Token string_token = lexer->NextToken(content);
  EXPECT_EQ(string_token.GetType(), kTokenString);
  const Token keyword_token = lexer->NextToken(content);
  EXPECT_EQ(keyword_token.GetType(), kTokenKeyword);
  const Token identifier_token = lexer->NextToken(content);
  EXPECT_EQ(identifier_token.GetType(), kTokenIdentifier);
  const Token symbol_token = lexer->NextToken(content);
  EXPECT_EQ(symbol_token.GetType(), kTokenSymbol);

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
  Parser parser(*lexer, std::move(rules));

  lexer->SetNextToken(int_token);
  ParseResult result = parser.Parse(content, "int");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("43"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 0));

  lexer->SetNextToken(float_token);
  result = parser.Parse(content, "float");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("3.15"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 3));

  lexer->SetNextToken(char_token);
  result = parser.Parse(content, "char");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("'d'"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 8));

  lexer->SetNextToken(string_token);
  result = parser.Parse(content, "string");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("\"world\""));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 12));

  lexer->SetNextToken(keyword_token);
  result = parser.Parse(content, "keyword");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("while"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 20));

  lexer->SetNextToken(identifier_token);
  result = parser.Parse(content, "identifier");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("grape"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 25));

  lexer->SetNextToken(symbol_token);
  result = parser.Parse(content, "symbol");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("'+'"));
  EXPECT_THAT(result.GetError().GetLocation(), IsLocation(content, 0, 29));
}

TEST(ParserTest, MatchRuleNameSuccess) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("(some, text)");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("ident", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "("));
  rule->AddSubItem("first", ParserRuleItem::CreateRule("ident"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ","));
  rule->AddSubItem("second", ParserRuleItem::CreateRule("ident"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ")"));
  rules.AddRule("pair", std::move(rule));

  Parser parser(*lexer, std::move(rules));
  ParseResult result = parser.Parse(content, "pair");
  ASSERT_TRUE(result.IsOk()) << result.GetError().FormatMessage();
  EXPECT_THAT(result->GetItem("first"),
              ElementsAre(IsToken(kTokenIdentifier, "some")));
  EXPECT_THAT(result->GetItem("second"),
              ElementsAre(IsToken(kTokenIdentifier, "text")));
  EXPECT_TRUE(lexer->NextToken(content, false).IsEnd()) << result->GetToken();
}

TEST(ParserTest, MatchRuleNameFail) {
  auto lexer = Lexer::Create(kCStyleLexerConfig);
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("(some, text)");

  ParserRules rules;
  auto rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem("ident", ParserRuleItem::CreateToken(kTokenIdentifier));
  rules.AddRule("ident", std::move(rule));
  rule = ParserRuleItem::CreateSequence();
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "("));
  rule->AddSubItem("first", ParserRuleItem::CreateRule("ident"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ","));
  rule->AddSubItem("second", ParserRuleItem::CreateRule("invalid"));
  rule->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ")"));
  rules.AddRule("pair", std::move(rule));

  Parser parser(*lexer, std::move(rules));
  ParseResult result = parser.Parse(content, "pair");
  ASSERT_FALSE(result.IsOk());
  EXPECT_THAT(result.GetError().GetMessage(), HasSubstr("\"invalid\""));
  EXPECT_EQ(result.GetError().GetLocation(), LexerLocation());
  EXPECT_TRUE(lexer->NextToken(content, false).IsSymbol('('))
      << result->GetToken();
}

}  // namespace
}  // namespace gb