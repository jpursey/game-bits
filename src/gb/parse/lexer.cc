// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer.h"

namespace gb {

Lexer::Lexer(const LexerConfig& config) {}

LexerContentId Lexer::AddFileContent(std::string_view filename,
                                     std::string content) {
  // TODO
  return 0;
}

LexerContentId Lexer::AddContent(std::string content) {
  // TODO
  return 0;
}

LexerContentId Lexer::GetContent(std::string_view filename) {
  // TODO
  return 0;
}

std::string_view Lexer::GetFilename(LexerContentId id) {
  // TODO
  return {};
}

TokenLocation Lexer::GetTokenLocation(const Token& token) {
  // TODO
  return {};
}

std::string_view Lexer::GetTokenText(const Token& token) {
  // TODO
  return {};
}

Token Lexer::NextToken(LexerContentId id) {
  // TODO
  return Token::TokenEnd(LexerInternal{}, 0);
}

void Lexer::NextLine(LexerContentId id) {
  // TODO
}

bool Lexer::RewindToken(LexerContentId id) {
  // TODO
  return false;
}

void Lexer::RewindLine(LexerContentId id) {
  // TODO
}

void Lexer::RewindContent(LexerContentId id) {
  // TODO
}

}  // namespace gb
