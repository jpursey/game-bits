// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_TOKEN_H_
#define GB_PARSE_TOKEN_H_

#include <cstdint>
#include <optional>
#include <string_view>

#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "gb/parse/lexer_types.h"
#include "gb/parse/symbol.h"

namespace gb {

// Token type.
using TokenType = uint8_t;

// Predefined token types.
inline constexpr TokenType kTokenNone = 0;        // Value: kNone
inline constexpr TokenType kTokenEnd = 1;         // Value: kNone
inline constexpr TokenType kTokenError = 2;       // Value: String
inline constexpr TokenType kTokenSymbol = 3;      // Value: Symbol
inline constexpr TokenType kTokenString = 4;      // Value: String
inline constexpr TokenType kTokenInt = 5;         // Value: Int
inline constexpr TokenType kTokenFloat = 6;       // Value: Float
inline constexpr TokenType kTokenIdentifier = 7;  // Value: String
inline constexpr TokenType kTokenKeyword = 8;     // Value: Index and String
inline constexpr TokenType kTokenNewline = 9;     // Value: kNone

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

  // Returns the index of the token within the Lexer. This is a more compact way
  // to store the token (4 bytes instead of 16 bytes), but requires re-parsing
  // by the Lexer to get the token value again (via Lexer::GetToken).
  TokenIndex GetTokenIndex() const { return token_index_; }

  // Returns the type of the token.
  TokenType GetType() const { return type_; }

  // Returns the typed value of the token or a default value if it isn't valid
  // for the token type. Some types (like kTokenEnd) have no value, and other
  // types (like kTokenKeyword) has both an index and a string value. See the
  // specific token types above for details on the valid value types for each
  // predefined token type.
  int64_t GetInt() const;              // Default: 0
  double GetFloat() const;             // Default: 0.0
  std::string_view GetString() const;  // Default: empty
  int GetIndex() const;                // Default: -1
  Symbol GetSymbol() const;            // Default: Symbol()

  // Comparison operators.
  auto operator<=>(const Token& other) const {
    if (auto cmp = token_index_ <=> other.token_index_; cmp != 0) {
      return cmp;
    }
    if (auto cmp = type_ <=> other.type_; cmp != 0) {
      return cmp;
    }
    if (type_ == kTokenError) {
      return *string_view_ <=> *other.string_view_;
    }
    return 1 <=> 1;
  }
  bool operator==(const Token& other) const {
    return token_index_ == other.token_index_ && type_ == other.type_ &&
           (type_ != kTokenError || *string_view_ == *other.string_view_);
  }

 private:
  friend class Lexer;

  // Maps to the type in the union
  enum class ValueType : uint8_t {
    kNone,        // No value (it is just a token type)
    kFloat,       // float_ and sizeof_ are valid
    kInt,         // int_ and sizeof_ are valid
    kString,      // string_ and strlen_ are valid
    kStringView,  // string_view_ is valid
    kIndex,       // index_ and string_view_ are valid
    kSymbol,      // symbol_ is valid
  };

  static Token CreateEnd(TokenIndex token_index) {
    return Token(token_index, kTokenEnd, ValueType::kNone);
  }
  static Token CreateError(TokenIndex token_index,
                           const std::string_view* value) {
    Token token(token_index, kTokenError, ValueType::kStringView);
    token.string_view_ = value;
    return token;
  }
  static Token CreateSymbol(TokenIndex token_index, Symbol symbol) {
    Token token(token_index, kTokenSymbol, ValueType::kSymbol);
    token.symbol_ = symbol.GetValue();
    return token;
  }
  static Token CreateString(TokenIndex token_index, const char* value,
                            uint16_t size) {
    Token token(token_index, kTokenString, ValueType::kString);
    token.strlen_ = size;
    token.string_ = value;
    return token;
  }
  static Token CreateString(TokenIndex token_index,
                            const std::string_view* value) {
    Token token(token_index, kTokenString, ValueType::kStringView);
    token.string_view_ = value;
    return token;
  }
  static Token CreateInt(TokenIndex token_index, int64_t value, uint16_t size) {
    Token token(token_index, kTokenInt, ValueType::kInt);
    token.sizeof_ = size;
    token.int_ = value;
    return token;
  }
  static Token CreateFloat(TokenIndex token_index, double value,
                           uint16_t size) {
    Token token(token_index, kTokenFloat, ValueType::kFloat);
    token.sizeof_ = size;
    token.float_ = value;
    return token;
  }
  static Token CreateIdentifier(TokenIndex token_index, const char* value,
                                uint16_t size) {
    Token token(token_index, kTokenIdentifier, ValueType::kString);
    token.strlen_ = size;
    token.string_ = value;
    return token;
  }
  static Token CreateIdentifier(TokenIndex token_index,
                                const std::string_view* value) {
    Token token(token_index, kTokenIdentifier, ValueType::kStringView);
    token.string_view_ = value;
    return token;
  }
  static Token CreateKeyword(TokenIndex token_index, int index,
                             std::string_view* indexed_string) {
    Token token(token_index, kTokenKeyword, ValueType::kIndex);
    token.value_type_ = ValueType::kIndex;
    token.index_ = index;
    token.string_view_ = indexed_string;
    return token;
  }
  static Token CreateNewline(TokenIndex token_index) {
    return Token(token_index, kTokenNewline, ValueType::kNone);
  }

  Token(TokenIndex token_index, TokenType type, ValueType value_type)
      : token_index_(token_index), type_(type), value_type_(value_type) {}

  TokenIndex token_index_;
  TokenType type_ = kTokenNone;
  ValueType value_type_ = ValueType::kNone;
  union {
    uint16_t sizeof_ = 0;  // For types int and float.
    uint16_t strlen_;      // For type string.
    uint16_t index_;       // For type index.
  };
  union {
    int64_t int_ = 0;
    double float_;
    const char* string_;
    const std::string_view* string_view_;
    SymbolValue symbol_;
  };
};
static_assert(sizeof(Token) == 16);

template <typename Sink>
void AbslStringify(Sink& sink, const Token& token) {
  absl::Format(&sink, "{%v, type:%d, value:", token.GetTokenIndex(),
               token.GetType());
  switch (token.GetType()) {
    case kTokenNone:
    case kTokenEnd:
    case kTokenNewline:
      absl::Format(&sink, "kNone");
      break;
    case kTokenError:
      absl::Format(&sink, "\"%s\"", token.GetString());
      break;
    case kTokenSymbol:
      absl::Format(&sink, "%s", token.GetSymbol().GetString());
      break;
    case kTokenString:
      absl::Format(&sink, "\"%s\"", token.GetString());
      break;
    case kTokenInt:
      absl::Format(&sink, "%d", token.GetInt());
      break;
    case kTokenFloat:
      absl::Format(&sink, "%f", token.GetFloat());
      break;
    case kTokenIdentifier:
      absl::Format(&sink, "\"%s\"", token.GetString());
      break;
    case kTokenKeyword:
      absl::Format(&sink, "{index:%d, value:\"%s\"}", token.GetIndex(),
                   token.GetString());
      break;
    default:
      // All user types are strings.
      absl::Format(&sink, "\"%s\"", token.GetString());
      break;
  }
  absl::Format(&sink, "}");
}

inline int64_t Token::GetInt() const {
  if (value_type_ != ValueType::kInt) {
    return 0;
  }
  return int_;
}

inline double Token::GetFloat() const {
  if (value_type_ != ValueType::kFloat) {
    return 0;
  }
  return float_;
}

inline std::string_view Token::GetString() const {
  if (value_type_ == ValueType::kString) {
    return std::string_view(string_, strlen_);
  }
  if (value_type_ == ValueType::kIndex ||
      value_type_ == ValueType::kStringView) {
    DCHECK(string_view_ != nullptr);
    return *string_view_;
  }
  return {};
}

inline int Token::GetIndex() const {
  if (value_type_ != ValueType::kIndex) {
    return -1;
  }
  return index_;
}

inline Symbol Token::GetSymbol() const {
  if (value_type_ != ValueType::kSymbol) {
    return {};
  }
  return symbol_;
}

}  // namespace gb

#endif  // GB_PARSE_TOKEN_H_