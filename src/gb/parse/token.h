// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_TOKEN_H_
#define GB_PARSE_TOKEN_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "gb/parse/parse_types.h"
#include "gb/parse/symbol.h"

namespace gb {

// Token type.
using TokenType = uint8_t;

// Predefined token types.
inline constexpr TokenType kTokenNone = 0;        // Value: kNone
inline constexpr TokenType kTokenEnd = 1;         // Value: kNone
inline constexpr TokenType kTokenError = 2;       // Value: String
inline constexpr TokenType kTokenSymbol = 3;      // Value: Symbol
inline constexpr TokenType kTokenInt = 4;         // Value: Int
inline constexpr TokenType kTokenFloat = 5;       // Value: Float
inline constexpr TokenType kTokenChar = 6;        // Value: String
inline constexpr TokenType kTokenString = 7;      // Value: String
inline constexpr TokenType kTokenKeyword = 8;     // Value: String
inline constexpr TokenType kTokenIdentifier = 9;  // Value: String
inline constexpr TokenType kTokenLineBreak = 10;  // Value: kNone

// Start of user-defined token types. User-defined tokens are always string
// values.
inline constexpr TokenType kTokenUser = 128;

// Map from token type to token type name.
using TokenTypeNames = absl::flat_hash_map<TokenType, std::string>;

inline std::string GetTokenTypeString(TokenType type,
                                      const TokenTypeNames* names = nullptr) {
  if (names != nullptr) {
    if (auto it = names->find(type); it != names->end()) {
      return it->second;
    }
  }
  switch (type) {
    case kTokenNone:
      return "none";
    case kTokenEnd:
      return "end";
    case kTokenError:
      return "error";
    case kTokenSymbol:
      return "symbol";
    case kTokenInt:
      return "integer value";
    case kTokenFloat:
      return "floating-point value";
    case kTokenChar:
      return "character value";
    case kTokenString:
      return "string value";
    case kTokenKeyword:
      return "keyword";
    case kTokenIdentifier:
      return "identifier";
    case kTokenLineBreak:
      return "line break";
    default:
      if (type >= kTokenUser) {
        return absl::StrFormat("user type(%d)", type - kTokenUser);
      }
      return absl::StrFormat("undefined(%d)", type);
  }
}

// The value of a token for token types that do not have a value.
struct NoTokenValue {
  int dummy = 0;
  auto operator<=>(const NoTokenValue&) const = default;
};

// The parsed value of a token.
using TokenValue =
    std::variant<NoTokenValue, Symbol, int64_t, double, std::string>;

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

  // Returns the parsed value of the token.
  TokenValue GetValue() const;

  // Returns the value of the token as a string, regardless of the underlying
  // type. This returns an empty string for errors or tokens that have no value.
  std::string ToString() const;

  // Returns the typed value of the token or a default value if it isn't valid
  // for the token type. Some types (like kTokenEnd) have no value. See the
  // specific token types above for details on the valid value types for each
  // predefined token type.
  int64_t GetInt() const;              // Default: 0
  int64_t GetInt64() const;            // Default: 0
  int32_t GetInt32() const;            // Default: 0
  int16_t GetInt16() const;            // Default: 0
  int8_t GetInt8() const;              // Default: 0
  uint64_t GetUInt() const;            // Default: 0
  uint64_t GetUInt64() const;          // Default: 0
  uint32_t GetUInt32() const;          // Default: 0
  uint16_t GetUInt16() const;          // Default: 0
  uint8_t GetUInt8() const;            // Default: 0
  double GetFloat() const;             // Default: 0.0
  double GetFloat64() const;           // Default: 0.0
  float GetFloat32() const;            // Default: 0.0f
  std::string_view GetString() const;  // Default: empty
  Symbol GetSymbol() const;            // Default: Symbol()

  // Token type comparison.
  bool IsNone() const { return GetType() == kTokenNone; }
  bool IsEnd() const { return GetType() == kTokenEnd; }
  bool IsError() const { return GetType() == kTokenError; }
  bool IsInt() const { return GetType() == kTokenInt; }
  bool IsInt(int64_t value) const {
    return GetType() == kTokenInt && GetInt() == value;
  }
  bool IsUint() const { return GetType() == kTokenInt && GetInt() >= 0; }
  bool IsUint(uint64_t value) const {
    return GetType() == kTokenInt && GetUInt() == value;
  }
  bool IsFloat() const { return GetType() == kTokenFloat; }
  bool IsFloat(double value) const {
    return GetType() == kTokenFloat && GetFloat() == value;
  }
  bool IsChar() const { return GetType() == kTokenChar; }
  bool IsChar(char value) const {
    return GetType() == kTokenChar &&
           GetString() == std::string_view(&value, 1);
  }
  bool IsString() const { return GetType() == kTokenString; }
  bool IsString(std::string_view value) const {
    return GetType() == kTokenString && GetString() == value;
  }
  bool IsSymbol(Symbol symbol) const {
    return GetType() == kTokenSymbol && GetSymbol() == symbol;
  }
  bool IsIdent(std::string_view value = {}) const {
    return GetType() == kTokenIdentifier &&
           (value.empty() || GetString() == value);
  }
  bool IsKeyword(std::string_view value) const {
    return GetType() == kTokenKeyword && GetString() == value;
  }
  bool IsUser(TokenType type, std::string_view value = {}) const {
    DCHECK(type >= kTokenUser);
    return GetType() == type && (value.empty() || GetString() == value);
  }

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
  static Token CreateInt(TokenIndex token_index, int64_t value) {
    Token token(token_index, kTokenInt, ValueType::kInt);
    token.int_ = value;
    return token;
  }
  static Token CreateFloat(TokenIndex token_index, double value) {
    Token token(token_index, kTokenFloat, ValueType::kFloat);
    token.float_ = value;
    return token;
  }
  static Token CreateChar(TokenIndex token_index, const char* value,
                          uint16_t size) {
    Token token(token_index, kTokenChar, ValueType::kString);
    token.strlen_ = size;
    token.string_ = value;
    return token;
  }
  static Token CreateString(TokenIndex token_index, const char* value,
                            uint16_t size) {
    Token token(token_index, kTokenString, ValueType::kString);
    token.strlen_ = size;
    token.string_ = value;
    return token;
  }
  static Token CreateKeyword(TokenIndex token_index, const char* value,
                             uint16_t size) {
    Token token(token_index, kTokenKeyword, ValueType::kString);
    token.strlen_ = size;
    token.string_ = value;
    return token;
  }
  static Token CreateIdentifier(TokenIndex token_index, const char* value,
                                uint16_t size) {
    Token token(token_index, kTokenIdentifier, ValueType::kString);
    token.strlen_ = size;
    token.string_ = value;
    return token;
  }
  static Token CreateLineBreak(TokenIndex token_index) {
    return Token(token_index, kTokenLineBreak, ValueType::kNone);
  }
  static Token CreateUser(TokenIndex token_index, TokenType user_type,
                          const char* value, uint16_t size) {
    DCHECK(user_type >= kTokenUser);
    Token token(token_index, user_type, ValueType::kString);
    token.strlen_ = size;
    token.string_ = value;
    return token;
  }

  Token(TokenIndex token_index, TokenType type, ValueType value_type)
      : token_index_(token_index), type_(type), value_type_(value_type) {}

  TokenIndex token_index_ = kInvalidTokenIndex;
  TokenType type_ = kTokenNone;
  ValueType value_type_ = ValueType::kNone;
  uint16_t strlen_ = 0;  // For type string.
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
void AbslStringify(Sink& sink, const NoTokenValue&) {
  absl::Format(&sink, "none");
}

template <typename Sink>
void AbslStringify(Sink& sink, const TokenValue& token_value) {
  std::visit([&sink](const auto& value) { absl::Format(&sink, "%v", value); },
             token_value);
}

template <typename Sink>
void AbslStringify(Sink& sink, const Token& token) {
  absl::Format(&sink, "{%v, type:%s, value:", token.GetTokenIndex(),
               GetTokenTypeString(token.GetType()));
  switch (token.GetType()) {
    case kTokenNone:
    case kTokenEnd:
    case kTokenLineBreak:
      absl::Format(&sink, "none");
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
      absl::Format(&sink, "\"%s\"", token.GetString());
      break;
    default:
      absl::Format(&sink, "\"%s\"", token.ToString());
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
inline int64_t Token::GetInt64() const { return GetInt(); }
inline int32_t Token::GetInt32() const {
  return static_cast<int32_t>(GetInt());
}
inline int16_t Token::GetInt16() const {
  return static_cast<int16_t>(GetInt());
}
inline int8_t Token::GetInt8() const { return static_cast<int8_t>(GetInt()); }
inline uint64_t Token::GetUInt() const {
  return static_cast<uint64_t>(GetInt());
}
inline uint64_t Token::GetUInt64() const {
  return static_cast<uint64_t>(GetInt());
}
inline uint32_t Token::GetUInt32() const {
  return static_cast<uint32_t>(GetInt());
}
inline uint16_t Token::GetUInt16() const {
  return static_cast<uint16_t>(GetInt());
}
inline uint8_t Token::GetUInt8() const {
  return static_cast<uint8_t>(GetInt());
}

inline double Token::GetFloat() const {
  if (value_type_ != ValueType::kFloat) {
    return 0;
  }
  return float_;
}
inline double Token::GetFloat64() const { return GetFloat(); }
inline float Token::GetFloat32() const {
  return static_cast<float>(GetFloat64());
}

inline std::string_view Token::GetString() const {
  if (value_type_ == ValueType::kString) {
    return std::string_view(string_, strlen_);
  }
  if (value_type_ == ValueType::kStringView) {
    DCHECK(string_view_ != nullptr);
    return *string_view_;
  }
  return {};
}

inline Symbol Token::GetSymbol() const {
  if (value_type_ != ValueType::kSymbol) {
    return {};
  }
  return symbol_;
}

}  // namespace gb

#endif  // GB_PARSE_TOKEN_H_
