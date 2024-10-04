// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_TOKEN_H_
#define GB_PARSE_TOKEN_H_

#include <cstdint>
#include <string_view>

#include "absl/log/check.h"
#include "gb/parse/lexer_types.h"
#include "gb/parse/symbol.h"

namespace gb {

// Token type.
using TokenType = uint8_t;

// Predefined token types.
inline constexpr TokenType kTokenNone = 0;        // Value: kNone
inline constexpr TokenType kTokenEnd = 1;         // Value: kNone
inline constexpr TokenType kTokenError = 2;       // Value: String (error)
inline constexpr TokenType kTokenSymbol = 3;      // Value: Symbol
inline constexpr TokenType kTokenString = 4;      // Value: String
inline constexpr TokenType kTokenInt = 5;         // Value: Int
inline constexpr TokenType kTokenFloat = 6;       // Value: Float
inline constexpr TokenType kTokenIdentifier = 7;  // Value: Index (identifier)
inline constexpr TokenType kTokenKeyword = 8;     // Value: Index (keyword)
inline constexpr TokenType kTokenNewline = 9;     // Value: kNone
inline constexpr TokenType kTokenComment = 10;    // Value: kEnd (comment end)

// The types of underlying values that a token can have.
//
// This is generally implied by the token type, although custom token types may
// vary in underlying value format.
enum class TokenValueType : uint8_t {
  kNone = 0,      // No value (it is just a token type)
  kFloat = 1,     // 64-bit floating point value
  kInt = 2,       // 64-bit integer value
  kString = 3,    // String value
  kIndex = 4,     // Index value (into a table dependent on the token type)
  kSymbol = 5,    // Symbol value (1 to 8 characters)
  kEndIndex = 6,  // TokenIndex to where the token ends (for very long tokens)
};

// A token represents a single parsed token from a lexer.
//
// Tokens are lightweight and can be freely copied and deleted. They are only
// valid as long as the Lexer that created them is still valid.
//
// This class is thread-compatible.
class Token final {
 public:
  Token() = default;
  Token(const Token&) = default;
  Token& operator=(const Token&) = default;
  ~Token() = default;

  // Returns the index of the token within the Lexer.
  TokenIndex GetTokenIndex() const { return token_index_; }

  // Returns the type of the token.
  TokenType GetType() const { return type_; }

  // Returns the type of value stored (this can generally be inferred from the
  // type of the token).
  TokenValueType GetValueType() const { return value_type_; }

  // Returns the value of the token as the specific value type. It is an error
  // to call this if the value type the matching value type.
  int64_t GetInt() const;
  double GetFloat() const;
  std::string_view GetString() const;
  int GetIndex() const;
  Symbol GetSymbol() const;
  TokenIndex GetEndIndex() const;

  // Returns the identifier value of the token. It is an error to call this if
  // the token type is not kIdentifier.
  std::string_view GetIdentifier() const {
    DCHECK(type_ == kTokenIdentifier && value_type_ == TokenValueType::kString);
    return std::string_view(string_, value_size_);
  }

 private:
  friend class Lexer;

  static Token CreateEnd(TokenIndex token_index) {
    return Token(token_index, kTokenEnd);
  }
  static Token CreateError(TokenIndex token_index) {
    return Token(token_index, kTokenError);
  }
  static Token CreateSymbol(TokenIndex token_index, Symbol symbol) {
    Token token(token_index, kTokenSymbol);
    token.symbol_ = symbol.GetValue();
    return token;
  }
  static Token CreateString(TokenIndex token_index, const char* value,
                            uint16_t size) {
    Token token(token_index, kTokenString);
    token.value_size_ = size;
    token.string_ = value;
    return token;
  }
  static Token CreateInt(TokenIndex token_index, int64_t value, uint16_t size) {
    Token token(token_index, kTokenInt);
    token.value_size_ = size;
    token.int_ = value;
    return token;
  }
  static Token CreateFloat(TokenIndex token_index, double value,
                           uint16_t size) {
    Token token(token_index, kTokenFloat);
    token.value_size_ = size;
    token.float_ = value;
    return token;
  }
  static Token CreateIdentifier(TokenIndex token_index, int value) {
    Token token(token_index, kTokenIdentifier);
    token.value_type_ = TokenValueType::kIndex;
    token.index_ = value;
    return token;
  }
  static Token CreateKeyword(TokenIndex token_index, int value) {
    Token token(token_index, kTokenKeyword);
    token.value_type_ = TokenValueType::kIndex;
    token.index_ = value;
    return token;
  }
  static Token CreateNewline(TokenIndex token_index) {
    return Token(token_index, kTokenNewline); 
  }
  static Token CreateComment(TokenIndex token_index, TokenIndex end_index) {
    Token token(token_index, kTokenComment);
    token.value_type_ = TokenValueType::kEndIndex;
    token.end_index_ = end_index;
    return token;
  }

  Token(TokenIndex token_index, TokenType type)
      : token_index_(token_index), type_(type) {}

  TokenIndex token_index_;
  TokenType type_ = kTokenNone;
  TokenValueType value_type_ = TokenValueType::kNone;
  uint16_t value_size_ = 0;
  union {
    int64_t int_ = 0;
    double float_;
    const char* string_;
    int index_;
    SymbolValue symbol_;
    TokenIndex end_index_;
  };
};
static_assert(sizeof(Token) == 16);

inline int64_t Token::GetInt() const {
  DCHECK(value_type_ == TokenValueType::kInt);
  return int_;
}

inline double Token::GetFloat() const {
  DCHECK(value_type_ == TokenValueType::kFloat);
  return float_;
}

inline std::string_view Token::GetString() const {
  DCHECK(value_type_ == TokenValueType::kString);
  return std::string_view(string_, value_size_);
}

inline int Token::GetIndex() const {
  DCHECK(value_type_ == TokenValueType::kIndex);
  return index_;
}

inline Symbol Token::GetSymbol() const {
  DCHECK(type_ == kTokenSymbol && value_type_ == TokenValueType::kIndex);
  return symbol_;
}

inline TokenIndex Token::GetEndIndex() const {
  DCHECK(type_ == kTokenComment && value_type_ == TokenValueType::kEndIndex);
  return end_index_;
}

}  // namespace gb

#endif  // GB_PARSE_TOKEN_H_
