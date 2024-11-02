// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_LEXER_CONFIG_H_
#define GB_PARSE_LEXER_CONFIG_H_

#include <cstdint>
#include <string_view>

#include "absl/types/span.h"
#include "gb/parse/symbol.h"
#include "gb/parse/token.h"

namespace gb {

// Configuration flags for the lexer.
enum class LexerFlag {
  // Integer parsing flags.
  // - Both a bit depth (8, 16, 32, or 64) and a format (decimal, binary, octal,
  //   and/or hex) must be set for integers to be parsed. Decimal values are
  //   always signed (but default to positive values only), while binary, octal,
  //   and hex values are always unsigned.
  // - The bit depth determines the largest and smallest values that are allowed
  //   as a value. If multiple bit depths are set, the highest depth is used.
  kInt8,              // Allow and detect 8-bit integers.
  kInt16,             // Allow and detect 16-bit integers.
  kInt32,             // Allow and detect 32-bit integers.
  kInt64,             // Allow and detect 64-bit integers.
  kNegativeIntegers,  // Allow negative integers.
  kBinaryIntegers,    // Allow binary format integers.
  kOctalIntegers,     // Allow octal format integers.
  kDecimalIntegers,   // Allow decimal format integers.
  kHexUpperIntegers,  // Allow hexadecimal format integers with upper case.
  kHexLowerIntegers,  // Allow hexadecimal format integers with lower case.

  // Float parsing flags.
  // - Both a bit depth (32 or 64) and a format (decimal and/or exponent) must
  //   be set for floats to be parsed. Float values are always signed (but
  //   default to positive values only).
  // - The bit depth determines the largest and smallest values that are allowed
  //   as a value. If multiple bit depths are set, the highest depth is used.
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

  // String and character escape settings.
  // - Both kQuoteQuoteEscape and kEscapeCharacter may be set, in which case
  //   both forms of escaping are allowed in strings.
  // - kNewlineEscape, kTabEscape, and kHexEscape are only used if
  //   kEscapeCharacter is set.
  kQuoteQuoteEscape,  // Allows "" or '' inside similarly quoted strings.
  kEscapeCharacter,   // Escape character provides escape (set in config).
  kNewlineEscape,     // Allows newline escape character (set in config).
  kTabEscape,         // Allows tab escape character (set in config).
  kHexEscape,         // Allows hex escape character (set in config).
  kDecodeEscape,      // Decodes escape sequences for token values.

  // Identifier parsing flags.
  // - Upper and/or lower case letters must be allowed (case sensitive) or
  //   forced (case insensitive). Identifiers cannot be only numbers and
  //   symbols. Both upper and lower may be set for case sensitivity, but not
  //   for forcing a specific case.
  // - Digits and underscores can be optionally allowed in identifiers, either
  //   generally or limited to non-leading characters.
  kIdentUpper,              // Allows uppercase ASCII letters.
  kIdentLower,              // Allows lowercase ASCII letters.
  kIdentDigit,              // Allows decimal digits.
  kIdentNonLeadDigit,       // Allows non-leading decimal digits.
  kIdentUnderscore,         // Allows underscores.
  kIdentNonLeadUnderscore,  // Allows non-leading underscores.
  kIdentForceUpper,         // Forces identifiers to be uppercase.
  kIdentForceLower,         // Forces identifiers to be lowercase.
  kKeywordCaseInsensitive,  // Keywords are case insensitive.

  // Whitespace and comment parsing flags.
  kLineBreak,  // Newlines are not whitespace (enables kTokenNewline).
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
    LexerFlag::kDecimalIntegers, LexerFlag::kHexUpperIntegers,
    LexerFlag::kHexLowerIntegers, LexerFlag::kOctalIntegers,
    LexerFlag::kBinaryIntegers};

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
    LexerFlag::kIdentUpper, LexerFlag::kIdentLower, LexerFlag::kIdentUnderscore,
    LexerFlag::kIdentNonLeadDigit};

// Support all C style features.
inline constexpr LexerFlags kLexerFlags_C = {
    kLexerFlags_AllIntegers,       kLexerFlags_AllFloats,
    kLexerFlags_AllIntegerFormats, kLexerFlags_AllFloatFormats,
    kLexerFlags_CStrings,          kLexerFlags_CCharacters,
    kLexerFlags_CIdentifiers};

//------------------------------------------------------------------------------
// Flag queries.
//------------------------------------------------------------------------------

inline bool LexerSupportsIntegers(LexerFlags flags) {
  return flags.Intersects({LexerFlag::kInt8, LexerFlag::kInt16,
                           LexerFlag::kInt32, LexerFlag::kInt64}) &&
         flags.Intersects(
             {LexerFlag::kDecimalIntegers, LexerFlag::kHexUpperIntegers,
              LexerFlag::kHexLowerIntegers, LexerFlag::kOctalIntegers,
              LexerFlag::kBinaryIntegers});
}

inline bool LexerSupportsFloats(LexerFlags flags) {
  return flags.Intersects({LexerFlag::kFloat32, LexerFlag::kFloat64}) &&
         flags.Intersects(
             {LexerFlag::kDecimalFloats, LexerFlag::kExponentFloats});
}

inline bool LexerSupportsStrings(LexerFlags flags) {
  return flags.Intersects(
      {LexerFlag::kDoubleQuoteString, LexerFlag::kSingleQuoteString});
}

inline bool LexerSupportsCharacters(LexerFlags flags) {
  return flags.Intersects(
      {LexerFlag::kDoubleQuoteCharacter, LexerFlag::kSingleQuoteCharacter});
}

inline bool LexerSupportsIdentifiers(LexerFlags flags) {
  return flags.Intersects({LexerFlag::kIdentUpper, LexerFlag::kIdentLower,
                           LexerFlag::kIdentForceLower,
                           LexerFlag::kIdentForceUpper});
}

//------------------------------------------------------------------------------
// Configuration for the lexer.
//------------------------------------------------------------------------------

struct LexerConfig {
  // Overall flags that control lexer behavior.
  LexerFlags flags;

  // Prefixes and suffixes.
  std::string_view binary_prefix = "";   // Used for kBinaryIntegers.
  std::string_view binary_suffix = "";   // Used for kBinaryIntegers.
  std::string_view octal_prefix = "";    // Used for kOctalIntegers.
  std::string_view octal_suffix = "";    // Used for kOctalIntegers.
  std::string_view decimal_prefix = "";  // Used for kDecimalIntegers.
  std::string_view decimal_suffix = "";  // Used for kDecimalIntegers.
  std::string_view hex_prefix = "";      // Used for kHexIntegers.
  std::string_view hex_suffix = "";      // Used for kHexIntegers.
  std::string_view float_prefix = "";    // Used for floating point numbers.
  std::string_view float_suffix = "";    // Used for floating point numbers.
  std::string_view ident_prefix = "";    // Used for identifiers.
  std::string_view ident_suffix = "";    // Used for identifiers.

  // Escape character settings.
  char escape = 0;          // Used for kCharacterEscaping or kEscapeNewLine.
  char escape_newline = 0;  // Used for kNewlineEscape.
  char escape_tab = 0;      // Used for kTabEscape.
  char escape_hex = 0;      // Used for kHexEscape (followed by 2 hex digits).

  // Comment settings.
  struct BlockComment {
    std::string_view start;
    std::string_view end;
  };
  absl::Span<const std::string_view> line_comments;
  absl::Span<const BlockComment> block_comments;

  // All valid symbols and keywords. This must include even single character
  // symbols, or they will not be allowed.
  absl::Span<const Symbol> symbols;

  // All special keywords. These can be anything, but are typically are
  // identifiers which have unique meaning.
  absl::Span<const std::string_view> keywords;

  // Additional user tokens specified with their own regex.
  struct UserToken {
    // The type of token. Must be >= to kTokenUser.
    TokenType type;

    // The regex must be RE2 syntax, and must have exactly one capturing group.
    std::string_view regex;
  };
  absl::Span<const UserToken> user_tokens;
};

inline constexpr std::string_view kCStyleLineComments[] = {"//"};
inline constexpr LexerConfig::BlockComment kCStyleBlockComments[] = {
    {"/*", "*/"}};

inline constexpr Symbol kCStyleArithmeticSymbols[] = {'+', '-', '*', '/', '%'};
inline constexpr Symbol kCStyleBitwiseSymbols[] = {'~', '&',  '|',
                                                   '^', "<<", ">>"};
inline constexpr Symbol kCStyleBooleanSymbols[] = {'!', "&&", "||"};
inline constexpr Symbol kCStyleComparisonSymbols[] = {
    '<', '>', "<=", ">=", "==", "!="};
inline constexpr Symbol kCStyleAssignmentSymbols[] = {"="};
inline constexpr Symbol kCStyleArithmaticAssignmentSymbols[] = {
    "+=", "-=", "*=", "/=", "%="};
inline constexpr Symbol kCStyleBitwiseAssignmentSymbols[] = {
    "&=", "|=", "^=", "<<=", ">>="};
inline constexpr Symbol kCStyleIncDecSymbols[] = {"++", "--"};
inline constexpr Symbol kCStyleDerefSymbols[] = {'.', "->"};
inline constexpr Symbol kCStyleSeparatorSymbols[] = {',', ';', ':', '?'};
inline constexpr Symbol kCStyleGroupingSymbols[] = {'(', ')', '[',
                                                    ']', '{', '}'};

// All single character symbols except backtick, backslash, and quotes.
inline constexpr Symbol kCharSymbols[] = {
    '+', '-', '*', '/', '%', '~', '&', '|', '^', '!', '<', '>', '=', '.',
    ',', ';', ':', '?', '$', '#', '@', '(', ')', '[', ']', '{', '}'};

// Extension of kCharSymbols with C-style expression symbols.
inline constexpr Symbol kCharSymbolsWithCStyleExpressions[] = {
    '+', '-',  '*',  '/',  '%',  '~',  '&',  '|',  '^',  '!',  '<',  '>', '=',
    '.', ',',  ';',  ':',  '?',  '$',  '#',  '@',  '(',  ')',  '[',  ']', '{',
    '}', "<=", ">=", "==", "!=", "<<", ">>", "&&", "||", "++", "--", "->"};

// Extension of kCharSymbolsWithCStyleExpressions with combo assignment symbols.
inline constexpr Symbol kCharSymbolsWithCStyleExpressionsAndAssignment[] = {
    '+',  '-',  '*',  '/',  '%',  '~',  '&',  '|',  '^',  '!',  '<',   '>',
    '=',  '.',  ',',  ';',  ':',  '?',  '$',  '#',  '@',  '(',  ')',   '[',
    ']',  '{',  '}',  "<=", ">=", "==", "!=", "<<", ">>", "&&", "||",  "++",
    "--", "->", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="};

// Only C-style symbols (no # or @ or $).
inline constexpr Symbol kCStyleSymbols[] = {
    '+',  '-',  '*',  '/',  '%',  '~',  '&',  '|',   '^',  '!',  '<',  '>',
    '=',  '.',  ',',  ';',  ':',  '?',  '(',  ')',   '[',  ']',  '{',  '}',
    "<=", ">=", "==", "!=", "<<", ">>", "&&", "||",  "++", "--", "->", "+=",
    "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="};

// Keywords for the C language
inline constexpr std::string_view kCKeywords[] = {
    "alignas",      "alignof",  "auto",          "bool",      "break",
    "case",         "char",     "const",         "constexpr", "continue",
    "default",      "do",       "double",        "else",      "enum",
    "extern",       "false",    "float",         "for",       "goto",
    "if",           "inline",   "int",           "long",      "nullptr",
    "register",     "restrict", "return",        "short",     "signed",
    "sizeof",       "static",   "static_assert", "struct",    "switch",
    "thread_local", "true",     "typedef",       "typeof",    "typeof_unqual",
    "union",        "unsigned", "void",          "volatile",  "while",
};

// Keywords for the C++ language
inline constexpr std::string_view kCppKeywords[] = {
    "alignas",       "alignof",     "and",
    "and_eq",        "asm",         "auto",
    "bitand",        "bitor",       "bool",
    "break",         "case",        "catch",
    "char",          "char8_t",     "char16_t",
    "char32_t",      "class",       "compl",
    "concept",       "const",       "consteval",
    "constexpr",     "constinit",   "const_cast",
    "continue",      "co_await",    "co_return",
    "co_yield",      "decltype",    "default",
    "delete",        "do",          "double",
    "dynamic_cast",  "else",        "enum",
    "explicit",      "export",      "extern",
    "false",         "float",       "for",
    "friend",        "goto",        "if",
    "inline",        "int",         "long",
    "mutable",       "namespace",   "new",
    "noexcept",      "not",         "not_eq",
    "nullptr",       "operator",    "or",
    "or_eq",         "private",     "protected",
    "public",        "register",    "reinterpret_cast",
    "requires",      "return",      "short",
    "signed",        "sizeof",      "static",
    "static_assert", "static_cast", "struct",
    "switch",        "template",    "this",
    "thread_local",  "throw",       "true",
    "try",           "typedef",     "typeid",
    "typename",      "union",       "unsigned",
    "using",         "virtual",     "void",
    "volatile",      "wchar_t",     "while",
    "xor",           "xor_eq",
};

inline constexpr LexerConfig kCStyleLexerConfig = {
    .flags = kLexerFlags_C,
    .binary_prefix = "0b",
    .octal_prefix = "0",
    .hex_prefix = "0x",
    .float_prefix = "",
    .ident_prefix = "",
    .escape = '\\',
    .escape_newline = 'n',
    .escape_tab = 't',
    .escape_hex = 'x',
    .line_comments = kCStyleLineComments,
    .block_comments = kCStyleBlockComments,
    .symbols = kCStyleSymbols,
    .keywords = kCKeywords,
};

inline constexpr LexerConfig kCppStyleLexerConfig = {
    .flags = kLexerFlags_C,
    .binary_prefix = "0b",
    .octal_prefix = "0",
    .hex_prefix = "0x",
    .float_prefix = "",
    .ident_prefix = "",
    .escape = '\\',
    .escape_newline = 'n',
    .escape_tab = 't',
    .escape_hex = 'x',
    .line_comments = kCStyleLineComments,
    .block_comments = kCStyleBlockComments,
    .symbols = kCStyleSymbols,
    .keywords = kCppKeywords,
};

}  // namespace gb

#endif  // GB_PARSE_LEXER_CONFIG_H_
