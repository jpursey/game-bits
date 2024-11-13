// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_PARSE_ERROR_H_
#define GB_PARSE_PARSE_ERROR_H_

#include <string>
#include <string_view>
#include <utility>

#include "gb/parse/parse_types.h"

namespace gb {

class ParseError final {
 public:
  ParseError() = default;
  explicit ParseError(std::string_view message) : message_(message) {}
  ParseError(LexerLocation location, std::string_view message)
      : location_(std::move(location)), message_(message) {}
  ParseError(const ParseError&) = default;
  ParseError& operator=(const ParseError&) = default;
  ParseError(ParseError&&) = default;
  ParseError& operator=(ParseError&&) = default;
  ~ParseError() = default;

  const LexerLocation& GetLocation() const { return location_; }
  std::string_view GetMessage() const { return message_; }

  std::string FormatMessage() const;

 private:
  LexerLocation location_;
  std::string message_;
};

}  // namespace gb

#endif  // GB_PARSE_PARSE_ERROR_H_
