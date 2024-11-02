// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_PARSE_TYPES_H_
#define GB_PARSE_PARSE_TYPES_H_

#include "absl/strings/str_format.h"
#include "gb/base/access_token.h"

namespace gb {

// Forward declarations
class Lexer;
class Parser;
class ParserRules;
class ParserRuleItem;
class ParserToken;
class ParserRuleName;
class ParserGroup;

// Unique ID of the LexerContent.
using LexerContentId = uint32_t;

// Special value for no lexer content. This also evaluates to false.
inline constexpr LexerContentId kNoLexerContent = 0;

namespace lexer_internal {
inline constexpr int kTokenIndexLineBits = 20;
inline constexpr int kTokenIndexTokenBits = 12;
static_assert(kTokenIndexLineBits + kTokenIndexTokenBits == 32);
}  // namespace lexer_internal

// Maximum number of lines and tokens per line in the lexer.
inline constexpr int kMaxLexerLines =
    (1 << lexer_internal::kTokenIndexLineBits) - 1;
inline constexpr int kMaxTokensPerLine =
    (1 << lexer_internal::kTokenIndexTokenBits) - 2;  // -1 is kTokenEnd.
inline constexpr int kTokenIndexEndToken = kMaxTokensPerLine + 1;
static_assert(kTokenIndexEndToken <
              (1 << lexer_internal::kTokenIndexTokenBits));

// Location within lexer content. This can be retrieved from the Lexer given a
// token that was parsed from it, or directly from a line index. If it is an
// unknown token, or one not from the lexer, or the line is out of range, the
// values will default.
struct LexerLocation {
  LexerContentId id =
      kNoLexerContent;        // The id of the content within the lexer
  std::string_view filename;  // The filename of the content, if there is one
  int line = -1;              // The line number of the token (0 is first)
  int column = -1;            // The column number of the token (0 is first)

  auto operator<=>(const LexerLocation&) const = default;
};

template <typename Sink>
void AbslStringify(Sink& sink, const LexerLocation& location) {
  absl::Format(&sink, "{id:%d, filename:\"%s\", line:%d, col:%d}", location.id,
               location.filename, location.line, location.column);
}

// Represents the index of a token within the Lexer. It can be used to retrieve
// token text and location information from the Lexer.
//
// TokensIndexes are lightweight value classes that are strictly ordered across
// all tokens within the Lexer first by content ID and token order within the
// content. They are not valid to compare across different Lexers.
class TokenIndex {
 public:
  constexpr TokenIndex() = default;
  constexpr TokenIndex(uint32_t line, uint32_t token)
      : line(line), token(token) {}
  constexpr TokenIndex(const TokenIndex&) noexcept = default;
  constexpr TokenIndex& operator=(const TokenIndex&) noexcept = default;
  ~TokenIndex() = default;

  auto constexpr operator<=>(const TokenIndex&) const = default;

 private:
  friend class Lexer;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const TokenIndex& token_index) {
    absl::Format(&sink, "(%d:%d)", token_index.line, token_index.token);
  }

  uint32_t line : lexer_internal::kTokenIndexLineBits = 0;
  uint32_t token : lexer_internal::kTokenIndexTokenBits = 0;
};
static_assert(sizeof(TokenIndex) == sizeof(uint32_t));

// Explicit invalid token index. This is used to indicate an invalid token index
// and is never valid for use in a lexer.
inline constexpr TokenIndex kInvalidTokenIndex =
    TokenIndex(kMaxLexerLines, kMaxTokensPerLine);

}  // namespace gb

#endif  // GB_PARSE_PARSE_TYPES_H_
