// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_PARSER_PROGRAM_H_
#define GB_PARSE_PARSER_PROGRAM_H_

#include <memory>
#include <string>

#include "gb/parse/lexer_program.h"
#include "gb/parse/parse_types.h"
#include "gb/parse/parser_rules.h"

namespace gb {

//==============================================================================
// ParserProgram
//==============================================================================

// This class represents a complete parser program, which includes a lexer and
// a set of rules that define the grammar of the language being parsed.
//
// Parser rules are defined as a text with the following syntax:
//   rule-name {
//     rule-item... ;
//     ...
//     rule-item... ;
//   }
//
// User token types can be defined in the lexer specification, and can be
// referenced in the parser rules as %token-type once defined:
//   %token-type = int-value ;
//
// Each rule-item can be a token type, a token literal, a rule name, or a
// group of rule-items enclosed in parenthesis (required) or square brackets
// (optional). Token types can be one of: %int, %float, %string, %char, %ident.
// For example:
//   %int                 Matches any integer token.
//   "token"              Matches the token literal "token".
//   'token'              Also matches the token literal "token".
//   rule_name            Matches the rule named "rule_name".
//   (%int "," %int)      Matches two integer tokens separated by a comma.
//   [%ident "=" %float]  Matches an optional identifier assigned with a float.
//
// Each rule-item can be further annotated with a name, which is used to
// identify the matched item in the parse result, and a repeat specifier, which
// can be '*' (zero or more), ',*' (zero or more separated by commas),
// '+' (one or more), or ',+' (one or more separated by commas. For example:
//   $name=%ident      Assigns the matched identifier to the name "name".
//   [rule_name]       Matches the rule named "rule_name" zero or one times.
//   (statement ";")*  Matches zero or more statements separated by semicolons.
//   %int,+            Matches one or more integers separated by commas.
//
// Matching a named rule by default scopes all named sub-items of the rule
// (requiring it to be assigned a name in the parent to access any named
// sub-items). However, it can optionally be set to promote any named sub-items
// to the parent scope by prefixing enclosing in angle brackets. For example:
//   rule_name         Scopes all named sub-items of "rule_name".
//   <rule_name>       Promotes named sub-items of "rule_name" to this rule.
//
// Finally, rule-items can be combined with '|' to indicate alternatives, and
// with ' ' to indicate sequence. For example:
//   %int | %float          Matches an integer or a float.
//   %int %float | %string  Matches an integer followed by a float, or a string.
class ParserProgram {
 public:
  // Creates a parser program with the specified lexer specification and program
  // text. This returns null if the ParserProgram was invalid (lexer or program
  // specification). If an error string is provided, it will be set to the error
  // message on failure.
  static std::unique_ptr<ParserProgram> Create(
      LexerConfig config, std::string_view program_text,
      std::string* error_message = nullptr);
  static std::unique_ptr<ParserProgram> Create(
      std::shared_ptr<const LexerProgram> lexer_program,
      std::string_view program_text, std::string* error_message = nullptr);

  ParserProgram(const ParserProgram&) = delete;
  ParserProgram& operator=(const ParserProgram&) = delete;
  ~ParserProgram() = default;

  const LexerProgram& GetLexerProgram() const { return *lexer_program_; }
  const ParserRules& GetRules() const { return *rules_; }

 private:
  friend class Parser;

  ParserProgram(std::shared_ptr<const LexerProgram> lexer_program,
                std::shared_ptr<const ParserRules> rules)
      : lexer_program_(std::move(lexer_program)), rules_(std::move(rules)) {}

  std::shared_ptr<const LexerProgram> lexer_program_;
  std::shared_ptr<const ParserRules> rules_;
};

}  // namespace gb

#endif  // GB_PARSE_PARSER_PROGRAM_H_
