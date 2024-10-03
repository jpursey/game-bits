// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "gb/base/flags.h"
#include "gb/parse/parse_types.h"

namespace gb {

// Unique ID of the LexerContent.
using LexerContentId = uint32_t;

// Location of a token in a file. This can be retrieved from the Lexer given a
// token that was parsed from it. If it is an unknown token, or one not from
// the lexer, the values will be 0 / empty.
struct TokenLocation {
  LexerContentId id = 0;      // The id of the content within the lexer
  std::string_view filename;  // The filename of the content, if there is one
  int line = 0;               // The line number of the token (0 is unknown)
  int column = 0;             // The column number of the token (0 is unknown)
};

// Token type.
using TokenType = uint8_t;

// Predefined token types.
inline constexpr TokenType kTokenNone = 0;
inline constexpr TokenType kTokenEnd = 1;
inline constexpr TokenType kTokenError = 2;
inline constexpr TokenType kTokenSymbol = 3;
inline constexpr TokenType kTokenString = 4;
inline constexpr TokenType kTokenInt = 5;
inline constexpr TokenType kTokenFloat = 6;
inline constexpr TokenType kTokenIdentifier = 7;
inline constexpr TokenType kTokenKeyword = 8;
inline constexpr TokenType kTokenNewline = 9;
inline constexpr TokenType kTokenComment = 10;

// The types of underlying values that a token can have.
enum TokenValueType : uint8_t {
  kNone = 0,
  kFloat = 1,
  kInt = 2,
  kString = 3,
  kIndex = 4,
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

  // Returns the type of the token.
  TokenType GetType() const { return type_; }

  // Returns the type of value stored (this can generally be inferred from the
  // type of the token).
  TokenValueType GetValueType() const { return value_type_; }

  // Returns the value of the token as an integer. It is an error to call this
  // if the value type is not kInt.
  int64_t GetInt() const {
    DCHECK(value_type_ == TokenValueType::kInt);
    return int_;
  }

  // Returns the value of the token as a float. It is an error to call this if
  // the value type is not kFloat.
  double GetFloat() const {
    DCHECK(value_type_ == TokenValueType::kFloat);
    return float_;
  }

  // Returns the value of the token as a string. It is an error to call this if
  // the value type is not kString.
  std::string_view GetString() const {
    DCHECK(value_type_ == TokenValueType::kString);
    return std::string_view(string_, value_size_);
  }

  // Returns the index value of the token. It is an error to call this if the
  // value type is not kIndex.
  int GetIndex() const {
    DCHECK(value_type_ == TokenValueType::kIndex);
    return index_;
  }

  // Returns the symbol value of the token. It is an error to call this if the
  // token type is not kSymbol.
  int GetSymbol() const {
    DCHECK(type_ == kTokenSymbol && value_type_ == TokenValueType::kIndex);
    return index_;
  }

  // Returns the keyword value of the token. It is an error to call this if the
  // token type is not kKeyword.
  int GetKeyword() const {
    DCHECK(type_ == kTokenKeyword && value_type_ == TokenValueType::kIndex);
    return index_;
  }

  // Returns the identifier value of the token. It is an error to call this if
  // the token type is not kIdentifier.
  std::string_view GetIdentifier() const {
    DCHECK(type_ == kTokenIdentifier && value_type_ == TokenValueType::kString);
    return std::string_view(string_, value_size_);
  }

  // Lexer factory methods for creating specific token types.
  static Token TokenEnd(LexerInternal, uint32_t lexer_index) {
    return Token(lexer_index, kTokenEnd);
  }
  static Token TokenError(LexerInternal, uint32_t lexer_index) {
    return Token(lexer_index, kTokenError);
  }
  static Token TokenSymbol(LexerInternal, uint32_t lexer_index, int symbol) {
    return Token(lexer_index, kTokenSymbol).Index(symbol);
  }
  static Token TokenString(LexerInternal, uint32_t lexer_index,
                           std::string_view value) {
    DCHECK(value.size() <= std::numeric_limits<decltype(value_size_)>::max());
    return Token(lexer_index, kTokenString).String(value.data(), value.size());
  }
  static Token TokenInt(LexerInternal, uint32_t lexer_index, int64_t value) {
    return Token(lexer_index, kTokenInt).Int(value);
  }
  static Token TokenFloat(LexerInternal, uint32_t lexer_index, double value) {
    return Token(lexer_index, kTokenFloat).Float(value);
  }
  static Token TokenIdentifier(LexerInternal, uint32_t lexer_index,
                               std::string_view value) {
    DCHECK(value.size() <= std::numeric_limits<decltype(value_size_)>::max());
    return Token(lexer_index, kTokenIdentifier)
        .String(value.data(), value.size());
  }
  static Token TokenKeyword(LexerInternal, uint32_t lexer_index, int keyword) {
    return Token(lexer_index, kTokenKeyword).Index(keyword);
  }
  static Token TokenNewline(LexerInternal, uint32_t lexer_index) {
    return Token(lexer_index, kTokenNewline);
  }
  static Token TokenComment(LexerInternal, uint32_t lexer_index,
                            uint32_t comment_end_lexer_index) {
    return Token(lexer_index, kTokenComment)
        .CommentEnd(comment_end_lexer_index);
  }

 private:
  Token(uint32_t lexer_index, TokenType type)
      : lexer_index_(lexer_index), type_(type) {}
  Token& Int(int64_t value, uint16_t size = sizeof(int64_t)) {
    value_type_ = TokenValueType::kInt;
    value_size_ = size;
    int_ = value;
    return *this;
  }
  Token& Float(double value, uint16_t size = sizeof(double)) {
    value_type_ = TokenValueType::kFloat;
    value_size_ = size;
    float_ = value;
    return *this;
  }
  Token& String(const char* value, uint16_t size) {
    value_type_ = TokenValueType::kString;
    value_size_ = size;
    string_ = value;
    return *this;
  }
  Token& Index(int value) {
    value_type_ = TokenValueType::kIndex;
    index_ = value;
    return *this;
  }
  Token& CommentEnd(uint32_t value) {
    comment_end_lexer_index_ = value;
    return *this;
  }

  uint32_t lexer_index_ = 0;
  TokenType type_ = kTokenNone;
  TokenValueType value_type_ = kNone;
  uint16_t value_size_ = 0;
  union {
    int64_t int_;
    double float_;
    const char* string_;
    int index_;
    uint32_t comment_end_lexer_index_;
  };
};

// Configuration flags for the lexer.
enum class LexerFlag {
  // Integer parsing flags.
  kInt8,
  kInt16,
  kInt32,
  kInt64,
  kNegativeIntegers,
  kDecimalIntegers,
  kHexIntegers,
  kOctalIntegers,
  kBinaryIntegers,

  // Float parsing flags.
  kFloat32,
  kFloat64,
  kNegativeFloats,

  // String and character parsing flags.
  kDoubleQuoteString,     // "abc"
  kSingleQuoteString,     // 'abc'
  kDoubleQuoteCharacter,  // "a"
  kSingleQuoteCharacter,  // 'a'

  // String and character escape settings.
  kDoubleQuoteEscape,  // Allows "" or '' inside double quoted strings.
  kEscapeCharacter,    // Escape character provides escape (set in config).
  kNewlineEscape,      // Allows newline escape character (set in config).
  kTabEscape,          // Allows tab escape character (set in config).
  kHexEscape,          // Allows hex escape character (set in config).
  kDecodeEscape,       // Decodes escape sequences in strings and characters.

  // Identifier parsing flags.
  kIdentUpper,               // Allows uppercase ASCII letters.
  kIdentLower,               // Allows lowercase ASCII letters.
  kIdentDigit,               // Allows non-leading decimal digits.
  kIdentUnderscore,          // Allows underscores in the middle of identifiers.
  kIdentLeadingUnderscore,   // Allows leading underscores.
  kIdentTrailingUnderscore,  // Allows trailing underscores.
  kIdentDash,                // Allows dashes in the middle of identifiers.
  kIdentLeadingDash,         // Allows leading dashes.
  kIdentTrailingDash,        // Allows trailing dashes.
  kIdentForceUpper,          // Forces all identifiers to be uppercase.
  kIdentForceLower,          // Forces all identifiers to be lowercase.
  kIdentForceUnderscore,     // Forces all dashes to be underscores.
  kIdentForceDash,           // Forces all underscores to be dashes.

  // Whitespace and comment parsing flags.
  kLineBreak,      // Newlines are not hitespace (enables kTokenNewline).
  kEscapeNewline,  // Newlines can be escaped (set in config).
  kLineComments,   // Allows line comments (set in config).
  kBlockComments,  // Allows block comments (set in config).
};
using LexerFlags = Flags<LexerFlag>;
inline constexpr LexerFlags kLexerFlags_MaxSizeNumbers = {
    LexerFlag::kInt64, LexerFlag::kFloat64, LexerFlag::kDecimalIntegers};
inline constexpr LexerFlags kLexerFlags_NegativeNumbers = {
    LexerFlag::kNegativeIntegers, LexerFlag::kNegativeFloats};

inline constexpr LexerFlags kLexerFlags_AllPositiveIntegers = {
    LexerFlag::kInt8, LexerFlag::kInt16, LexerFlag::kInt32, LexerFlag::kInt64,
    LexerFlag::kDecimalIntegers};
inline constexpr LexerFlags kLexerFlags_AllIntegers = {
    LexerFlag::kInt8,
    LexerFlag::kInt16,
    LexerFlag::kInt32,
    LexerFlag::kInt64,
    LexerFlag::kNegativeIntegers,
    LexerFlag::kDecimalIntegers};
inline constexpr LexerFlags kLexerFlags_AllIntegerFormats = {
    LexerFlag::kDecimalIntegers, LexerFlag::kHexIntegers,
    LexerFlag::kOctalIntegers, LexerFlag::kBinaryIntegers};

inline constexpr LexerFlags kLexerFlags_PositiveFloats = {LexerFlag::kFloat32,
                                                          LexerFlag::kFloat64};
inline constexpr LexerFlags kLexerFlags_AllFloats = {
    LexerFlag::kFloat32, LexerFlag::kFloat64, LexerFlag::kNegativeFloats};

inline constexpr LexerFlags kLexerFlags_CStrings = {
    LexerFlag::kDoubleQuoteString, LexerFlag::kEscapeCharacter,
    LexerFlag::kNewlineEscape, LexerFlag::kTabEscape, LexerFlag::kHexEscape};
inline constexpr LexerFlags kLexerFlags_CCharacters = {
    LexerFlag::kSingleQuoteCharacter, LexerFlag::kEscapeCharacter,
    LexerFlag::kNewlineEscape, LexerFlag::kTabEscape, LexerFlag::kHexEscape};

inline constexpr LexerFlags kLexerFlags_CIdentifiers = {
    LexerFlag::kIdentUpper,
    LexerFlag::kIdentLower,
    LexerFlag::kIdentDigit,
    LexerFlag::kIdentUnderscore,
    LexerFlag::kIdentLeadingUnderscore,
    LexerFlag::kIdentTrailingUnderscore};

inline constexpr LexerFlags kLexerFlags_C = {
    kLexerFlags_AllIntegers,       kLexerFlags_AllFloats,
    kLexerFlags_AllIntegerFormats, kLexerFlags_CStrings,
    kLexerFlags_CCharacters,       kLexerFlags_CIdentifiers,
    LexerFlag::kLineComments,      LexerFlag::kBlockComments,
    LexerFlag::kEscapeNewline};

// Configuration for the lexer.
struct LexerConfig {
  // Overall flags that control lexer behavior.
  LexerFlags flags;

  // Integer prefixes and suffixes.
  std::string_view hex_prefix = "0x";     // Used for kHexIntegers.
  std::string_view hex_suffix = "";       // Used for kHexIntegers.
  std::string_view octal_prefix = "0";    // Used for kOctalIntegers.
  std::string_view octal_suffix = "";     // Used for kOctalIntegers.
  std::string_view binary_prefix = "0b";  // Used for kBinaryIntegers.
  std::string_view binary_suffix = "";    // Used for kBinaryIntegers.

  // Escape character settings
  char escape = 0;          // Used for kCharacterEscaping or kEscapeNewLine.
  char escape_newline = 0;  // Used for kNewlineEscape.
  char escape_tab = 0;      // Used for kTabEscape.
  char escape_hex = 0;      // Used for kHexEscape.

  // Comment settings.
  struct BlockComment {
    std::string_view start;
    std::string_view end;
  };
  absl::Span<std::string_view> line_comments;
  absl::Span<BlockComment> block_comments;

  // All valid symbols and keywords. This must include even single character
  // symbols, or they will not be allowed.
  absl::Span<std::string_view> symbols;
  absl::Span<std::string_view> keywords;
};

// The lexer is used to parse text content into tokens.
class Lexer final {
 public:
  explicit Lexer(const LexerConfig& config);
  Lexer(const Lexer&) = delete;
  Lexer& operator=(const Lexer&) = delete;
  ~Lexer() = default;

  // Adds a file to the lexer, returning the LexerContentId which can be used to
  // parse the file.
  LexerContentId AddFileContent(std::string_view filename, std::string content);

  // Adds text content without a specific filename to the lexer. Returns the
  // LexerContentId which can be used to parse the content.
  LexerContentId AddContent(std::string content);

  // Returns the content that was previously added for the given filename.
  LexerContentId GetContent(std::string_view filename);

  // Returns the filename associated with the specified content ID.
  std::string_view GetFilename(LexerContentId id);

  // Returns the token location.
  TokenLocation GetTokenLocation(const Token& token);

  // Returns the text of the token.
  std::string_view GetTokenText(const Token& token);

  // Returns the next token in the stream for the content.
  //
  // This always returns a token, even if there are no more tokens in the
  // stream. In that case, the token will have type kTokenEnd. If there was a
  // parsing error, the token will have type kTokenError. Otherwise, the token
  // will have the appropriate type and value as determine by the LexerConfig
  // settings.
  //
  // The returned token is valid for as long as the Lexer is valid.
  Token NextToken(LexerContentId id);

  // Skips forward to the next line beginning in the content. If the content is
  // already at the beginning of a line, it will skip to the beginning of the
  // next line.
  void NextLine(LexerContentId id);

  // Rewinds the token stream by one token.
  //
  // This can only be called once after NextToken() returns a non-end/error
  // token, or it will return false. If the token stream is rewound past the
  // beginning, it will be clamped to the beginning and this will return also
  // false.
  bool RewindToken(LexerContentId id);

  // Rewinds the content to the beginning of the current line. If this is
  // currently at the beginning of a line, it will rewind to the beginning of
  // the previous line.
  void RewindLine(LexerContentId id);

  // Rewinds the content to the beginning.
  void RewindContent(LexerContentId id);

 private:
  struct Content {
    LexerContentId id;
    std::string_view filename;
    std::string content;
  };

  std::vector<std::unique_ptr<Content>> content_;
  absl::flat_hash_map<std::string_view, LexerContentId> filename_content_;
  std::vector<std::unique_ptr<std::string>> modified_text_;
};

}  // namespace gb