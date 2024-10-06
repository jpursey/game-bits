// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer.h"

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
const std::string_view Lexer::kErrorUnexpectedCharacter =
    "Unexpected character";
const std::string_view Lexer::kErrorInvalidInteger = "Invalid integer";
const std::string_view Lexer::kErrorInvalidFloat = "Invalid float";

namespace {

bool CreateNoSymPattern(std::string& pattern_nosym,
                        const LexerConfig& lexer_config, std::string& error) {
  const LexerFlags flags = lexer_config.flags;

  if (!lexer_config.keywords.empty()) {
    error = Lexer::kErrorNotImplemented;
    return false;
  }

  if (LexerSupportsIntegers(flags)) {
    if (flags.Intersects({LexerFlag::kHexIntegers, LexerFlag::kOctalIntegers,
                          LexerFlag::kBinaryIntegers})) {
      error = Lexer::kErrorNotImplemented;
      return false;
    }
    absl::StrAppend(&pattern_nosym, "(");
    if (flags.IsSet(LexerFlag::kDecimalIntegers)) {
      if (flags.IsSet(LexerFlag::kNegativeIntegers)) {
        absl::StrAppend(&pattern_nosym, "-?");
      }
      absl::StrAppend(&pattern_nosym, "[0-9]+");
    }
    absl::StrAppend(&pattern_nosym, ")|");
  }

  if (LexerSupportsFloats(flags)) {
    if (flags.Intersects({LexerFlag::kExponentFloats})) {
      error = Lexer::kErrorNotImplemented;
      return false;
    }
    absl::StrAppend(&pattern_nosym, "(");
    if (flags.IsSet(LexerFlag::kDecimalFloats)) {
      if (flags.IsSet(LexerFlag::kNegativeFloats)) {
        absl::StrAppend(&pattern_nosym, "-?");
      }
      absl::StrAppend(&pattern_nosym, "[0-9]+\\.[0-9]+");
    }
    absl::StrAppend(&pattern_nosym, ")|");
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

    absl::StrAppend(&pattern_nosym, "([");
    if (flags.IsSet(LexerFlag::kIdentLeadingUnderscore)) {
      absl::StrAppend(&pattern_nosym, "_");
    }
    if (flags.IsSet(LexerFlag::kIdentUpper)) {
      absl::StrAppend(&pattern_nosym, "A-Z");
    }
    if (flags.IsSet(LexerFlag::kIdentLower)) {
      absl::StrAppend(&pattern_nosym, "a-z");
    }
    absl::StrAppend(&pattern_nosym, "][");
    if (flags.IsSet(LexerFlag::kIdentUnderscore)) {
      absl::StrAppend(&pattern_nosym, "_");
    }
    if (flags.IsSet(LexerFlag::kIdentUpper)) {
      absl::StrAppend(&pattern_nosym, "A-Z");
    }
    if (flags.IsSet(LexerFlag::kIdentLower)) {
      absl::StrAppend(&pattern_nosym, "a-z");
    }
    if (flags.IsSet(LexerFlag::kIdentDigit)) {
      absl::StrAppend(&pattern_nosym, "0-9");
    }
    absl::StrAppend(&pattern_nosym, "]+)|");
  }

  if (!pattern_nosym.empty()) {
    pattern_nosym.pop_back();
  }

  return true;
}

bool CreateSymPattern(std::string& pattern_sym, const LexerConfig& config,
                      std::string& error) {
  if (config.symbols.empty()) {
    return true;
  }
  absl::StrAppend(&pattern_sym, "(");
  std::vector<Symbol> symbols(config.symbols.begin(), config.symbols.end());
  std::sort(symbols.begin(), symbols.end(),
            [](Symbol a, Symbol b) { return b < a; });
  for (const Symbol& symbol : symbols) {
    if (!symbol.IsValid()) {
      error = Lexer::kErrorInvalidSymbolSpec;
      return false;
    }
    absl::StrAppend(&pattern_sym, RE2::QuoteMeta(symbol.GetString()));
    absl::StrAppend(&pattern_sym, "|");
  }
  pattern_sym.back() = ')';
  return true;
}

}  // namespace

std::unique_ptr<Lexer> Lexer::Create(const LexerConfig& lexer_config,
                                     std::string* error_message) {
  std::string temp_error;
  std::string& error = (error_message != nullptr ? *error_message : temp_error);

  Config config;
  config.flags = lexer_config.flags;

  std::string pattern_nosym;
  if (!CreateNoSymPattern(pattern_nosym, lexer_config, error)) {
    return nullptr;
  }

  int index = 0;
  if (LexerSupportsIntegers(lexer_config.flags)) {
    config.int_index = index++;
  }
  if (LexerSupportsFloats(lexer_config.flags)) {
    config.float_index = index++;
  }
  if (LexerSupportsIdentifiers(lexer_config.flags)) {
    config.ident_index = index++;
  }

  std::string pattern_sym;
  if (!CreateSymPattern(pattern_sym, lexer_config, error)) {
    return nullptr;
  }
  if (!pattern_sym.empty()) {
    config.symbols_index = index++;
  }
  config.token_pattern_count = index;

  std::string pattern_symfirst;
  std::string pattern_symlast;
  if (pattern_nosym.empty()) {
    pattern_symfirst = pattern_symlast = pattern_sym;
  } else if (pattern_sym.empty()) {
    pattern_symfirst = pattern_symlast = pattern_nosym;
  } else {
    pattern_symlast = absl::StrCat(pattern_nosym, "|", pattern_sym);
    pattern_symfirst = absl::StrCat(pattern_sym, "|", pattern_nosym);
  }
  if (pattern_symfirst.empty()) {
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

  config.pattern_whitespace = "[ \t]*";
  config.pattern_symfirst = pattern_symfirst;
  config.pattern_symlast = pattern_symlast;
  return absl::WrapUnique(new Lexer(config));
}

Lexer::Lexer(const Config& config)
    : flags_(config.flags),
      re_whitespace_(config.pattern_whitespace),
      re_token_{config.pattern_symlast, config.pattern_symfirst} {
  re_args_.resize(config.token_pattern_count);
  re_token_args_[kReSymFirst].resize(config.token_pattern_count);
  re_token_args_[kReSymLast].resize(config.token_pattern_count);
  int symfirst_offset = (config.symbols_index >= 0 ? 1 : 0);
  if (config.int_index >= 0) {
    const int i = config.int_index;
    re_args_[i].type = kTokenInt;
    re_token_args_[kReSymFirst][symfirst_offset + i] = &re_args_[i].arg;
    re_token_args_[kReSymLast][i] = &re_args_[i].arg;
  }
  if (config.float_index >= 0) {
    const int i = config.float_index;
    re_args_[config.float_index].type = kTokenFloat;
    re_token_args_[kReSymFirst][symfirst_offset + i] = &re_args_[i].arg;
    re_token_args_[kReSymLast][i] = &re_args_[i].arg;
  }
  if (config.ident_index >= 0) {
    const int i = config.ident_index;
    re_args_[config.ident_index].type = kTokenIdentifier;
    re_token_args_[kReSymFirst][symfirst_offset + i] = &re_args_[i].arg;
    re_token_args_[kReSymLast][i] = &re_args_[i].arg;
  }
  if (config.symbols_index >= 0) {
    const int i = config.symbols_index;
    re_args_[config.symbols_index].type = kTokenSymbol;
    re_token_args_[kReSymFirst][0] = &re_args_[i].arg;
    re_token_args_[kReSymLast][i] = &re_args_[i].arg;
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
  if (!RE2::ConsumeN(&line->remain, re_token_[content->token_re],
                     re_token_args_[content->token_re].data(),
                     re_token_args_->size())) {
    Token token = Token::CreateError(content->GetTokenIndex(),
                                     &kErrorUnexpectedCharacter);
    line->tokens.emplace_back(line->remain.data() - line->line.data(), 1,
                              kTokenError);
    line->remain.remove_prefix(1);
    ++content->token;
    return token;
  }
  TokenArg* match = nullptr;
  for (TokenArg& arg : re_args_) {
    if (!arg.text.empty()) {
      match = &arg;
      break;
    }
  }
  DCHECK(match != nullptr);
  TokenIndex token_index = content->GetTokenIndex();
  ++content->token;
  std::string_view match_text = match->text;
  match->text = {};
  line->tokens.emplace_back(match_text.data() - line->line.data(),
                            match_text.size(), match->type);
  switch (match->type) {
    case kTokenInt: {
      content->token_re = kReSymFirst;
      int64_t value = 0;
      if (!absl::SimpleAtoi(match_text, &value)) {
        return Token::CreateError(token_index, &kErrorInvalidInteger);
      }
      return Token::CreateInt(token_index, value, sizeof(value));
    } break;
    case kTokenFloat: {
      content->token_re = kReSymFirst;
      double value = 0;
      if (!absl::SimpleAtod(match_text, &value)) {
        return Token::CreateError(token_index, &kErrorInvalidFloat);
      }
      return Token::CreateFloat(token_index, value, sizeof(value));
    } break;
    case kTokenIdentifier:
      content->token_re = kReSymFirst;
      return Token::CreateIdentifier(token_index, match_text.data(),
                                     match_text.size());
    case kTokenSymbol:
      content->token_re = kReSymLast;
      return Token::CreateSymbol(token_index, match_text);
  }
  return Token::CreateError(token_index, &kErrorNotImplemented);
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
