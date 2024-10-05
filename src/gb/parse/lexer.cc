// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_split.h"

namespace gb {

const std::string_view Lexer::kErrorNotImplemented = "Not implemented";
const std::string_view Lexer::kErrorInvalidTokenContent =
    "Token does not refer to valid content";

std::unique_ptr<Lexer> Lexer::Create(const LexerConfig& config,
                                     std::string* error) {
  return absl::WrapUnique(new Lexer);
}

Lexer::Lexer() = default;

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
  content->column = 0;
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
  std::string_view result = line->line.substr(content->column);
  ++content->line;
  content->column = 0;
  content->token = 0;
  return result;
}

bool Lexer::RewindLine(LexerContentId id) {
  Content* content = GetContent(id);
  if (content == nullptr) {
    return false;
  }
  if (content->line <= content->start_line) {
    return false;
  }
  --content->line;
  content->column = 0;
  content->token = 0;
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
  return Token::CreateError(content->GetTokenIndex(), &kErrorNotImplemented);
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
  content->column = token_info.column + token_info.size;
  return true;
}

}  // namespace gb
