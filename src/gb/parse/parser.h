// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_PARSER_H_
#define GB_PARSE_PARSER_H_

#include <memory>
#include <string_view>

#include "gb/parse/lexer.h"
#include "gb/parse/parse_result.h"
#include "gb/parse/parse_types.h"
#include "gb/parse/parser_rules.h"
#include "gb/parse/token.h"

namespace gb {

class Parser final {
 public:
  // Creates a parser with the specified lexer, lexer configuration and parser
  // rules. If an error string is provided, it will be set to the error message.
  static std::unique_ptr<Parser> Create(LexerConfig config, ParserRules rules,
                                        std::string* error_message = nullptr);
  static std::unique_ptr<Parser> Create(std::unique_ptr<Lexer> lexer,
                                        ParserRules rules,
                                        std::string* error_message = nullptr);
  static std::unique_ptr<Parser> Create(Lexer& lexer, ParserRules rules,
                                        std::string* error_message = nullptr);

  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;
  ~Parser() = default;

  const Lexer& GetLexer() const { return lexer_; }
  Lexer& GetLexer() { return lexer_; }

  ParseResult Parse(LexerContentId content, std::string_view rule);

  ParseMatch MatchTokenItem(ParserInternal internal, const ParserToken& item);
  ParseMatch MatchGroup(ParserInternal internal, const ParserGroup& item);
  ParseMatch MatchRuleItem(ParserInternal internal, const ParserRuleName& item);

 private:
  Parser(Lexer& lexer, ParserRules rules)
      : lexer_(lexer), rules_(std::move(rules)) {}

  ParseError Error(gb::Token token, std::string_view message);

  Callback<ParseError()> TokenErrorCallback(gb::Token token,
                                            TokenType expected_type,
                                            TokenValue expected_value);

  Token NextToken() { return lexer_.NextToken(content_); }
  Token PeekToken() { return lexer_.NextToken(content_, false); }
  void SetNextToken(Token token) { lexer_.SetNextToken(token); }

  std::unique_ptr<Lexer> owned_lexer_;
  Lexer& lexer_;
  const ParserRules rules_;
  LexerContentId content_ = kNoLexerContent;
};

}  // namespace gb

#endif  // GB_PARSE_PARSER_H_
