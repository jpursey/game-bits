// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer.h"

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/str_split.h"

namespace gb {

const std::string_view Lexer::kErrorNotImplemented = "Not implemented";
const std::string_view Lexer::kErrorConflictingStringAndCharSpec =
    "Conflicting string and character specifications";
const std::string_view Lexer::kErrorInvalidSymbolSpec =
    "Symbol specification has non-ASCII or whitespace characters";
const std::string_view Lexer::kErrorNoTokenSpec =
    "No token specification (from symbols, keywords, or flags)";
const std::string_view Lexer::kErrorInvalidTokenContent =
    "Token does not refer to valid content";
const std::string_view Lexer::kErrorInvalidToken = "Invalid token";
const std::string_view Lexer::kErrorInvalidInteger = "Invalid integer";
const std::string_view Lexer::kErrorInvalidFloat = "Invalid float";

namespace {

bool CreateTokenPattern(std::string& token_pattern,
                        const LexerConfig& lexer_config, std::string& error) {
  const LexerFlags flags = lexer_config.flags;

  if (!lexer_config.keywords.empty()) {
    error = Lexer::kErrorNotImplemented;
    return false;
  }

  if (LexerSupportsIntegers(flags)) {
    bool first = true;
    if (flags.IsSet(LexerFlag::kBinaryIntegers)) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      first = false;
      absl::StrAppend(&token_pattern, "(");
      absl::StrAppend(&token_pattern,
                      RE2::QuoteMeta(lexer_config.binary_prefix));
      absl::StrAppend(&token_pattern, "[01]+");
      absl::StrAppend(&token_pattern,
                      RE2::QuoteMeta(lexer_config.binary_suffix));
      absl::StrAppend(&token_pattern, ")");
    }
    if (flags.IsSet(LexerFlag::kOctalIntegers)) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      first = false;
      absl::StrAppend(&token_pattern, "(");
      absl::StrAppend(&token_pattern,
                      RE2::QuoteMeta(lexer_config.octal_prefix));
      absl::StrAppend(&token_pattern, "[0-7]+");
      absl::StrAppend(&token_pattern,
                      RE2::QuoteMeta(lexer_config.octal_suffix));
      absl::StrAppend(&token_pattern, ")");
    }
    if (flags.IsSet(LexerFlag::kDecimalIntegers)) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      first = false;
      absl::StrAppend(&token_pattern, "(");
      absl::StrAppend(&token_pattern,
                      RE2::QuoteMeta(lexer_config.decimal_prefix));
      if (flags.IsSet(LexerFlag::kNegativeIntegers)) {
        absl::StrAppend(&token_pattern, "-?");
      }
      absl::StrAppend(&token_pattern, "[0-9]+");
      absl::StrAppend(&token_pattern,
                      RE2::QuoteMeta(lexer_config.decimal_suffix));
      absl::StrAppend(&token_pattern, ")");
    }
    if (flags.Intersects(
            {LexerFlag::kHexUpperIntegers, LexerFlag::kHexLowerIntegers})) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      absl::StrAppend(&token_pattern, "(");
      absl::StrAppend(&token_pattern, RE2::QuoteMeta(lexer_config.hex_prefix));
      absl::StrAppend(&token_pattern, "[0-9");
      if (flags.IsSet(LexerFlag::kHexUpperIntegers)) {
        absl::StrAppend(&token_pattern, "A-F");
      }
      if (flags.IsSet(LexerFlag::kHexLowerIntegers)) {
        absl::StrAppend(&token_pattern, "a-f");
      }
      absl::StrAppend(&token_pattern, "]+");
      absl::StrAppend(&token_pattern, RE2::QuoteMeta(lexer_config.hex_suffix));
      absl::StrAppend(&token_pattern, ")");
    }
    absl::StrAppend(&token_pattern, "|");
  }

  if (LexerSupportsFloats(flags)) {
    absl::StrAppend(&token_pattern, "(");
    absl::StrAppend(&token_pattern, RE2::QuoteMeta(lexer_config.float_prefix));
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
    absl::StrAppend(&token_pattern, RE2::QuoteMeta(lexer_config.float_suffix));
    absl::StrAppend(&token_pattern, ")|");
  }

  if (flags.IsSet(
          {LexerFlag::kDoubleQuoteCharacter, LexerFlag::kDoubleQuoteString}) ||
      flags.IsSet(
          {LexerFlag::kSingleQuoteCharacter, LexerFlag::kSingleQuoteString})) {
    error = Lexer::kErrorConflictingStringAndCharSpec;
    return false;
  }

  if (LexerSupportsStrings(flags)) {
    if (flags.Intersects({LexerFlag::kQuoteQuoteEscape,
                          LexerFlag::kEscapeCharacter, LexerFlag::kTabInQuotes,
                          LexerFlag::kDecodeEscape})) {
      error = Lexer::kErrorNotImplemented;
      return false;
    }
    error = Lexer::kErrorNotImplemented;
    return false;
  }

  if (LexerSupportsCharacters(flags)) {
    if (flags.Intersects({LexerFlag::kQuoteQuoteEscape,
                          LexerFlag::kEscapeCharacter, LexerFlag::kTabInQuotes,
                          LexerFlag::kDecodeEscape})) {
      error = Lexer::kErrorNotImplemented;
      return false;
    }
    error = Lexer::kErrorNotImplemented;
    return false;
  }

  if (LexerSupportsIdentifiers(flags)) {
    if (flags.Intersects(
            {LexerFlag::kIdentForceUpper, LexerFlag::kIdentForceLower})) {
      error = Lexer::kErrorNotImplemented;
      return false;
    }

    absl::StrAppend(&token_pattern, "([");
    if (flags.IsSet(LexerFlag::kIdentLeadingUnderscore)) {
      absl::StrAppend(&token_pattern, "_");
    }
    if (flags.IsSet(LexerFlag::kIdentUpper)) {
      absl::StrAppend(&token_pattern, "A-Z");
    }
    if (flags.IsSet(LexerFlag::kIdentLower)) {
      absl::StrAppend(&token_pattern, "a-z");
    }
    absl::StrAppend(&token_pattern, "][");
    if (flags.IsSet(LexerFlag::kIdentUnderscore)) {
      absl::StrAppend(&token_pattern, "_");
    }
    if (flags.IsSet(LexerFlag::kIdentUpper)) {
      absl::StrAppend(&token_pattern, "A-Z");
    }
    if (flags.IsSet(LexerFlag::kIdentLower)) {
      absl::StrAppend(&token_pattern, "a-z");
    }
    if (flags.IsSet(LexerFlag::kIdentDigit)) {
      absl::StrAppend(&token_pattern, "0-9");
    }
    absl::StrAppend(&token_pattern, "]+)|");
  }

  if (!token_pattern.empty()) {
    token_pattern.pop_back();
  }

  return true;
}

bool CreateSymbolPattern(std::string& pattern_sym, std::string& symbol_chars,
                         const LexerConfig& config, std::string& error) {
  if (config.symbols.empty()) {
    return true;
  }
  std::vector<Symbol> symbols(config.symbols.begin(), config.symbols.end());
  for (const Symbol& symbol : symbols) {
    if (!symbol.IsValid()) {
      error = Lexer::kErrorInvalidSymbolSpec;
      return false;
    }
    const char first_char = symbol.GetString()[0];
    if (symbol_chars.find(first_char) == std::string::npos) {
      symbol_chars.push_back(first_char);
    }
    absl::StrAppend(&pattern_sym, RE2::QuoteMeta(symbol.GetString()));
    absl::StrAppend(&pattern_sym, "|");
  }
  pattern_sym.pop_back();
  if (!symbol_chars.empty()) {
    symbol_chars = RE2::QuoteMeta(symbol_chars);
  }
  return true;
}

}  // namespace

std::unique_ptr<Lexer> Lexer::Create(const LexerConfig& lexer_config,
                                     std::string* error_message) {
  std::string temp_error;
  std::string& error = (error_message != nullptr ? *error_message : temp_error);

  Config config;
  config.flags = lexer_config.flags;

  std::string token_pattern;
  if (!CreateTokenPattern(token_pattern, lexer_config, error)) {
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
  if (LexerSupportsIdentifiers(lexer_config.flags)) {
    config.ident_index = index++;
  }
  config.token_pattern_count = index;

  std::string symbol_pattern;
  std::string symbol_chars;
  if (!CreateSymbolPattern(symbol_pattern, symbol_chars, lexer_config, error)) {
    return nullptr;
  }

  if (token_pattern.empty() && symbol_pattern.empty()) {
    error = Lexer::kErrorNoTokenSpec;
    return nullptr;
  }

  if (lexer_config.flags.Intersects(
          {LexerFlag::kLineBreak, LexerFlag::kLineIndent,
           LexerFlag::kLeadingTabs, LexerFlag::kEscapeNewline,
           LexerFlag::kLineComments, LexerFlag::kBlockComments})) {
    error = Lexer::kErrorNotImplemented;
    return nullptr;
  }

  config.whitespace_pattern = "[ \\t]*";
  if (!symbol_pattern.empty()) {
    config.symbol_pattern = absl::StrCat("(", symbol_pattern, ")");
    config.token_end_pattern = absl::StrCat("[ \\t]|", symbol_pattern);
    config.not_token_end_pattern = absl::StrCat("[^ \\t", symbol_chars, "]*");
  } else {
    config.token_end_pattern = "[ \\t]";
    config.not_token_end_pattern = "[^ \\t]*";
  }
  config.token_pattern = token_pattern;
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
      ident_config_(config.ident_config) {
  re_args_.resize(config.token_pattern_count);
  re_token_args_.resize(config.token_pattern_count);
  if (config.binary_index >= 0) {
    const int i = config.binary_index;
    re_args_[i].type = kTokenInt;
    re_args_[i].parse_type = ParseType::kBinary;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.octal_index >= 0) {
    const int i = config.octal_index;
    re_args_[i].type = kTokenInt;
    re_args_[i].parse_type = ParseType::kOctal;
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
    re_args_[i].parse_type = ParseType::kHex;
    re_token_args_[i] = &re_args_[i].arg;
  }
  if (config.float_index >= 0) {
    const int i = config.float_index;
    re_args_[config.float_index].type = kTokenFloat;
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

std::string_view Lexer::GetIndexedString(int index) const {
  if (index < 0 || index >= indexed_strings_.size()) {
    return {};
  }
  return indexed_strings_[index];
}

Token Lexer::ParseToken(TokenIndex index) {
  const Line* line = GetLine(index.line);
  if (line == nullptr) {
    return Token::CreateError({}, &kErrorInvalidTokenContent);
  }
  return Token::CreateError(index, &kErrorNotImplemented);
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

bool Lexer::ParseInt(std::string_view text, ParseType parse_type,
                     int64_t& value) {
  switch (parse_type) {
    case ParseType::kDefault: {
      std::string_view digits = text.substr(
          decimal_config_.prefix, text.size() - decimal_config_.size_offset);
      if (!absl::SimpleAtoi(digits, &value)) {
        return false;
      }
      return value >= min_int_ && value <= max_int_;
    } break;
    case ParseType::kHex: {
      uint64_t hex_value = 0;
      std::string_view digits = text.substr(
          hex_config_.prefix, text.size() - hex_config_.size_offset);
      for (char ch : digits) {
        if (hex_value >> 60 != 0) {
          return false;
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
      return value >= min_int_ && value <= max_int_;
    } break;
    case ParseType::kOctal: {
      uint64_t octal_value = 0;
      std::string_view digits = text.substr(
          octal_config_.prefix, text.size() - octal_config_.size_offset);
      for (char ch : digits) {
        if (octal_value >> 61 != 0) {
          return false;
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
      return value >= min_int_ && value <= max_int_;
    } break;
    case ParseType::kBinary: {
      uint64_t binary_value = 0;
      std::string_view digits = text.substr(
          binary_config_.prefix, text.size() - binary_config_.size_offset);
      for (char ch : digits) {
        if (binary_value >> 63 != 0) {
          return false;
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
      return value >= min_int_ && value <= max_int_;
    } break;
  }
  LOG(FATAL) << "Unhandled integer parse type";
  return false;
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
    // This should never happen in practice, as matches must be non-empty.
    return Token::CreateError(content->GetTokenIndex(), &kErrorNotImplemented);
  }

  // See what the next text is. If it is not whitespace or a symol, then this
  // is not actually a match (it is either an error or a symbol, depending on
  // the ReOrder).
  if (!remain.empty() && !RE2::Consume(&remain, re_token_end_)) {
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
    case kTokenInt: {
      int64_t value = 0;
      if (!ParseInt(match_text, match->parse_type, value)) {
        return Token::CreateError(token_index, &kErrorInvalidInteger);
      }
      return Token::CreateInt(token_index, value, sizeof(value));
    } break;
    case kTokenFloat: {
      std::string_view digits = match_text.substr(
          float_config_.prefix, match_text.size() - float_config_.size_offset);
      if (flags_.IsSet(LexerFlag::kFloat64)) {
        double value = 0;
        if (!absl::SimpleAtod(digits, &value) || !std::isnormal(value)) {
          return Token::CreateError(token_index, &kErrorInvalidFloat);
        }
        return Token::CreateFloat(token_index, value, sizeof(value));
      } else {
        float value = 0;
        if (!absl::SimpleAtof(digits, &value) || !std::isnormal(value)) {
          return Token::CreateError(token_index, &kErrorInvalidFloat);
        }
        return Token::CreateFloat(token_index, value, sizeof(value));
      }
    } break;
    case kTokenIdentifier:
      return Token::CreateIdentifier(token_index, match_text.data(),
                                     match_text.size());
  }
  return Token::CreateError(token_index, &kErrorNotImplemented);
}

Token Lexer::NextToken(LexerContentId id) {
  auto [content, line] = GetContentLine(id);
  if (content == nullptr) {
    return Token::CreateError({}, &kErrorInvalidTokenContent);
  }
  if (line == nullptr) {
    return Token::CreateEnd(content->GetTokenIndex());
  }
  if (content->token < line->tokens.size()) {
    Token token = ParseToken(content->GetTokenIndex());
    ++content->token;
    return token;
  }
  while (true) {
    RE2::Consume(&line->remain, re_whitespace_);
    if (!line->remain.empty()) {
      break;
    }
    ++content->line;
    content->token = 0;
    if (content->line == content->end_line) {
      return Token::CreateEnd(content->GetTokenIndex());
    }
    ++line;
    if (content->token < line->tokens.size()) {
      Token token = ParseToken(content->GetTokenIndex());
      ++content->token;
      return token;
    }
  }
  Token token;
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
    break;
  }
  content->token = line->tokens.size() - 1;
  const TokenInfo token_info = line->tokens[content->token];
  return true;
}

}  // namespace gb
