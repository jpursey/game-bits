// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_program.h"

#include "absl/memory/memory.h"

namespace gb {
namespace {

std::optional<ParserRules> ParseProgram(Lexer& lexer, std::string program_text,
                                        std::string* error_message) {
  // TODO
  if (error_message != nullptr) {
    *error_message = "Not implemented";
  }
  return std::nullopt;
}

}  // namespace

std::unique_ptr<ParserProgram> ParserProgram::Create(
    LexerConfig config, std::string program_text, std::string* error_message) {
  auto lexer = Lexer::Create(config, error_message);
  if (lexer == nullptr) {
    return nullptr;
  }
  return Create(std::move(lexer), std::move(program_text), error_message);
}

std::unique_ptr<ParserProgram> ParserProgram::Create(
    std::unique_ptr<Lexer> lexer, std::string program_text,
    std::string* error_message) {
  if (lexer == nullptr) {
    if (error_message != nullptr) {
      *error_message = "Lexer is null";
    }
    return nullptr;
  }
  auto rules = ParseProgram(*lexer, std::move(program_text), error_message);
  if (!rules.has_value()) {
    return nullptr;
  }
  return absl::WrapUnique(
      new ParserProgram(std::move(lexer), *std::move(rules)));
}

std::unique_ptr<ParserProgram> ParserProgram::Create(
    Lexer* lexer, std::string program_text, std::string* error_message) {
  auto rules = ParseProgram(*lexer, std::move(program_text), error_message);
  if (!rules.has_value()) {
    return nullptr;
  }
  return absl::WrapUnique(new ParserProgram(lexer, *std::move(rules)));
}

}  // namespace gb
