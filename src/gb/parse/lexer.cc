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

const std::string_view Lexer::kErrorInternal = "Internal error";
const std::string_view Lexer::kErrorInvalidTokenContent =
    "Token does not refer to valid content";
const std::string_view Lexer::kErrorInvalidToken = "Invalid token";
const std::string_view Lexer::kErrorInvalidInteger = "Invalid integer";
const std::string_view Lexer::kErrorInvalidFloat = "Invalid float";

std::unique_ptr<Lexer> Lexer::Create(const LexerConfig& lexer_config,
                                     std::string* error_message) {
  return Create(LexerProgram::Create(lexer_config, error_message));
}

std::unique_ptr<Lexer> Lexer::Create(
    std::shared_ptr<const LexerProgram> program) {
  if (program == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(new Lexer(std::move(program)));
}

Lexer::Lexer(std::shared_ptr<const LexerProgram> program)
    : program_(program), state_(program->state_) {
  re_args_.resize(state_.re_args.size());
  re_token_args_.resize(state_.re_args.size());
  for (int i = 0; i < re_args_.size(); ++i) {
    re_args_[i].type = state_.re_args[i].type;
    re_args_[i].int_parse_type = state_.re_args[i].int_parse_type;
    re_token_args_[i] = &re_args_[i].arg;
  }
}

bool Lexer::IsValidTokenType(TokenType token_type) const {
  if (token_type == kTokenEnd) {
    return true;
  }
  if (token_type == kTokenLineBreak &&
      state_.flags.IsSet(LexerFlag::kLineBreak)) {
    return true;
  }
  if (token_type == kTokenSymbol) {
    return state_.re_symbol->NumberOfCapturingGroups() != 0;
  }
  for (auto& token_arg : re_args_) {
    if (token_arg.type == token_type) {
      return true;
    }
  }
  return false;
}

inline TokenIndex Lexer::Content::GetTokenIndex() const {
  // We can't return the normal token index here, as it is would refer to the
  // first token in the next content, which we need to distinguish from. We do
  // this by returning a token index that is one past the end of the last token
  // possible in this content.
  if (line >= end_line - start_line) {
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
  if (content->line >= content->end_line - content->start_line) {
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

  // Sanitize line endings in the content.
  absl::StrReplaceAll({{"\r\n", "\n"}, {"\r", "\n"}}, &content->text);

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
  return {
      .id = id, .filename = content->filename, .line = line_index, .column = 0};
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
  const Content* content = GetContent(line->id);
  DCHECK(content != nullptr);
  return {.id = line->id,
          .filename = content->filename,
          .line = static_cast<int>(index.line) - content->start_line,
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

Token Lexer::ParseTokenText(std::string_view token_text) const {
  if (RE2::FullMatch(token_text, *state_.re_symbol)) {
    return Token::CreateSymbol(kInvalidTokenIndex, token_text);
  }
  if (re_token_args_.empty() ||
      !RE2::FullMatchN(token_text, *state_.re_token, re_token_args_.data(),
                       re_token_args_.size())) {
    return Token::CreateError(kInvalidTokenIndex, &kErrorInvalidToken);
  }
  const TokenArg* match = nullptr;
  for (const TokenArg& arg : re_args_) {
    if (!arg.text.empty()) {
      match = &arg;
    }
  }
  if (match == nullptr) {
    LOG(DFATAL) << "Token found without a token type match";
    return Token::CreateError(kInvalidTokenIndex, &kErrorInternal);
  }
  Token token;
  switch (match->type) {
    case kTokenInt:
      return ParseInt(kInvalidTokenIndex, match->text, match->int_parse_type);
    case kTokenFloat:
      return ParseFloat(kInvalidTokenIndex, match->text);
    case kTokenChar:
      return ParseChar(kInvalidTokenIndex, match->text);
    case kTokenString:
      return ParseString(kInvalidTokenIndex, match->text);
    case kTokenKeyword:
      return ParseKeyword(kInvalidTokenIndex, match->text);
    case kTokenIdentifier:
      return ParseIdent(kInvalidTokenIndex, match->text);
    default:
      if (match->type >= kTokenUser) {
        return ParseUserToken(kInvalidTokenIndex, match->type, match->text);
      }
      break;
  }
  LOG(DFATAL) << "Unhandled token type when parsing token text";
  return Token::CreateError(kInvalidTokenIndex, &kErrorInternal);
}

bool Lexer::SetNextToken(Token token) {
  const TokenIndex index = token.GetTokenIndex();
  const Line* line = GetLine(index.line);
  if (line == nullptr || (index.token >= line->tokens.size() &&
                          index.token != kTokenIndexEndToken)) {
    return false;
  }
  Content* content = GetContent(line->id);
  DCHECK(content != nullptr);
  if (index.token == kTokenIndexEndToken) {
    DCHECK(index.line == content->end_line - 1);
    content->line = content->end_line - content->start_line;
    content->token = 0;
    last_token_ = token;
    return true;
  }
  content->line = index.line - content->start_line;
  content->token = index.token;
  last_token_ = token;
  return true;
}

Token Lexer::ParseToken(TokenIndex index) {
  const Line* line = GetLine(index.line);
  if (line == nullptr || index.token > line->tokens.size()) {
    if (index.token == kTokenIndexEndToken) {
      return Token::CreateEnd(index);
    }
    return Token::CreateError(kInvalidTokenIndex, &kErrorInvalidTokenContent);
  }
  if (index == last_token_.GetTokenIndex()) {
    return last_token_;
  }
  const TokenInfo& token_info = line->tokens[index.token];
  std::string_view text = line->line.substr(token_info.column, token_info.size);
  auto ReturnToken = [this](const Token& token) {
    last_token_ = token;
    return token;
  };
  switch (token_info.type) {
    case kTokenError:
      // The only kind of tokens stored as error are invalid tokens.
      return ReturnToken(Token::CreateError(index, &kErrorInvalidToken));
    case kTokenSymbol:
      return ReturnToken(Token::CreateSymbol(index, text));
    case kTokenInt: {
      DCHECK(!re_token_args_.empty());
      if (!RE2::ConsumeN(&text, *state_.re_token, re_token_args_.data(),
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
        return ReturnToken(Token::CreateError(index, &kErrorInternal));
      }
      return ReturnToken(ParseInt(index, match->text, match->int_parse_type));
    } break;
    case kTokenFloat:
      return ReturnToken(ParseFloat(index, text));
    case kTokenChar:
      return ReturnToken(ParseChar(index, text));
    case kTokenString:
      return ReturnToken(ParseString(index, text));
    case kTokenKeyword:
      return ReturnToken(ParseKeyword(index, text));
    case kTokenIdentifier:
      return ReturnToken(ParseIdent(index, text));
    case kTokenLineBreak:
      return ReturnToken(Token::CreateLineBreak(index));
    default:
      if (token_info.type >= kTokenUser) {
        return ReturnToken(ParseUserToken(index, token_info.type, text));
      }
      break;
  }
  LOG(DFATAL) << "Unhandled token type when reparsing";
  return Token::CreateError(index, &kErrorInternal);
}

Token Lexer::ParseNextSymbol(Content* content, Line* line, bool advance) {
  std::string_view symbol_text;
  if (!RE2::Consume(&line->remain, *state_.re_symbol, &symbol_text)) {
    return {};
  }
  content->re_order = ReOrder::kSymLast;
  TokenIndex token_index = content->GetTokenIndex();
  if (advance) {
    ++content->token;
  }
  line->tokens.emplace_back(symbol_text.data() - line->line.data(),
                            symbol_text.size(), kTokenSymbol);
  last_token_ = Token::CreateSymbol(token_index, symbol_text);
  return last_token_;
}

Token Lexer::ParseInt(TokenIndex token_index, std::string_view text,
                      IntParseType int_parse_type) const {
  int64_t value = 0;
  switch (int_parse_type) {
    case IntParseType::kDefault: {
      std::string_view digits =
          text.substr(state_.decimal_config.prefix,
                      text.size() - state_.decimal_config.size_offset);
      if (!absl::SimpleAtoi(digits, &value)) {
        return Token::CreateError(token_index, &kErrorInvalidInteger);
      }
    } break;
    case IntParseType::kHex: {
      uint64_t hex_value = 0;
      std::string_view digits =
          text.substr(state_.hex_config.prefix,
                      text.size() - state_.hex_config.size_offset);
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
      if (hex_value > static_cast<uint64_t>(state_.max_int)) {
        hex_value |= state_.int_sign_extend;
      }
      value = hex_value;
    } break;
    case IntParseType::kOctal: {
      uint64_t octal_value = 0;
      std::string_view digits =
          text.substr(state_.octal_config.prefix,
                      text.size() - state_.octal_config.size_offset);
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
      if (octal_value > static_cast<uint64_t>(state_.max_int)) {
        octal_value |= state_.int_sign_extend;
      }
      value = octal_value;
    } break;
    case IntParseType::kBinary: {
      uint64_t binary_value = 0;
      std::string_view digits =
          text.substr(state_.binary_config.prefix,
                      text.size() - state_.binary_config.size_offset);
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
      if (binary_value > static_cast<uint64_t>(state_.max_int)) {
        binary_value |= state_.int_sign_extend;
      }
      value = binary_value;
    } break;
  }
  if (value < state_.min_int || value > state_.max_int) {
    return Token::CreateError(token_index, &kErrorInvalidInteger);
  }
  return Token::CreateInt(token_index, value);
}

Token Lexer::ParseFloat(TokenIndex token_index, std::string_view text) const {
  std::string_view digits =
      text.substr(state_.float_config.prefix,
                  text.size() - state_.float_config.size_offset);
  if (state_.flags.IsSet(LexerFlag::kFloat64)) {
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

Token Lexer::ParseChar(TokenIndex token_index, std::string_view text) const {
  DCHECK(text.size() >= 3);
  const char quote = text[0];
  const std::string_view char_text = text.substr(1, text.size() - 2);
  if (char_text.size() == 1 || !state_.flags.IsSet(LexerFlag::kDecodeEscape)) {
    return Token::CreateChar(token_index, char_text.data(), char_text.size());
  }
  DCHECK(char_text.size() >= 2 &&
         (char_text[0] == state_.escape || char_text[0] == quote));
  if (state_.escape_newline != 0 && char_text[1] == state_.escape_newline) {
    DCHECK(char_text.size() == 2);
    return Token::CreateChar(token_index, "\n", 1);
  }
  if (state_.escape_tab != 0 && char_text[1] == state_.escape_tab) {
    DCHECK(char_text.size() == 2);
    return Token::CreateChar(token_index, "\t", 1);
  }
  if (state_.escape_hex != 0 && char_text[1] == state_.escape_hex) {
    DCHECK(char_text.size() == 4);
    const char hex = ToHex(char_text[2]) << 4 | ToHex(char_text[3]);
    std::string& str =
        *modified_text_.emplace_back(std::make_unique<std::string>(1, hex));
    return Token::CreateChar(token_index, str.data(), str.size());
  }
  DCHECK(char_text.size() == 2);
  return Token::CreateChar(token_index, char_text.data() + 1, 1);
}

Token Lexer::ParseString(TokenIndex token_index, std::string_view text) const {
  DCHECK(text.size() >= 2);
  const char quote = text[0];
  std::string_view char_text = text.substr(1, text.size() - 2);
  if (char_text.size() <= 1 || !state_.flags.IsSet(LexerFlag::kDecodeEscape)) {
    return Token::CreateString(token_index, char_text.data(), char_text.size());
  }
  const char escape_starts[3] = {quote, state_.escape, 0};
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
    if (state_.escape_newline != 0 && char_text[1] == state_.escape_newline) {
      absl::StrAppend(&str, "\n");
      char_text.remove_prefix(2);
    } else if (state_.escape_tab != 0 && char_text[1] == state_.escape_tab) {
      absl::StrAppend(&str, "\t");
      char_text.remove_prefix(2);
    } else if (state_.escape_hex != 0 && char_text[1] == state_.escape_hex) {
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

Token Lexer::ParseKeyword(TokenIndex token_index, std::string_view text) const {
  if (!state_.flags.IsSet(LexerFlag::kKeywordCaseInsensitive)) {
    return Token::CreateKeyword(token_index, text.data(), text.size());
  }
  auto it = state_.keywords.find(absl::AsciiStrToLower(text));
  if (it == state_.keywords.end()) {
    LOG(DFATAL) << "Keyword should not be found in case-insensitive map";
    return Token::CreateError(token_index, &kErrorInternal);
  }
  return Token::CreateKeyword(token_index, it->second.data(),
                              it->second.size());
}

Token Lexer::ParseIdent(TokenIndex token_index, std::string_view text) const {
  text = text.substr(state_.ident_config.prefix,
                     text.size() - state_.ident_config.size_offset);
  if (state_.flags.IsSet(LexerFlag::kIdentForceLower)) {
    modified_text_.push_back(
        std::make_unique<std::string>(absl::AsciiStrToLower(text)));
    text = *modified_text_.back();
  } else if (state_.flags.IsSet(LexerFlag::kIdentForceUpper)) {
    modified_text_.push_back(
        std::make_unique<std::string>(absl::AsciiStrToUpper(text)));
    text = *modified_text_.back();
  }
  return Token::CreateIdentifier(token_index, text.data(), text.size());
}

Token Lexer::ParseUserToken(TokenIndex token_index, TokenType token_type,
                            std::string_view text) const {
  return Token::CreateUser(token_index, token_type, text.data(), text.size());
}

Token Lexer::ParseNextToken(Content* content, Line* line, bool advance) {
  std::string_view remain = line->remain;
  std::string_view token_start = remain;
  if (re_token_args_.empty() ||
      !RE2::ConsumeN(&remain, *state_.re_token, re_token_args_.data(),
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
  if (!after_token.empty() &&
      !RE2::Consume(&after_token, *state_.re_token_end)) {
    return {};
  }

  line->remain = remain;
  TokenIndex token_index = content->GetTokenIndex();
  content->re_order = ReOrder::kSymFirst;
  if (advance) {
    ++content->token;
  }
  std::string_view match_text = match->text;
  match->text = {};
  line->tokens.emplace_back(token_start.data() - line->line.data(),
                            remain.data() - token_start.data(), match->type);
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
    default:
      if (match->type >= kTokenUser) {
        return ParseUserToken(token_index, match->type, match_text);
      }
      break;
  }
  LOG(DFATAL) << "Unhandled token type while parsing";
  return Token::CreateError(token_index, &kErrorInternal);
}

Token Lexer::NextToken(LexerContentId id, bool advance) {
  auto [content, line] = GetContentLine(id);
  if (content == nullptr) {
    return Token::CreateError(kInvalidTokenIndex, &kErrorInvalidTokenContent);
  }
  if (line == nullptr) {
    return Token::CreateEnd(content->GetTokenIndex());
  }

  Token token;

  // Returns true if there already is a parsed token.
  auto GetToken = [&]() {
    if (content->token < line->tokens.size()) {
      token = ParseToken(content->GetTokenIndex());
      if (advance) {
        content->re_order = (token.GetType() == kTokenSymbol ||
                                     token.GetType() == kTokenLineBreak
                                 ? ReOrder::kSymLast
                                 : ReOrder::kSymFirst);
        ++content->token;
      }
      return true;
    }
    return false;
  };

  // Advances to the next line and returns true if the end of the content was
  // reached.
  auto NextLineOrEnd = [&]() {
    ++content->line;
    content->token = 0;
    if (content->line == content->end_line - content->start_line) {
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
    RE2::Consume(&line->remain, *state_.re_whitespace);
    if (line->remain.empty()) {
      DCHECK(content->token >= line->tokens.size());
      if (state_.flags.IsSet(LexerFlag::kLineBreak) &&
          (line->tokens.empty() ||
           line->tokens.back().type != kTokenLineBreak)) {
        content->re_order = ReOrder::kSymLast;
        TokenIndex token_index = content->GetTokenIndex();
        if (advance) {
          ++content->token;
        }
        line->tokens.emplace_back(line->line.size(), 0, kTokenLineBreak);
        last_token_ = Token::CreateLineBreak(token_index);
        return last_token_;
      }
      if (NextLineOrEnd() || GetToken()) {
        return token;
      }
      continue;
    }

    // Handle block comment which may span multiple lines.
    parsed_block_comment = false;
    for (const auto& [start, end] : state_.block_comments) {
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
    token = ParseNextSymbol(content, line, advance);
    if (token.GetType() == kTokenNone) {
      token = ParseNextToken(content, line, advance);
    }
  } else {
    token = ParseNextToken(content, line, advance);
    if (token.GetType() == kTokenNone) {
      token = ParseNextSymbol(content, line, advance);
    }
  }

  // If we still don't have a token, then we have an error.
  if (token.GetType() == kTokenNone) {
    const TokenIndex token_index = content->GetTokenIndex();
    if (advance) {
      ++content->token;
    }
    const char* token_start = line->remain.data();
    RE2::Consume(&line->remain, *state_.re_not_token_end);
    line->tokens.emplace_back(token_start - line->line.data(),
                              line->remain.data() - token_start, kTokenError);
    token = Token::CreateError(token_index, &kErrorInvalidToken);
  }

  last_token_ = token;
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
