// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_PARSE_RESULT_H_
#define GB_PARSE_PARSE_RESULT_H_

#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "gb/parse/parse_types.h" 
#include "gb/parse/parse_error.h"
#include "gb/parse/token.h"

namespace gb {

class ParsedItem final {
 public:
  ParsedItem() = default;
  ParsedItem(const ParsedItem&) = delete;
  ParsedItem& operator=(const ParsedItem&) = delete;
  ParsedItem(ParsedItem&&) = default;
  ParsedItem& operator=(ParsedItem&&) = default;
  ~ParsedItem() = default;

  void SetToken(Token token) { token_ = token; }
  Token GetToken() const { return token_; }

  void AddItem(std::string_view name, ParsedItem result) {
    items_[name].emplace_back(std::move(result));
  }
  absl::Span<const ParsedItem> GetItem(std::string_view name) const;

 private:
  Token token_;
  absl::flat_hash_map<std::string, std::vector<ParsedItem>> items_;
};

class ParseResult {
 public:
  ParseResult(ParseError error) : error_(std::move(error)) {}
  ParseResult(ParsedItem item) : item_(std::move(item)) {}
  ParseResult(const ParseResult&) = delete;
  ParseResult& operator=(const ParseResult&) = delete;
  ParseResult(ParseResult&&) = default;
  ParseResult& operator=(ParseResult&&) = default;
  ~ParseResult() = default;

  operator bool() const { return item_.has_value(); }
  bool IsOk() const { return item_.has_value(); }

  const ParseError& GetError() const { return *error_; }

  const ParsedItem& operator*() const& { return *item_; }
  const ParsedItem* operator->() const& { return &*item_; }
  ParsedItem operator*() && { return *std::move(item_); }

 private:
  std::optional<ParseError> error_;
  std::optional<ParsedItem> item_;
};

}  // namespace gb

#endif  // GB_PARSE_PARSE_RESULT_H_
