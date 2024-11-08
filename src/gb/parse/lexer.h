// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_LEXER_H_
#define GB_PARSE_LEXER_H_

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "gb/base/flags.h"
#include "gb/parse/lexer_config.h"
#include "gb/parse/lexer_program.h"
#include "gb/parse/parse_types.h"
#include "gb/parse/token.h"
#include "re2/re2.h"

namespace gb {

// This class is used to parse text content into tokens.
//
// The lexer is configured with a LexerConfig, which determines how the lexer
// will parse the text. The lexer can be used to parse multiple files or
// file-less text blocks identified as separate "lexer content", with each
// identified by a LexerContentId.
//
// The Lexer assumes several base syntax rules that are common to most
// languages and cannot be configured:
//   - Whitespace is any sequence of space or tab characters.
//   - Newlines are optionally whitespace or their own token. Tokenization does
//     not cross line boundaries.
//   - Normal tokens must be separated by whitespace or symbols.
//   - Symbols must be explicitly defined, and may be any sequence of up to 7
//     ASCII characters that are not whitespace or control characters (best
//     practice is they do not match any other token, but this is not a hard
//     requirement).
//   - Symbols are matched before other tokens after a non-symbol token is
//     parsed, and other tokens are matched before symbols after a symbol is
//     parsed (or at the beginning of content). This disambiguates between the
//     common issue of symbols (like '-') which match the beginning of tokens
//     (like '-2'), so "-3-2" becomes {"-3", "-", " 2"} not {"-3", "-2"} or
//     {"-", "3", "-", "2"}. Numbers may also be configured to only be positive
//     in case the latter result is what is desired.
//   - Characters encountered that do not match any symbol or whitespace, and
//     are illegal within a normal token result in an error token. The error
//     token extends up to the next encountered whitespace or symbol start
//     character). This results in more natural error token results in general,
//     allowing token skipping and better error reporting using the token text
//     as context.
//   - Floating point numbers cannot start or end with a '.' (unlike C/C++)
//     There must be digits on both sides of a period.
//   - Non-decimal integer formats (hexadecimal, octal, binary) do not support
//     explicit negation (although if large enough relative to the specified
//     max bit depth, they will be negative when converted to 2's compliment).
//   - Token types are determined by the longest match, and in the case of a
//     tie in the following order: binary integer, octal integer, decimal
//     integer, hexadecimal integer, floating point number, character, string,
//     reserved word, and identifier. As described above, symbols are matched
//     either first or last depending on the previous token.
//
// The Lexer maintains ownership of all source added to it, and provides access
// via Tokens (as defined by LexerConfig), and Lines which are views into the
// data (zero copy). It is stateful, tracking the current line and token
// position within each content, and can be rewound or parsed by token or line.
//
// It also provides built in transformations for built-in token types like
// decoding escaping on string constants or normalizing identifiers (for
// case-insensitive tokenization).
//
// Tokens are parsed on demand, allowing for mixed-mode parsing where lines can
// be parsed individually, or tokens can be parsed from the current line. Random
// access is also supported at the line level, and supports reseting to a
// specific line number in the content.
//
// Example:
//
//   // Set up a configuration for a lexer.
//   LexerConfig config;
//   config.flags = {
//       // Specific flags for custom control over number parsing.
//       LexerFlag::kInt32, LexerFlag::kNegativeIntegers,
//       LexerFlag::kDecimalIntegers,
//
//       // Bundled options to follow C style identifiers
//       kLexerFlags_CIdentifiers};
//   const Symbol kSymbols[] = {'{', '}', ':', ',', ".."};
//   config.symbols = kSymbols;
//
//   // Create the lexer with the configuration.
//   auto lexer = Lexer::Create(config);
//   LexerContentId content_id = lexer->AddFileContent(
//       "filename.txt",
//       "list: {one: 1, two: 2}\n"
//       "range: -100..100\n");
//
//   // Parse the tokens!
//   int line = 0;
//   for (Token token = lexer->NextToken(content_id); token.GetType() !=
//   kTokenEnd;
//        token = lexer->NextToken(content_id)) {
//     if (lexer->GetCurrentLine(content_id) > line) {
//       std::cout << std::endl;
//       ++line;
//     }
//     std::cout << static_cast<int>(token.GetType()) << "("
//               << lexer->GetTokenText(token) << ") ";
//   }
//
// This outputs:
//   9(list) 3(:) 3({) 9(one) 3(:) 4(1) 3(,) 9(two) 3(:) 4(2) 3(})
//   9(range) 3(:) 4(-100) 3(..) 4(100)
//
// This class is thread-compatible.
class Lexer final {
 public:
  //----------------------------------------------------------------------------
  // Token error strings
  //
  // These may be returned as the message of a Token of type kTokenError.
  //----------------------------------------------------------------------------

  // Internal error occured in the lexer (a bug) .
  static const std::string_view kErrorInternal;

  // The token referred to invalid content, or content not associated with this
  // Lexer instance. This is only returned by ParseToken which takes a
  // TokenIndex.
  static const std::string_view kErrorInvalidTokenContent;

  // An invalid token was encountered when parsing tokens. This is due to an
  // unknown character or other type of malformed token.
  static const std::string_view kErrorInvalidToken;

  // An invalid integer was encountered when parsing tokens. This is due to the
  // integer being out of range of the min/max values based on the bit-depth.
  // Decimal integers are always signed (even if only positive), and binary,
  // octal, and hex integers are always unsigned.
  static const std::string_view kErrorInvalidInteger;

  // An invalid float was encountered when parsing tokens. This is due to the
  // floating point value being out of range of the min/max values based on the
  // bit-depth. It also may be returned if the floating point requires precision
  // beyond what is supported by the bit-depth.
  static const std::string_view kErrorInvalidFloat;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new Lexer with the specified configuration.
  //
  // If the configuration is invalid, this will return nullptr, and if an error
  // string is provided, it will be set to the error message.
  static std::unique_ptr<Lexer> Create(const LexerConfig& lexer_config,
                                       std::string* error_message = nullptr);

  Lexer(const Lexer&) = delete;
  Lexer& operator=(const Lexer&) = delete;
  ~Lexer() = default;

  //----------------------------------------------------------------------------
  // Lexer configuration
  //----------------------------------------------------------------------------

  // Returns the flags used to configure the lexer.
  LexerFlags GetFlags() const { return state_.flags; }

  // Returns any defined names of user tokens.
  const TokenTypeNames& GetUserTokenNames() const {
    return state_.user_token_names;
  }

  // Returns true if the specified token type is a valid token type for this
  // lexer.
  bool IsValidTokenType(TokenType token_type) const;

  //----------------------------------------------------------------------------
  // Content management
  //----------------------------------------------------------------------------

  // Adds a file to the lexer, returning the LexerContentId which can be used to
  // parse the file.
  //
  // If the text exceeds the capacity of the Lexer, this will return
  // kNoLexerContent. Lexer content cannot exceed kMaxLexerLines - content_count
  // lines across *all* content loaded in the lexer. Each line is further
  // limited to kMaxTokensPerLine characters long (including newline).
  LexerContentId AddFileContent(std::string_view filename, std::string text);

  // Adds text content without a specific filename to the lexer. Returns the
  // LexerContentId which can be used to parse the content.
  //
  // If the text exceeds the capacity of the Lexer, this will return
  // kNoLexerContent. Lexer content cannot exceed kMaxLexerLines - content_count
  // lines across *all* content loaded in the lexer. Each line is further
  // limited to kMaxTokensPerLine characters long (including newline).
  LexerContentId AddContent(std::string text);

  // Rewinds the content to the beginning. NextToken() and NextLine() will
  // return the first token and line respectively in the content.
  void RewindContent(LexerContentId id);

  // Returns the content that was previously added for the given filename. If
  // there is no content, this returns zero.
  LexerContentId GetFileContentId(std::string_view filename) const;

  // Returns the filename associated with the specified content ID, or an empty
  // string if the content ID is invalid or it has no filename.
  std::string_view GetContentFilename(LexerContentId id) const;

  // Returns the text associated with the specified content ID.
  //
  // This is the full text of the content, including any newlines. If the
  // content ID is invalid, this returns an empty string.
  std::string_view GetContentText(LexerContentId id) const;

  //----------------------------------------------------------------------------
  // Line properties
  //----------------------------------------------------------------------------

  // Returns the number of lines in the content, or 0 if the content ID is
  // invalid.
  int GetLineCount(LexerContentId id) const;

  // Returns the text of the line at the specified index (excluding any newline
  // at the end), or an empty string if the content ID is invalid or the line
  // index is out of range.
  std::string_view GetLineText(LexerContentId id, int line_index) const;

  // Returns the location for the specified line number in the content. If the
  // line number is out of range, this returns a default constructed (invalid)
  // location.
  LexerLocation GetLineLocation(LexerContentId id, int line_index) const;

  //----------------------------------------------------------------------------
  // Line parsing
  //----------------------------------------------------------------------------

  // Returns the current line number in the content, or -1 if the content ID is
  // invalid.
  int GetCurrentLine(LexerContentId id) const;

  // Returns the content remaining on the current line (not including newline),
  // and advances to the next line in the content. If the content is already at
  // the end, this will return an empty string.
  //
  // After this call, if NextToken() is called, it will return the first token
  // that appears after the beginning of the line.
  std::string_view NextLine(LexerContentId id);

  // Rewinds the content to the beginning of the current line.
  //
  // If this is currently at the beginning of a line, it will rewind to the
  // beginning of the previous line. If the content is already at the beginning
  // of the content, this will return false, otherwise it will return true.
  //
  // After this call if NextToken() is called, it will return the first token
  // that appears after the beginning of the line.
  bool RewindLine(LexerContentId id);

  //----------------------------------------------------------------------------
  // Token properties
  //----------------------------------------------------------------------------

  // Returns the token location for the token or token index.
  //
  // The location is to the start of the token (or token index). If the token or
  // token index is invalid, this returns a default constructed (invalid)
  // location.
  LexerLocation GetTokenLocation(const Token& token) const;
  LexerLocation GetTokenLocation(TokenIndex index) const;

  // Returns the raw text of the token or token index.
  //
  // Returns an empty string if the token or start token index is invalid.
  std::string_view GetTokenText(const Token& token) const;
  std::string_view GetTokenText(TokenIndex index) const;

  //----------------------------------------------------------------------------
  // Token parsing
  //----------------------------------------------------------------------------

  // Sets the next token to be parsed to the specified token index.
  //
  // If the token is the end token, error token, or it was not a token
  // previously parsed by this Lexer, then this will fail and return false.
  bool SetNextToken(Token token);

  // Parses the token at the specified token index.
  //
  // This always returns a token. If the token index is invalid, then this will
  // return a token of type kTokenError, and it will contain the error message.
  //
  // The returned token is valid for as long as the Lexer is valid.
  Token ParseToken(TokenIndex index);

  // Parses the next token in the stream for the content.
  //
  // This always returns a token, even if there are no more tokens in the
  // stream. In that case, the token will have type kTokenEnd. If there was a
  // parsing error, the token will have type kTokenError. Otherwise, the token
  // will have the appropriate type and value as determine by the LexerConfig
  // settings.
  //
  // The returned token is valid for as long as the Lexer is valid.
  Token NextToken(LexerContentId id, bool advance = true);

  // Rewinds the token stream by one token.
  //
  // This can be called multiple times rewinding through previously parsed
  // tokens. This does not "parse backward" so if lines were skipped (with
  // NextLine), or tokens only partially parsed from a previous line, this will
  // simply go back to the previous parsed token in the content. If the token
  // stream is rewound past the beginning, it will be clamped to the beginning
  // and this will return also false. NextToken() will return the token that was
  // rewound to.
  bool RewindToken(LexerContentId id);

  //----------------------------------------------------------------------------
  // Standalone token parsing
  //----------------------------------------------------------------------------

  // Parses the specified token text. The resulting token is not part of any
  // content, has s defaulted token index, and so is invalid to use with
  // SetNextToken.
  Token ParseTokenText(std::string_view token_text) const;

 private:
  enum class ReOrder { kSymLast, kSymFirst };

  struct TokenInfo {
    uint32_t column : 12;
    uint32_t size : 12;
    uint32_t type : 8;
  };
  static_assert(sizeof(TokenInfo) == sizeof(uint32_t));

  struct Line {
    Line(LexerContentId in_id, std::string_view in_line)
        : id(in_id), line(in_line), remain(in_line) {}
    LexerContentId id = 0;
    std::string_view line;
    std::string_view remain;
    std::vector<TokenInfo> tokens;
  };

  struct Content {
    Content(std::string_view filename, std::string content)
        : filename(filename), text(std::move(content)) {}

    int GetLineIndex() const { return start_line + line; }
    TokenIndex GetTokenIndex() const;

    std::string filename;
    std::string text;
    int start_line = 0;
    int end_line = 0;

    int line = 0;
    int token = 0;
    ReOrder re_order = ReOrder::kSymLast;
  };

  using IntParseType = LexerProgram::IntParseType;

  struct TokenArg {
    TokenArg() : arg(&text) {}
    TokenType type = kTokenNone;
    IntParseType int_parse_type = IntParseType::kDefault;
    std::string_view text;
    RE2::Arg arg;
  };

  explicit Lexer(std::shared_ptr<const LexerProgram> program);

  LexerContentId GetContentId(int index) const;
  int GetContentIndex(LexerContentId id) const;

  const Content* GetContent(LexerContentId id) const;
  Content* GetContent(LexerContentId id);

  const Line* GetLine(int line) const;
  Line* GetLine(int line);

  std::tuple<Content*, Line*> GetContentLine(LexerContentId id);

  Token ParseInt(TokenIndex token_index, std::string_view text,
                 IntParseType int_parse_type) const;
  Token ParseFloat(TokenIndex token_index, std::string_view text) const;
  Token ParseChar(TokenIndex token_index, std::string_view text) const;
  Token ParseString(TokenIndex token_index, std::string_view text) const;
  Token ParseKeyword(TokenIndex token_index, std::string_view text) const;
  Token ParseIdent(TokenIndex token_index, std::string_view text) const;
  Token ParseUserToken(TokenIndex token_index, TokenType token_type,
                       std::string_view text) const;

  Token ParseNextSymbol(Content* content, Line* line, bool advance);
  Token ParseNextToken(Content* content, Line* line, bool advance);

  // Set at initialization.
  std::shared_ptr<const LexerProgram> program_;
  const LexerProgram::State& state_;

  // Working state for parsing.
  std::vector<TokenArg> re_args_;
  std::vector<RE2::Arg*> re_token_args_;
  std::vector<std::unique_ptr<Content>> content_;
  absl::flat_hash_map<std::string_view, LexerContentId> filename_to_id_;
  std::vector<Line> lines_;
  Token last_token_;

  // Cache state for token content that differs from the original content
  // (forced case, escaping, etc).
  mutable std::vector<std::unique_ptr<std::string>> modified_text_;
};

}  // namespace gb

#endif  // GB_PARSE_LEXER_H_
