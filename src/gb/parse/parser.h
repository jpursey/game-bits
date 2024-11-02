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

// This class is used to parse a sequence of tokens into a parse tree based on a
// set of rules.
//
// The parser is created with a lexer or lexer configuration and a set of rules,
// and then Parse is called to parse a sequence of tokens into a parse tree.
//
// The parser itself is a greedy semi-predictive recursive-descent parser
// (https://en.wikipedia.org/wiki/Recursive_descent_parser). Specifically, it
// is only semi-predictive as it will greedily accept the *first* match in a
// group of alternatives, even if a later match would be longer or result in a
// successful parse. Further, in a sequence all optional items in a group that
// match are greedily accepted (there is no backtracking within a group). This
// makes the "dangling else" problem trivial to resolve in the normal way, as it
// will be greedily accepted as part of the closeset "if" statement if it
// matches.
//
// Also, being a recursive-descent parser, left recursion is not allowed (which
// makes binary expression recursion always right-associative by default).
// However, if this is required for a language, each precendence level can
// instead be represented as a repeating group, leaving left/right association
// to the caller after parsing.
//
// This class is thread-compatible.
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

  // Returns the lexer used by this parser.
  const Lexer& GetLexer() const { return lexer_; }
  Lexer& GetLexer() { return lexer_; }

  // Parses the specified rule from the current lexer content, starting at the
  // current token within the content.
  //
  // If the rule is not found, or the rule does not match, a parse error is
  // returned and the lexer content is not advanced. If the rule is matched, a
  // parse tree is returned and the lexer content is advanced past the matched
  // tokens.
  ParseResult Parse(LexerContentId content, std::string_view rule);

 private:
  friend class ParserToken;
  friend class ParserRuleName;
  friend class ParserGroup;

  Parser(Lexer& lexer, ParserRules rules)
      : lexer_(lexer), rules_(std::move(rules)) {}

  ParseError Error(gb::Token token, std::string_view message);

  Callback<ParseError()> TokenErrorCallback(gb::Token token,
                                            TokenType expected_type,
                                            TokenValue expected_value);

  ParseMatch MatchTokenItem(const ParserToken& item);
  ParseMatch MatchGroup(const ParserGroup& item);
  ParseMatch MatchRuleItem(const ParserRuleName& item);

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
