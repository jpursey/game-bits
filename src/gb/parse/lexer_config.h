// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_LEXER_CONFIG_H_
#define GB_PARSE_LEXER_CONFIG_H_

#include <cstdint>
#include <string_view>

#include "absl/types/span.h"

namespace gb {

// Configuration flags for the lexer.
enum class LexerFlag {
  // Integer parsing flags.
  kInt8,              // Allow and detect 8-bit integers.
  kInt16,             // Allow and detect 16-bit integers.
  kInt32,             // Allow and detect 32-bit integers.
  kInt64,             // Allow and detect 64-bit integers.
  kNegativeIntegers,  // Allow negative integers.
  kDecimalIntegers,   // Allow decimal format integers.
  kHexIntegers,       // Allow hexadecimal format integers.
  kOctalIntegers,     // Allow octal format integers.
  kBinaryIntegers,    // Allow binary format integers.

  // Float parsing flags.
  kFloat32,         // Allow and detect 32-bit floating point numbers.
  kFloat64,         // Allow and detect 64-bit floating point numbers.
  kNegativeFloats,  // Allow negative floating point numbers.
  kDecimalFloats,   // Allow decimal format floating point (no exponent).
  kExponentFloats,  // Allow exponents in floating point numbers.

  // String and character parsing flags.
  kDoubleQuoteString,     // "abc" (incompatible with kDoubleQuoteCharacter)
  kSingleQuoteString,     // 'abc' (incompatible with kSingleQuoteCharacter)
  kDoubleQuoteCharacter,  // "a" (incompatible with kDoubleQuoteString)
  kSingleQuoteCharacter,  // 'a' (incompatible with kSingleQuoteString)
  kTabInQuotes,           // Allows literal tab characters in strings

  // String and character escape settings.
  // - Both kDoubleQuoteEscape and kEscapeCharacter may be set, in which case
  //   both forms of escaping are allowed in strings.
  // - kNewlineEscape, kTabEscape, and kHexEscape are only used if
  //   kEscapeCharacter is set.
  kDoubleQuoteEscape,  // Allows "" or '' inside double quoted strings.
  kEscapeCharacter,    // Escape character provides escape (set in config).
  kNewlineEscape,      // Allows newline escape character (set in config).
  kTabEscape,          // Allows tab escape character (set in config).
  kHexEscape,          // Allows hex escape character (set in config).
  kDecodeEscape,       // Decodes escape sequences for token values.

  // Identifier parsing flags.
  kIdentUpper,               // Allows uppercase ASCII letters.
  kIdentLower,               // Allows lowercase ASCII letters.
  kIdentDigit,               // Allows non-leading decimal digits.
  kIdentUnderscore,          // Allows underscores in the middle of identifiers.
  kIdentLeadingUnderscore,   // Allows leading underscores.
  kIdentTrailingUnderscore,  // Allows trailing underscores.
  kIdentForceUpper,          // Forces all identifiers to be uppercase.
  kIdentForceLower,          // Forces all identifiers to be lowercase.

  // Whitespace and comment parsing flags.
  kLineBreak,      // Newlines are not whitespace (enables kTokenNewline).
  kLineIndent,     // Indentation is significant (enables kTokenIndent).
  kEscapeNewline,  // Newlines can be escaped (set in config).
  kLineComments,   // Allows line comments (set in config).
  kBlockComments,  // Allows block comments (set in config).
};
using LexerFlags = Flags<LexerFlag>;

// Basic positive int and number support (size is always 64-bit). Combine with
// kLexerFlags_NegativeNumbers for negative numbers.
inline constexpr LexerFlags kLexerFlags_Positive64BitNumbers = {
    LexerFlag::kInt64, LexerFlag::kDecimalIntegers, LexerFlag::kFloat64,
    LexerFlag::kDecimalFloats};

// Support negative numbers for whichever number types are supported.
inline constexpr LexerFlags kLexerFlags_NegativeNumbers = {
    LexerFlag::kNegativeIntegers, LexerFlag::kNegativeFloats};

// Support all positive decimal integer types and determine bit depth.
inline constexpr LexerFlags kLexerFlags_AllPositiveIntegers = {
    LexerFlag::kInt8, LexerFlag::kInt16, LexerFlag::kInt32, LexerFlag::kInt64,
    LexerFlag::kDecimalIntegers};

// Support all decimal integer types and determine bit depth.
inline constexpr LexerFlags kLexerFlags_AllIntegers = {
    LexerFlag::kInt8,
    LexerFlag::kInt16,
    LexerFlag::kInt32,
    LexerFlag::kInt64,
    LexerFlag::kNegativeIntegers,
    LexerFlag::kDecimalIntegers};

// Support all integer formats (decimal, hex, octal, and binary).
inline constexpr LexerFlags kLexerFlags_AllIntegerFormats = {
    LexerFlag::kDecimalIntegers, LexerFlag::kHexIntegers,
    LexerFlag::kOctalIntegers, LexerFlag::kBinaryIntegers};

// Support all positive float types and determine bit depth.
inline constexpr LexerFlags kLexerFlags_PositiveFloats = {
    LexerFlag::kDecimalFloats, LexerFlag::kFloat32, LexerFlag::kFloat64};

// Support all float types and determine bit depth.
inline constexpr LexerFlags kLexerFlags_AllFloats = {
    LexerFlag::kDecimalFloats, LexerFlag::kFloat32, LexerFlag::kFloat64,
    LexerFlag::kNegativeFloats};

// Support all float formats (decimal and exponent).
inline constexpr LexerFlags kLexerFlags_AllFloatFormats = {
    LexerFlag::kDecimalFloats, LexerFlag::kExponentFloats};

// Support C style strings and escaping.
inline constexpr LexerFlags kLexerFlags_CStrings = {
    LexerFlag::kDoubleQuoteString, LexerFlag::kEscapeCharacter,
    LexerFlag::kNewlineEscape, LexerFlag::kTabEscape, LexerFlag::kHexEscape};

// Support C style characters and escaping.
inline constexpr LexerFlags kLexerFlags_CCharacters = {
    LexerFlag::kSingleQuoteCharacter, LexerFlag::kEscapeCharacter,
    LexerFlag::kNewlineEscape, LexerFlag::kTabEscape, LexerFlag::kHexEscape};

// Support C style identifiers.
inline constexpr LexerFlags kLexerFlags_CIdentifiers = {
    LexerFlag::kIdentUpper,
    LexerFlag::kIdentLower,
    LexerFlag::kIdentDigit,
    LexerFlag::kIdentUnderscore,
    LexerFlag::kIdentLeadingUnderscore,
    LexerFlag::kIdentTrailingUnderscore};

// Support all C style features.
inline constexpr LexerFlags kLexerFlags_C = {
    kLexerFlags_AllIntegers,       kLexerFlags_AllFloats,
    kLexerFlags_AllIntegerFormats, kLexerFlags_AllFloatFormats,
    kLexerFlags_CStrings,          kLexerFlags_CCharacters,
    kLexerFlags_CIdentifiers,      LexerFlag::kLineComments,
    LexerFlag::kBlockComments,     LexerFlag::kEscapeNewline};

//------------------------------------------------------------------------------
// Configuration for the lexer.
//------------------------------------------------------------------------------

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
  absl::Span<const std::string_view> line_comments;
  absl::Span<const BlockComment> block_comments;

  // All valid symbols and keywords. This must include even single character
  // symbols, or they will not be allowed.
  absl::Span<const std::string_view> symbols;
  absl::Span<const std::string_view> keywords;
};

}  // namespace gb

#endif  // GB_PARSE_LEXER_CONFIG_H_
