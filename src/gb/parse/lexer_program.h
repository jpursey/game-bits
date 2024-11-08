// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_LEXER_PROGRAM_H_
#define GB_PARSE_LEXER_PROGRAM_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "gb/parse/lexer_config.h"
#include "gb/parse/token.h"
#include "re2/re2.h"

namespace gb {

//==============================================================================
// LexerProgram
//==============================================================================

// This class represents a complete lexer program, which is built from a lexer
// configuration. The lexer program is used to create a lexer that can tokenize
// a sequence of characters into a sequence of tokens.
class LexerProgram final {
 public:
  //----------------------------------------------------------------------------
  // Lexer initialization error strings
  //
  // These may be returned by Lexer::Create if the configuration is invalid.
  //----------------------------------------------------------------------------

  // Duplicate symbol specification in LexerConfig.
  static const std::string_view kErrorDuplicateSymbolSpec;

  // Invalid symbol specification in LexerConfig.
  static const std::string_view kErrorInvalidSymbolSpec;

  // Conflicting configuration between strings and characters in LexerConfig
  static const std::string_view kErrorConflictingStringAndCharSpec;

  // Conflicting identifier configuration in LexerConfig.
  static const std::string_view kErrorConflictingIdentifierSpec;

  // Conflicting line and block comment configuration in LexerConfig.
  static const std::string_view kErrorConflictingCommentSpec;

  // Empty string in comment specifications in LexerConfig.
  static const std::string_view kErrorEmptyCommentSpec;

  // Duplicate string in keyword specifications in LexerConfig.
  static const std::string_view kErrorDuplicateKeywordSpec;

  // Empty string in keyword specifications in LexerConfig.
  static const std::string_view kErrorEmptyKeywordSpec;

  // Invalid token specification in LexerConfig (no symbols, keywords, or other
  // tokens).
  static const std::string_view kErrorNoTokenSpec;

  // Invalid user token type in LexerConfig. It must be >= kTokenUser.
  static const std::string_view kErrorInvalidUserTokenType;

  // Invalid user token regex in LexerConfig. It must be a valid RE2 regex, and
  // it must have a single capture group.
  static const std::string_view kErrorInvalidUserTokenRegex;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a lexer program from the specified lexer configuration. If an error
  // string is provided, it will be set to the error message.
  static std::unique_ptr<const LexerProgram> Create(
      LexerConfig config, std::string* error_message = nullptr);

  LexerProgram(const LexerProgram&) = delete;
  LexerProgram& operator=(const LexerProgram&) = delete;
  ~LexerProgram() = default;

  //----------------------------------------------------------------------------
  // Private implementation
  //----------------------------------------------------------------------------

 private:
  friend class Lexer;

  struct TokenConfig {
    int prefix = 0;
    int size_offset = 0;
  };

  enum class IntParseType {
    kDefault,
    kHex,
    kOctal,
    kBinary,
  };

  struct TokenArgInfo {
    TokenArgInfo() {}
    TokenType type = kTokenNone;
    IntParseType int_parse_type = IntParseType::kDefault;
  };

  struct BlockComment {
    std::string start;
    std::string end;
  };

  struct State {
    LexerFlags flags;
    std::unique_ptr<RE2> re_whitespace;
    std::unique_ptr<RE2> re_symbol;
    std::unique_ptr<RE2> re_token_end;
    std::unique_ptr<RE2> re_not_token_end;
    std::unique_ptr<RE2> re_token;
    std::vector<TokenArgInfo> re_args;
    TokenConfig binary_config;
    TokenConfig octal_config;
    TokenConfig decimal_config;
    TokenConfig hex_config;
    TokenConfig float_config;
    TokenConfig ident_config;
    int64_t max_int = std::numeric_limits<int64_t>::max();
    int64_t min_int = std::numeric_limits<int64_t>::min();
    uint64_t int_sign_extend = 0;
    char escape = 0;
    char escape_newline = 0;
    char escape_tab = 0;
    char escape_hex = 0;
    std::vector<BlockComment> block_comments;
    TokenTypeNames user_token_names;
    absl::flat_hash_map<std::string, std::string> keywords;
  };

  explicit LexerProgram(State& state) : state_(std::move(state)) {}

  const State state_;
};

}  // namespace gb

#endif  // GB_PARSE_LEXER_PROGRAM_H_
