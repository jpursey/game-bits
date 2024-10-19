// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer.h"

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

namespace gb {

const std::string_view Lexer::kErrorDuplicateSymbolSpec =
    "Duplicate symbol specification";
const std::string_view Lexer::kErrorInvalidSymbolSpec =
    "Symbol specification has non-ASCII or whitespace characters";
const std::string_view Lexer::kErrorConflictingStringAndCharSpec =
    "Character and String specifications share the same quote type";
const std::string_view Lexer::kErrorConflictingIdentifierSpec =
    "Identifiers cannot be set to force both lower and upper case";
const std::string_view Lexer::kErrorConflictingCommentSpec =
    "Multiple line and/or block comment starts share a common prefix";
const std::string_view Lexer::kErrorEmptyCommentSpec =
    "Empty string used in line or block comment specification";
const std::string_view Lexer::kErrorDuplicateKeywordSpec =
    "Duplicate keyword specification";
const std::string_view Lexer::kErrorEmptyKeywordSpec =
    "Empty string used in keyword specification";
const std::string_view Lexer::kErrorNoTokenSpec =
    "No token specification (from symbols, keywords, or flags)";

const std::string_view Lexer::kErrorInternal = "Internal error";
const std::string_view Lexer::kErrorInvalidTokenContent =
    "Token does not refer to valid content";
const std::string_view Lexer::kErrorInvalidToken = "Invalid token";
const std::string_view Lexer::kErrorInvalidInteger = "Invalid integer";
const std::string_view Lexer::kErrorInvalidFloat = "Invalid float";

namespace {

bool CreateTokenPattern(std::string& token_pattern,
                        const LexerConfig& lexer_config, std::string& error) {
  const LexerFlags flags = lexer_config.flags;

  if (LexerSupportsIntegers(flags)) {
    bool first = true;
    if (flags.IsSet(LexerFlag::kBinaryIntegers)) {
      DCHECK(first);
      first = false;
      absl::StrAppend(&token_pattern, "(",
                      RE2::QuoteMeta(lexer_config.binary_prefix), "[01]+",
                      RE2::QuoteMeta(lexer_config.binary_suffix), ")");
    }
    if (flags.IsSet(LexerFlag::kOctalIntegers)) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      first = false;
      absl::StrAppend(&token_pattern, "(",
                      RE2::QuoteMeta(lexer_config.octal_prefix), "[0-7]+",
                      RE2::QuoteMeta(lexer_config.octal_suffix), ")");
    }
    if (flags.IsSet(LexerFlag::kDecimalIntegers)) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      first = false;
      absl::StrAppend(&token_pattern, "(",
                      RE2::QuoteMeta(lexer_config.decimal_prefix));
      if (flags.IsSet(LexerFlag::kNegativeIntegers)) {
        absl::StrAppend(&token_pattern, "-?");
      }
      absl::StrAppend(&token_pattern, "[0-9]+",
                      RE2::QuoteMeta(lexer_config.decimal_suffix), ")");
    }
    if (flags.Intersects(
            {LexerFlag::kHexUpperIntegers, LexerFlag::kHexLowerIntegers})) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      absl::StrAppend(&token_pattern, "(",
                      RE2::QuoteMeta(lexer_config.hex_prefix), "[0-9");
      if (flags.IsSet(LexerFlag::kHexUpperIntegers)) {
        absl::StrAppend(&token_pattern, "A-F");
      }
      if (flags.IsSet(LexerFlag::kHexLowerIntegers)) {
        absl::StrAppend(&token_pattern, "a-f");
      }
      absl::StrAppend(&token_pattern, "]+",
                      RE2::QuoteMeta(lexer_config.hex_suffix), ")");
    }
    absl::StrAppend(&token_pattern, "|");
  }

  if (LexerSupportsFloats(flags)) {
    absl::StrAppend(&token_pattern, "(",
                    RE2::QuoteMeta(lexer_config.float_prefix));
    if (flags.IsSet(LexerFlag::kNegativeFloats)) {
      absl::StrAppend(&token_pattern, "-?");
    }
    absl::StrAppend(&token_pattern, "[0-9]+(?:\\.[0-9]+)?");
    if (flags.IsSet(LexerFlag::kExponentFloats)) {
      absl::StrAppend(&token_pattern, "(?:[eE][-+]?[0-9]+)");
      if (flags.IsSet(LexerFlag::kDecimalFloats)) {
        absl::StrAppend(&token_pattern, "?");
      }
    }
    absl::StrAppend(&token_pattern, RE2::QuoteMeta(lexer_config.float_suffix),
                    ")|");
  }

  if (flags.IsSet(
          {LexerFlag::kDoubleQuoteCharacter, LexerFlag::kDoubleQuoteString}) ||
      flags.IsSet(
          {LexerFlag::kSingleQuoteCharacter, LexerFlag::kSingleQuoteString})) {
    error = Lexer::kErrorConflictingStringAndCharSpec;
    return false;
  }

  std::string escape_char;
  std::string escape_hex;
  if (flags.IsSet(LexerFlag::kEscapeCharacter) && lexer_config.escape != 0) {
    escape_char = RE2::QuoteMeta(std::string_view(&lexer_config.escape, 1));
    if (lexer_config.escape_hex != 0) {
      escape_hex = absl::StrCat(
          escape_char,
          RE2::QuoteMeta(std::string_view(&lexer_config.escape_hex, 1)),
          "[0-9a-fA-F]{2}");
    }
  }

  if (LexerSupportsCharacters(flags)) {
    absl::StrAppend(&token_pattern, "(");
    auto quote_re = [&](std::string_view quote) {
      absl::StrAppend(&token_pattern, "(?:", quote, "(?:");
      absl::StrAppend(&token_pattern, "[^", quote, escape_char, "]");
      if (flags.IsSet(LexerFlag::kQuoteQuoteEscape)) {
        absl::StrAppend(&token_pattern, "|", quote, quote);
      }
      if (flags.IsSet(LexerFlag::kEscapeCharacter) &&
          lexer_config.escape != 0) {
        if (lexer_config.escape_hex) {
          absl::StrAppend(&token_pattern, "|", escape_hex);
        }
        absl::StrAppend(&token_pattern, "|", escape_char, ".");
      }
      absl::StrAppend(&token_pattern, ")", quote, ")");
    };
    if (flags.IsSet(LexerFlag::kDoubleQuoteCharacter)) {
      quote_re("\"");
    }
    if (flags.IsSet(LexerFlag::kSingleQuoteCharacter)) {
      if (flags.IsSet(LexerFlag::kDoubleQuoteCharacter)) {
        absl::StrAppend(&token_pattern, "|");
      }
      quote_re("'");
    }
    absl::StrAppend(&token_pattern, ")|");
  }

  if (LexerSupportsStrings(flags)) {
    absl::StrAppend(&token_pattern, "(");
    auto quote_re = [&](std::string_view quote) {
      absl::StrAppend(&token_pattern, "(?:", quote, "(?:");
      absl::StrAppend(&token_pattern, "[^", quote, escape_char, "]");
      if (flags.IsSet(LexerFlag::kQuoteQuoteEscape)) {
        absl::StrAppend(&token_pattern, "|", quote, quote);
      }
      if (flags.IsSet(LexerFlag::kEscapeCharacter) &&
          lexer_config.escape != 0) {
        if (lexer_config.escape_hex) {
          absl::StrAppend(&token_pattern, "|", escape_hex);
        }
        absl::StrAppend(&token_pattern, "|", escape_char, ".");
      }
      absl::StrAppend(&token_pattern, ")*", quote, ")");
    };
    if (flags.IsSet(LexerFlag::kDoubleQuoteString)) {
      quote_re("\"");
    }
    if (flags.IsSet(LexerFlag::kSingleQuoteString)) {
      if (flags.IsSet(LexerFlag::kDoubleQuoteString)) {
        absl::StrAppend(&token_pattern, "|");
      }
      quote_re("'");
    }
    absl::StrAppend(&token_pattern, ")|");
  }

  if (!lexer_config.keywords.empty()) {
    absl::flat_hash_set<std::string_view> keywords;
    absl::StrAppend(&token_pattern, "(");
    if (flags.IsSet(LexerFlag::kKeywordCaseInsensitive)) {
      absl::StrAppend(&token_pattern, "(?i)");
    }
    for (const std::string_view& keyword : lexer_config.keywords) {
      if (keyword.empty()) {
        error = Lexer::kErrorEmptyKeywordSpec;
        return false;
      }
      if (!keywords.insert(keyword).second) {
        error = Lexer::kErrorDuplicateKeywordSpec;
        return false;
      }
      absl::StrAppend(&token_pattern, RE2::QuoteMeta(keyword), "|");
    }
    token_pattern.pop_back();
    absl::StrAppend(&token_pattern, ")|");
  }

  if (LexerSupportsIdentifiers(flags)) {
    if (flags.IsSet(
            {LexerFlag::kIdentForceUpper, LexerFlag::kIdentForceLower})) {
      error = Lexer::kErrorConflictingIdentifierSpec;
      return false;
    }
    absl::StrAppend(&token_pattern, "(",
                    RE2::QuoteMeta(lexer_config.ident_prefix), "[");
    if (flags.IsSet(LexerFlag::kIdentUnderscore) &&
        !flags.IsSet(LexerFlag::kIdentNonLeadUnderscore)) {
      absl::StrAppend(&token_pattern, "_");
    }
    if (flags.IsSet(LexerFlag::kIdentDigit) &&
        !flags.IsSet(LexerFlag::kIdentNonLeadDigit)) {
      absl::StrAppend(&token_pattern, "0-9");
    }
    if (flags.Intersects({LexerFlag::kIdentUpper, LexerFlag::kIdentForceLower,
                          LexerFlag::kIdentForceUpper})) {
      absl::StrAppend(&token_pattern, "A-Z");
    }
    if (flags.Intersects({LexerFlag::kIdentLower, LexerFlag::kIdentForceLower,
                          LexerFlag::kIdentForceUpper})) {
      absl::StrAppend(&token_pattern, "a-z");
    }
    absl::StrAppend(&token_pattern, "][");
    if (flags.Intersects({LexerFlag::kIdentUnderscore,
                          LexerFlag::kIdentNonLeadUnderscore})) {
      absl::StrAppend(&token_pattern, "_");
    }
    if (flags.Intersects(
            {LexerFlag::kIdentDigit, LexerFlag::kIdentNonLeadDigit})) {
      absl::StrAppend(&token_pattern, "0-9");
    }
    if (flags.Intersects({LexerFlag::kIdentUpper, LexerFlag::kIdentForceLower,
                          LexerFlag::kIdentForceUpper})) {
      absl::StrAppend(&token_pattern, "A-Z");
    }
    if (flags.Intersects({LexerFlag::kIdentLower, LexerFlag::kIdentForceLower,
                          LexerFlag::kIdentForceUpper})) {
      absl::StrAppend(&token_pattern, "a-z");
    }
    absl::StrAppend(&token_pattern, "]*",
                    RE2::QuoteMeta(lexer_config.ident_suffix), ")|");
  }

  if (!token_pattern.empty()) {
    token_pattern.pop_back();
  }

  return true;
}

bool CreateSymbolPattern(std::string& symbol_pattern, std::string& symbol_chars,
                         const LexerConfig& config, std::string& error) {
  if (config.symbols.empty()) {
    return true;
  }
  absl::flat_hash_set<Symbol> symbols;
  absl::StrAppend(&symbol_pattern, "(");
  for (const Symbol& symbol : config.symbols) {
    if (!symbol.IsValid()) {
      error = Lexer::kErrorInvalidSymbolSpec;
      return false;
    }
    if (!symbols.insert(symbol).second) {
      error = Lexer::kErrorDuplicateSymbolSpec;
      return false;
    }
    const char first_char = symbol.GetString()[0];
    if (symbol_chars.find(first_char) == std::string::npos) {
      symbol_chars.push_back(first_char);
    }
    absl::StrAppend(&symbol_pattern, RE2::QuoteMeta(symbol.GetString()));
    absl::StrAppend(&symbol_pattern, "|");
  }
  symbol_pattern.back() = ')';
  return true;
}

bool CreateWhitespacePattern(std::string& whitespace_pattern,
                             std::string& whitespace_chars,
                             const LexerConfig& config, std::string& error) {
  absl::flat_hash_set<std::string_view> comment_starts;
  if (whitespace_chars.find(' ') == std::string::npos) {
    whitespace_chars.push_back(' ');
  }
  if (whitespace_chars.find('\t') == std::string::npos) {
    whitespace_chars.push_back('\t');
  }
  if (config.block_comments.empty()) {
    whitespace_pattern = "[ \\t]*";
  } else {
    absl::StrAppend(&whitespace_pattern, "(?:[ \t]|");
    for (const auto& [start, end] : config.block_comments) {
      if (start.empty() || end.empty()) {
        error = Lexer::kErrorEmptyCommentSpec;
        return false;
      }
      if (!comment_starts.insert(start).second) {
        error = Lexer::kErrorConflictingCommentSpec;
        return false;
      }
      const char first_char = start[0];
      if (whitespace_chars.find(first_char) == std::string::npos) {
        whitespace_chars.push_back(first_char);
      }
      absl::StrAppend(&whitespace_pattern, RE2::QuoteMeta(start), ".*?",
                      RE2::QuoteMeta(end), "|");
    }
    whitespace_pattern.pop_back();
    absl::StrAppend(&whitespace_pattern, ")*");
  }
  if (!config.line_comments.empty()) {
    absl::StrAppend(&whitespace_pattern, "(?:(?:");
    for (const std::string_view& line_comment : config.line_comments) {
      if (line_comment.empty()) {
        error = Lexer::kErrorEmptyCommentSpec;
        return false;
      }
      if (!comment_starts.insert(line_comment).second) {
        error = Lexer::kErrorConflictingCommentSpec;
        return false;
      }
      const char first_char = line_comment[0];
      if (whitespace_chars.find(first_char) == std::string::npos) {
        whitespace_chars.push_back(first_char);
      }
      absl::StrAppend(&whitespace_pattern, RE2::QuoteMeta(line_comment), "|");
    }
    whitespace_pattern.pop_back();
    absl::StrAppend(&whitespace_pattern, ").*)?");
  }
  return true;
}

}  // namespace

std::unique_ptr<Lexer> Lexer::Create(const LexerConfig& lexer_config,
                                     std::string* error_message) {
  std::string temp_error;
  std::string& error = (error_message != nullptr ? *error_message : temp_error);
  error.clear();

  Config config;
  config.flags = lexer_config.flags;

  if (!CreateTokenPattern(config.token_pattern, lexer_config, error)) {
    return nullptr;
  }

  int index = 0;
  if (LexerSupportsIntegers(lexer_config.flags)) {
    if (lexer_config.flags.IsSet(LexerFlag::kBinaryIntegers)) {
      config.binary_index = index++;
    }
    if (lexer_config.flags.IsSet(LexerFlag::kOctalIntegers)) {
      config.octal_index = index++;
    }
    if (lexer_config.flags.IsSet(LexerFlag::kDecimalIntegers)) {
      config.decimal_index = index++;
    }
    if (lexer_config.flags.Intersects(
            {LexerFlag::kHexUpperIntegers, LexerFlag::kHexLowerIntegers})) {
      config.hex_index = index++;
    }
  }
  if (LexerSupportsFloats(lexer_config.flags)) {
    config.float_index = index++;
  }
  if (LexerSupportsCharacters(lexer_config.flags)) {
    config.char_index = index++;
  }
  if (LexerSupportsStrings(lexer_config.flags)) {
    config.string_index = index++;
  }
  if (!lexer_config.keywords.empty()) {
    config.keyword_index = index++;
    if (lexer_config.flags.IsSet(LexerFlag::kKeywordCaseInsensitive)) {
      config.keywords = lexer_config.keywords;
    }
  }
  if (LexerSupportsIdentifiers(lexer_config.flags)) {
    config.ident_index = index++;
  }
  config.token_pattern_count = index;

  std::string token_end_chars;
  if (!CreateSymbolPattern(config.symbol_pattern, token_end_chars, lexer_config,
                           error)) {
    return nullptr;
  }

  if (config.token_pattern.empty() && config.symbol_pattern.empty()) {
    error = Lexer::kErrorNoTokenSpec;
    return nullptr;
  }

  if (!CreateWhitespacePattern(config.whitespace_pattern, token_end_chars,
                               lexer_config, error)) {
    return nullptr;
  }

  token_end_chars = RE2::QuoteMeta(token_end_chars);
  config.token_end_pattern = absl::StrCat("[", token_end_chars, "]+");
  config.not_token_end_pattern = absl::StrCat("[^", token_end_chars, "]*");

  config.binary_config.prefix = lexer_config.binary_prefix.size();
  config.binary_config.size_offset =
      config.binary_config.prefix + lexer_config.binary_suffix.size();
  config.octal_config.prefix = lexer_config.octal_prefix.size();
  config.octal_config.size_offset =
      config.octal_config.prefix + lexer_config.octal_suffix.size();
  config.decimal_config.prefix = lexer_config.decimal_prefix.size();
  config.decimal_config.size_offset =
      config.decimal_config.prefix + lexer_config.decimal_suffix.size();
  config.hex_config.prefix = lexer_config.hex_prefix.size();
  config.hex_config.size_offset =
      config.hex_config.prefix + lexer_config.hex_suffix.size();
  config.float_config.prefix = lexer_config.float_prefix.size();
  config.float_config.size_offset =
      config.float_config.prefix + lexer_config.float_suffix.size();
  config.ident_config.prefix = lexer_config.ident_prefix.size();
  config.ident_config.size_offset =
      config.ident_config.prefix + lexer_config.ident_suffix.size();
  config.escape = lexer_config.escape;
  config.escape_newline = lexer_config.escape_newline;
  config.escape_tab = lexer_config.escape_tab;
  config.escape_hex = lexer_config.escape_hex;
  config.block_comments = lexer_config.block_comments;
  return absl::WrapUnique(new Lexer(config));
}

namespace {
const RE2::Options Re2MatchLongest() {
  static absl::once_flag once;
  static RE2::Options options;
  absl::call_once(once, [] { options.set_longest_match(true); });
  return options;
}
}  // namespace

Lexer::Lexer(const Config& config)
    : flags_(config.flags),
      re_whitespace_(config.whitespace_pattern),
      re_symbol_(config.symbol_pattern, Re2MatchLongest()),
      re_token_end_(config.token_end_pattern),
      re_not_token_end_(config.not_token_end_pattern),
      re_token_(config.token_pattern, Re2MatchLongest()),
      binary_config_(config.binary_config),
      octal_config_(config.octal_config),
      decimal_config_(config.decimal_config),
      hex_config_(config.hex_config),
      float_config_(config.float_config),
      ident_config_(config.ident_config),
      escape_(config.escape),
      escape_newline_(config.escape_newline),
      escape_tab_(config.escape_tab),
      escape_hex_(config.escape_hex) {
  re_args_.resize(config.token_pattern_count);
  re_token_args_.resize(config.token_pattern_count);
  if (config.binary_index >= 0) {
    const int i = config.binary_index;
    re_args_[i].type = kTokenInt;
    re_args_[i].int_parse_type = IntParseType::kBinary;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.octal_index >= 0) {
    const int i = config.octal_index;
    re_args_[i].type = kTokenInt;
    re_args_[i].int_parse_type = IntParseType::kOctal;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.decimal_index >= 0) {
    const int i = config.decimal_index;
    re_args_[i].type = kTokenInt;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.hex_index >= 0) {
    const int i = config.hex_index;
    re_args_[i].type = kTokenInt;
    re_args_[i].int_parse_type = IntParseType::kHex;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.float_index >= 0) {
    const int i = config.float_index;
    re_args_[config.float_index].type = kTokenFloat;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.char_index >= 0) {
    const int i = config.char_index;
    re_args_[config.char_index].type = kTokenChar;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.string_index >= 0) {
    const int i = config.string_index;
    re_args_[config.string_index].type = kTokenString;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.keyword_index >= 0) {
    const int i = config.keyword_index;
    re_args_[config.keyword_index].type = kTokenKeyword;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.ident_index >= 0) {
    const int i = config.ident_index;
    re_args_[config.ident_index].type = kTokenIdentifier;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (LexerSupportsIntegers(flags_)) {
    if (flags_.IsSet(LexerFlag::kInt64)) {
      max_int_ = std::numeric_limits<int64_t>::max();
      min_int_ = std::numeric_limits<int64_t>::min();
      int_sign_extend_ = 0;
    } else if (flags_.IsSet(LexerFlag::kInt32)) {
      max_int_ = std::numeric_limits<int32_t>::max();
      min_int_ = std::numeric_limits<int32_t>::min();
      int_sign_extend_ = 0xFFFFFFFF00000000;
    } else if (flags_.IsSet(LexerFlag::kInt16)) {
      max_int_ = std::numeric_limits<int16_t>::max();
      min_int_ = std::numeric_limits<int16_t>::min();
      int_sign_extend_ = 0xFFFFFFFFFFFF0000;
    } else if (flags_.IsSet(LexerFlag::kInt8)) {
      max_int_ = std::numeric_limits<int8_t>::max();
      min_int_ = std::numeric_limits<int8_t>::min();
      int_sign_extend_ = 0xFFFFFFFFFFFFFF00;
    }
  }
  if (!config.keywords.empty()) {
    DCHECK(flags_.IsSet(LexerFlag::kKeywordCaseInsensitive));
    for (const std::string_view& keyword : config.keywords) {
      keywords_[absl::AsciiStrToLower(keyword)] = keyword;
    }
  }
  for (const auto& [start, end] : config.block_comments) {
    block_comments_.emplace_back(std::string(start), std::string(end));
  }
}

inline TokenIndex Lexer::Content::GetTokenIndex() const {
  // We can't return the normal token index here, as it is would refer to the
  // first token in the next content, which we need to distinguish from. We do
  // this by returning a token index that is one past the end of the last token
  // possible in this content.
  if (line >= end_line) {
    return {static_cast<uint32_t>(end_line - 1),
            static_cast<uint32_t>(kTokenIndexEndToken)};
  }
  return {static_cast<uint32_t>(start_line + line),
          static_cast<uint32_t>(token)};
}

inline LexerContentId Lexer::GetContentId(int index) const { return index + 1; }

inline int Lexer::GetContentIndex(LexerContentId id) const { return id - 1; }

inline Lexer::Content* Lexer::GetContent(LexerContentId id) {
  const int index = GetContentIndex(id);
  if (index < 0 || index >= content_.size()) {
    return nullptr;
  }
  return content_[index].get();
}

inline const Lexer::Content* Lexer::GetContent(LexerContentId id) const {
  return const_cast<Lexer*>(this)->GetContent(id);
}

inline Lexer::Line* Lexer::GetLine(int line) {
  if (line < 0 || line >= lines_.size()) {
    return nullptr;
  }
  return &lines_[line];
}

inline const Lexer::Line* Lexer::GetLine(int line) const {
  return const_cast<Lexer*>(this)->GetLine(line);
}

std::tuple<Lexer::Content*, Lexer::Line*> Lexer::GetContentLine(
    LexerContentId id) {
  Content* content = GetContent(id);
  if (content == nullptr) {
    return {nullptr, nullptr};
  }
  if (content->line >= content->end_line) {
    return {content, nullptr};
  }
  return {content, &lines_[content->start_line + content->line]};
}

LexerContentId Lexer::AddFileContent(std::string_view filename,
                                     std::string text) {
  const LexerContentId id = GetContentId(content_.size());
  content_.emplace_back(std::make_unique<Content>(filename, std::move(text)));
  Content* content = content_.back().get();
  if (!filename.empty()) {
    filename_to_id_[filename] = id;
  }
  std::vector<std::string_view> lines = absl::StrSplit(content->text, '\n');
  if (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }
  // To have a valid kTokenEnd for a valid piece of content, we need at least
  // one line for every content, even if it is empty.
  if (lines.empty()) {
    lines.emplace_back();
  }

  // Enforce limits so that we don't overflow the token index.
  if (lines_.size() + lines.size() + 1 > kMaxLexerLines) {
    return kNoLexerContent;
  }
  for (const auto& line : lines) {
    if (line.size() >= kMaxTokensPerLine) {
      return kNoLexerContent;
    }
  }

  content->start_line = lines_.size();
  content->end_line = content->start_line + lines.size();
  lines_.reserve(content->end_line + 2);
  for (const auto& line : lines) {
    lines_.emplace_back(id, line);
  }
  lines_.emplace_back(kNoLexerContent, "");
  return id;
}

LexerContentId Lexer::AddContent(std::string text) {
  return AddFileContent("", std::move(text));
}

void Lexer::RewindContent(LexerContentId id) {
  Content* content = GetContent(id);
  if (content == nullptr) {
    return;
  }
  content->line = 0;
  content->token = 0;
}

LexerContentId Lexer::GetFileContentId(std::string_view filename) const {
  auto it = filename_to_id_.find(filename);
  if (it == filename_to_id_.end()) {
    return 0;
  }
  return it->second;
}

std::string_view Lexer::GetContentFilename(LexerContentId id) const {
  const Content* content = GetContent(id);
  if (content == nullptr) {
    return {};
  }
  return content->filename;
}

std::string_view Lexer::GetContentText(LexerContentId id) const {
  const Content* content = GetContent(id);
  if (content == nullptr) {
    return {};
  }
  return content->text;
}

int Lexer::GetLineCount(LexerContentId id) const {
  const Content* content = GetContent(id);
  if (content == nullptr) {
    return 0;
  }
  return content->end_line - content->start_line;
}

std::string_view Lexer::GetLineText(LexerContentId id, int line_index) const {
  const Content* content = GetContent(id);
  if (content == nullptr) {
    return {};
  }
  const Line* line = GetLine(content->start_line + line_index);
  if (line == nullptr) {
    return {};
  }
  return line->line;
}

LexerLocation Lexer::GetLineLocation(LexerContentId id, int line_index) const {
  const Content* content = GetContent(id);
  if (content == nullptr) {
    return {};
  }
  const Line* line = GetLine(content->start_line + line_index);
  if (line == nullptr || line->id != id) {
    return {};
  }
  return {.id = id,
          .filename = content->filename,
          .line = content->start_line + line_index,
          .column = 0};
}

int Lexer::GetCurrentLine(LexerContentId id) const {
  const Content* content = GetContent(id);
  if (content == nullptr) {
    return -1;
  }
  return content->line;
}

std::string_view Lexer::NextLine(LexerContentId id) {
  auto [content, line] = GetContentLine(id);
  if (line == nullptr) {
    return {};
  }
  std::string_view result = line->remain;
  ++content->line;
  content->token = 0;
  return result;
}

bool Lexer::RewindLine(LexerContentId id) {
  Content* content = GetContent(id);
  if (content == nullptr) {
    return false;
  }
  if (content->token > 0) {
    content->token = 0;
    return true;
  }
  if (content->line <= content->start_line) {
    return false;
  }
  --content->line;
  return true;
}

LexerLocation Lexer::GetTokenLocation(const Token& token) const {
  return GetTokenLocation(token.token_index_);
}

LexerLocation Lexer::GetTokenLocation(TokenIndex index) const {
  const Line* line = GetLine(index.line);
  if (line == nullptr) {
    return {};
  }
  int column;
  if (index.token < line->tokens.size()) {
    column = static_cast<int>(line->tokens[index.token].column);
  } else if (index.token == kTokenIndexEndToken) {
    column = line->line.size();
  } else {
    return {};
  }
  return {.id = line->id,
          .filename = content_[GetContentIndex(line->id)]->filename,
          .line = static_cast<int>(index.line),
          .column = column};
}

std::string_view Lexer::GetTokenText(const Token& token) const {
  return GetTokenText(token.token_index_);
}

std::string_view Lexer::GetTokenText(TokenIndex index) const {
  const Line* line = GetLine(index.line);
  if (line == nullptr) {
    return {};
  }
  if (index.token < line->tokens.size()) {
    const TokenInfo token_info = line->tokens[index.token];
    return line->line.substr(token_info.column, token_info.size);
  }
  return {};
}

Token Lexer::ParseToken(TokenIndex index) {
  const Line* line = GetLine(index.line);
  if (line == nullptr || index.token > line->tokens.size()) {
    return Token::CreateError({}, &kErrorInvalidTokenContent);
  }
  const TokenInfo& token_info = line->tokens[index.token];
  std::string_view text = line->line.substr(token_info.column, token_info.size);
  switch (token_info.type) {
    case kTokenError:
      // The only kind of tokens stored as error are invalid tokens.
      return Token::CreateError(index, &kErrorInvalidToken);
    case kTokenSymbol:
      return Token::CreateSymbol(index, text);
    case kTokenInt: {
      DCHECK(!re_token_args_.empty());
      if (!RE2::ConsumeN(&text, re_token_, re_token_args_.data(),
                         re_token_args_.size())) {
        LOG(DFATAL) << "Integer failed to be re-parsed";
        return Token::CreateError(index, &kErrorInternal);
      }
      TokenArg* match = nullptr;
      for (TokenArg& arg : re_args_) {
        if (!arg.text.empty()) {
          match = &arg;
          break;
        }
      }
      if (match == nullptr) {
        LOG(DFATAL) << "Integer failed to be re-parsed";
        return Token::CreateError(index, &kErrorInternal);
      }
      return ParseInt(index, match->text, match->int_parse_type);
    } break;
    case kTokenFloat:
      return ParseFloat(index, text);
    case kTokenChar:
      return ParseChar(index, text);
    case kTokenString:
      return ParseString(index, text);
    case kTokenKeyword:
      return ParseKeyword(index, text);
    case kTokenIdentifier:
      return ParseIdent(index, text);
    case kTokenLineBreak:
      return Token::CreateLineBreak(index);
  }
  LOG(DFATAL) << "Unhandled token type when reparsing";
  return Token::CreateError(index, &kErrorInternal);
}

Token Lexer::ParseNextSymbol(Content* content, Line* line) {
  std::string_view symbol_text;
  if (!RE2::Consume(&line->remain, re_symbol_, &symbol_text)) {
    return {};
  }
  content->re_order = ReOrder::kSymLast;
  TokenIndex token_index = content->GetTokenIndex();
  ++content->token;
  line->tokens.emplace_back(symbol_text.data() - line->line.data(),
                            symbol_text.size(), kTokenSymbol);
  return Token::CreateSymbol(token_index, symbol_text);
}

Token Lexer::ParseInt(TokenIndex token_index, std::string_view text,
                      IntParseType int_parse_type) {
  int64_t value = 0;
  switch (int_parse_type) {
    case IntParseType::kDefault: {
      std::string_view digits = text.substr(
          decimal_config_.prefix, text.size() - decimal_config_.size_offset);
      if (!absl::SimpleAtoi(digits, &value)) {
        return Token::CreateError(token_index, &kErrorInvalidInteger);
      }
    } break;
    case IntParseType::kHex: {
      uint64_t hex_value = 0;
      std::string_view digits = text.substr(
          hex_config_.prefix, text.size() - hex_config_.size_offset);
      for (char ch : digits) {
        if (hex_value >> 60 != 0) {
          return Token::CreateError(token_index, &kErrorInvalidInteger);
        }
        hex_value <<= 4;
        if (ch >= '0' && ch <= '9') {
          hex_value |= ch - '0';
        } else if (ch >= 'A' && ch <= 'F') {
          hex_value |= ch - 'A' + 10;
        } else if (ch >= 'a' && ch <= 'f') {
          hex_value |= ch - 'a' + 10;
        }
      }
      // This is now defined behavior to 2's compliment starting in C++20.
      if (hex_value > static_cast<uint64_t>(max_int_)) {
        hex_value |= int_sign_extend_;
      }
      value = hex_value;
    } break;
    case IntParseType::kOctal: {
      uint64_t octal_value = 0;
      std::string_view digits = text.substr(
          octal_config_.prefix, text.size() - octal_config_.size_offset);
      for (char ch : digits) {
        if (octal_value >> 61 != 0) {
          return Token::CreateError(token_index, &kErrorInvalidInteger);
        }
        octal_value <<= 3;
        if (ch >= '0' && ch <= '7') {
          octal_value |= ch - '0';
        }
      }
      // This is now defined behavior to 2's compliment starting in C++20.
      if (octal_value > static_cast<uint64_t>(max_int_)) {
        octal_value |= int_sign_extend_;
      }
      value = octal_value;
    } break;
    case IntParseType::kBinary: {
      uint64_t binary_value = 0;
      std::string_view digits = text.substr(
          binary_config_.prefix, text.size() - binary_config_.size_offset);
      for (char ch : digits) {
        if (binary_value >> 63 != 0) {
          return Token::CreateError(token_index, &kErrorInvalidInteger);
        }
        binary_value <<= 1;
        if (ch == '1') {
          binary_value |= 1;
        }
      }
      // This is now defined behavior to 2's compliment starting in C++20.
      if (binary_value > static_cast<uint64_t>(max_int_)) {
        binary_value |= int_sign_extend_;
      }
      value = binary_value;
    } break;
  }
  if (value < min_int_ || value > max_int_) {
    return Token::CreateError(token_index, &kErrorInvalidInteger);
  }
  return Token::CreateInt(token_index, value);
}

Token Lexer::ParseFloat(TokenIndex token_index, std::string_view text) {
  std::string_view digits = text.substr(
      float_config_.prefix, text.size() - float_config_.size_offset);
  if (flags_.IsSet(LexerFlag::kFloat64)) {
    double value = 0;
    if (!absl::SimpleAtod(digits, &value) || !std::isnormal(value)) {
      return Token::CreateError(token_index, &kErrorInvalidFloat);
    }
    return Token::CreateFloat(token_index, value);
  } else {
    float value = 0;
    if (!absl::SimpleAtof(digits, &value) || !std::isnormal(value)) {
      return Token::CreateError(token_index, &kErrorInvalidFloat);
    }
    return Token::CreateFloat(token_index, value);
  }
}

namespace {
unsigned char ToHex(char ch) {
  if (absl::ascii_isdigit(ch)) {
    return ch - '0';
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  return 0;
}
}  // namespace

Token Lexer::ParseChar(TokenIndex token_index, std::string_view text) {
  DCHECK(text.size() >= 3);
  const char quote = text[0];
  const std::string_view char_text = text.substr(1, text.size() - 2);
  if (char_text.size() == 1 || !flags_.IsSet(LexerFlag::kDecodeEscape)) {
    return Token::CreateChar(token_index, char_text.data(), char_text.size());
  }
  DCHECK(char_text.size() >= 2 &&
         (char_text[0] == escape_ || char_text[0] == quote));
  if (escape_newline_ != 0 && char_text[1] == escape_newline_) {
    DCHECK(char_text.size() == 2);
    return Token::CreateChar(token_index, "\n", 1);
  }
  if (escape_tab_ != 0 && char_text[1] == escape_tab_) {
    DCHECK(char_text.size() == 2);
    return Token::CreateChar(token_index, "\t", 1);
  }
  if (escape_hex_ != 0 && char_text[1] == escape_hex_) {
    DCHECK(char_text.size() == 4);
    const char hex = ToHex(char_text[2]) << 4 | ToHex(char_text[3]);
    std::string& str =
        *modified_text_.emplace_back(std::make_unique<std::string>(1, hex));
    return Token::CreateChar(token_index, str.data(), str.size());
  }
  DCHECK(char_text.size() == 2);
  return Token::CreateChar(token_index, char_text.data() + 1, 1);
}

Token Lexer::ParseString(TokenIndex token_index, std::string_view text) {
  DCHECK(text.size() >= 2);
  const char quote = text[0];
  std::string_view char_text = text.substr(1, text.size() - 2);
  if (char_text.size() <= 1 || !flags_.IsSet(LexerFlag::kDecodeEscape)) {
    return Token::CreateString(token_index, char_text.data(), char_text.size());
  }
  const char escape_starts[3] = {quote, escape_, 0};
  auto pos = char_text.find_first_of(escape_starts);
  if (pos == std::string_view::npos) {
    return Token::CreateString(token_index, char_text.data(), char_text.size());
  }
  std::string& str =
      *modified_text_.emplace_back(std::make_unique<std::string>());
  str.reserve(char_text.size());
  while (pos != std::string_view::npos) {
    absl::StrAppend(&str, char_text.substr(0, pos));
    char_text.remove_prefix(pos);
    DCHECK(char_text.size() >= 2);
    if (escape_newline_ != 0 && char_text[1] == escape_newline_) {
      absl::StrAppend(&str, "\n");
      char_text.remove_prefix(2);
    } else if (escape_tab_ != 0 && char_text[1] == escape_tab_) {
      absl::StrAppend(&str, "\t");
      char_text.remove_prefix(2);
    } else if (escape_hex_ != 0 && char_text[1] == escape_hex_) {
      DCHECK(char_text.size() >= 4);
      const char hex = ToHex(char_text[2]) << 4 | ToHex(char_text[3]);
      absl::StrAppend(&str, std::string_view(&hex, 1));
      char_text.remove_prefix(4);
    } else {
      absl::StrAppend(&str, char_text.substr(1, 1));
      char_text.remove_prefix(2);
    }
    pos = char_text.find_first_of(escape_starts);
  }
  absl::StrAppend(&str, char_text);
  DCHECK(str.size() <= 0xFFFF);
  return Token::CreateString(token_index, str.data(), str.size());
}

Token Lexer::ParseKeyword(TokenIndex token_index, std::string_view text) {
  if (!flags_.IsSet(LexerFlag::kKeywordCaseInsensitive)) {
    return Token::CreateKeyword(token_index, text.data(), text.size());
  }
  auto it = keywords_.find(absl::AsciiStrToLower(text));
  if (it == keywords_.end()) {
    LOG(DFATAL) << "Keyword should not be found in case-insensitive map";
    return Token::CreateError(token_index, &kErrorInternal);
  }
  return Token::CreateKeyword(token_index, it->second.data(),
                              it->second.size());
}

Token Lexer::ParseIdent(TokenIndex token_index, std::string_view text) {
  text = text.substr(ident_config_.prefix,
                     text.size() - ident_config_.size_offset);
  if (flags_.IsSet(LexerFlag::kIdentForceLower)) {
    modified_text_.push_back(
        std::make_unique<std::string>(absl::AsciiStrToLower(text)));
    text = *modified_text_.back();
  } else if (flags_.IsSet(LexerFlag::kIdentForceUpper)) {
    modified_text_.push_back(
        std::make_unique<std::string>(absl::AsciiStrToUpper(text)));
    text = *modified_text_.back();
  }
  return Token::CreateIdentifier(token_index, text.data(), text.size());
}

Token Lexer::ParseNextToken(Content* content, Line* line) {
  std::string_view remain = line->remain;
  if (re_token_args_.empty() ||
      !RE2::ConsumeN(&remain, re_token_, re_token_args_.data(),
                     re_token_args_.size())) {
    return {};
  }
  TokenArg* match = nullptr;
  for (TokenArg& arg : re_args_) {
    if (!arg.text.empty()) {
      match = &arg;
      break;
    }
  }
  if (match == nullptr) {
    LOG(DFATAL) << "Token found without a token type match";
    return Token::CreateError(content->GetTokenIndex(), &kErrorInternal);
  }

  // See what the next text is. If it is not whitespace or a symol, then this
  // is not actually a match (it is either an error or a symbol, depending on
  // the ReOrder).
  std::string_view after_token = remain;
  if (!after_token.empty() && !RE2::Consume(&after_token, re_token_end_)) {
    return {};
  }

  line->remain = remain;
  TokenIndex token_index = content->GetTokenIndex();
  content->re_order = ReOrder::kSymFirst;
  ++content->token;
  std::string_view match_text = match->text;
  match->text = {};
  line->tokens.emplace_back(match_text.data() - line->line.data(),
                            match_text.size(), match->type);
  switch (match->type) {
    case kTokenInt:
      return ParseInt(token_index, match_text, match->int_parse_type);
    case kTokenFloat:
      return ParseFloat(token_index, match_text);
    case kTokenChar:
      return ParseChar(token_index, match_text);
    case kTokenString:
      return ParseString(token_index, match_text);
    case kTokenKeyword:
      return ParseKeyword(token_index, match_text);
    case kTokenIdentifier:
      return ParseIdent(token_index, match_text);
  }
  LOG(DFATAL) << "Unhandled token type while parsing";
  return Token::CreateError(token_index, &kErrorInternal);
}

Token Lexer::NextToken(LexerContentId id) {
  auto [content, line] = GetContentLine(id);
  if (content == nullptr) {
    return Token::CreateError({}, &kErrorInvalidTokenContent);
  }
  if (line == nullptr) {
    return Token::CreateEnd(content->GetTokenIndex());
  }

  Token token;

  // Returns true if there already is a parsed token.
  auto GetToken = [&]() {
    if (content->token < line->tokens.size()) {
      token = ParseToken(content->GetTokenIndex());
      ++content->token;
      return true;
    }
    return false;
  };

  // Advances to the next line and returns true if the end of the content was
  // reached.
  auto NextLineOrEnd = [&]() {
    ++content->line;
    content->token = 0;
    if (content->line == content->end_line) {
      token = token.CreateEnd(content->GetTokenIndex());
      return true;
    }
    ++line;
    return false;
  };

  // If we have a token, then we just return it.
  if (GetToken()) {
    return token;
  }

  // Skip whitespace and comments
  bool parsed_block_comment = false;
  while (true) {
    RE2::Consume(&line->remain, re_whitespace_);
    if (line->remain.empty()) {
      DCHECK(content->token >= line->tokens.size());
      if (flags_.IsSet(LexerFlag::kLineBreak) &&
          (line->tokens.empty() ||
           line->tokens.back().type != kTokenLineBreak)) {
        content->re_order = ReOrder::kSymLast;
        TokenIndex token_index = content->GetTokenIndex();
        ++content->token;
        line->tokens.emplace_back(line->line.size(), 0, kTokenLineBreak);
        return Token::CreateLineBreak(content->GetTokenIndex());
      }
      if (NextLineOrEnd() || GetToken()) {
        return token;
      }
      continue;
    }

    // Handle block comment which may span multiple lines.
    parsed_block_comment = false;
    for (const auto& [start, end] : block_comments_) {
      if (!line->remain.starts_with(start)) {
        continue;
      }
      parsed_block_comment = true;
      // At this point, we know the block comment extends to the next line,
      // or it would have been matched as part of regular whitespace.
      line->remain.remove_prefix(line->remain.size());
      if (NextLineOrEnd()) {
        return token;
      }
      while (true) {
        DCHECK(content->token == 0);

        //  This line must start over, even if there were previously tokens
        //  parsed on it, as we are in a comment block. We reset, just in case
        //  it was previously parsed via mixed-mode parsing (tokens and
        //  lines). This should be rare.
        line->tokens.clear();
        line->remain = line->line;

        auto end_pos = line->remain.find(end);
        if (end_pos == std::string_view::npos) {
          line->remain.remove_prefix(line->remain.size());
          if (NextLineOrEnd()) {
            return token;
          }
        } else {
          line->remain.remove_prefix(end_pos + end.size());
          break;
        }
      }
      break;
    }
    if (!parsed_block_comment) {
      break;
    }
  }

  // Parse the next token.
  if (content->re_order == ReOrder::kSymFirst) {
    token = ParseNextSymbol(content, line);
    if (token.GetType() == kTokenNone) {
      token = ParseNextToken(content, line);
    }
  } else {
    token = ParseNextToken(content, line);
    if (token.GetType() == kTokenNone) {
      token = ParseNextSymbol(content, line);
    }
  }

  // If we still don't have a token, then we have an error.
  if (token.GetType() == kTokenNone) {
    const TokenIndex token_index = content->GetTokenIndex();
    ++content->token;
    const char* token_start = line->remain.data();
    RE2::Consume(&line->remain, re_not_token_end_);
    line->tokens.emplace_back(token_start - line->line.data(),
                              line->remain.data() - token_start, kTokenError);
    return Token::CreateError(token_index, &kErrorInvalidToken);
  }

  return token;
}

bool Lexer::RewindToken(LexerContentId id) {
  Content* content = GetContent(id);
  if (content == nullptr) {
    return false;
  }
  Line* line = GetLine(content->GetLineIndex());
  --content->token;
  while (content->token < 0) {
    if (content->line == content->start_line) {
      content->token = 0;
      return false;
    }
    --content->line;
    Line* line = GetLine(content->GetLineIndex());
    if (line->tokens.empty()) {
      continue;
    }
    content->token = line->tokens.size() - 1;
    break;
  }
  return true;
}

}  // namespace gb
