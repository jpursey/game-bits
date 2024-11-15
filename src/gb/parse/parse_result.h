// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_PARSE_RESULT_H_
#define GB_PARSE_PARSE_RESULT_H_

#include <map>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "gb/parse/parse_error.h"
#include "gb/parse/parse_types.h"
#include "gb/parse/token.h"

namespace gb {

//==============================================================================
// ParsedItem
//==============================================================================

class ParsedItem;
// using ParsedItems = absl::flat_hash_map<std::string,
// std::vector<ParsedItem>>;
using ParsedItems = std::map<std::string, std::vector<ParsedItem>>;

// This class represents a single parsed item in a parse tree.
//
// A parsed item contains the first token that was part of the match. If
// this was group match, it will also contain a map of named sub-items that were
// matched within the group.
class ParsedItem final {
 public:
  ParsedItem() = default;
  ParsedItem(const ParsedItem&) = delete;
  ParsedItem& operator=(const ParsedItem&) = delete;
  ParsedItem(ParsedItem&&) = default;
  ParsedItem& operator=(ParsedItem&&) = default;
  ~ParsedItem() = default;

  // Returns the token that was the beginning of the matched item.
  Token GetToken() const { return token_; }

  // Token helpers
  bool IsEnd() const { return GetToken().IsEnd(); }
  bool IsInt() const { return GetToken().IsInt(); }
  bool IsInt(int64_t value) const { return GetToken().IsInt(value); }
  bool IsUint() const { return GetToken().IsUint(); }
  bool IsUint(uint64_t value) const { return GetToken().IsUint(value); }
  bool IsFloat() const { return GetToken().IsFloat(); }
  bool IsFloat(double value) const { return GetToken().IsFloat(value); }
  bool IsChar() const { return GetToken().IsChar(); }
  bool IsChar(char value) const { return GetToken().IsChar(value); }
  bool IsString() const { return GetToken().IsString(); }
  bool IsString(std::string_view value) const {
    return GetToken().IsString(value);
  }
  bool IsSymbol(Symbol symbol) const { return GetToken().IsSymbol(symbol); }
  bool IsIdent(std::string_view value = {}) const {
    return GetToken().IsIdent(value);
  }
  bool IsKeyword(std::string_view value) const {
    return GetToken().IsKeyword(value);
  }
  bool IsUser(TokenType type, std::string_view value = {}) const {
    return GetToken().IsUser(type, value);
  }

  // Gets all the named sub-items from this item. This will be empty if this
  // item is not a group match. If "name"is scoped (separated by '.'), then
  // GetItems() is called recusively on the the first sub item (if there is
  // one).
  absl::Span<const ParsedItem> GetItems(std::string_view name) const;

  // Gets the name of the sub-items iff there is exactly one name matched. This
  // will be an empty string if there are no matched sub-items, or there
  // ar ematched sub-items with different names.
  std::string_view GetMatchName() const {
    if (items_.size() != 1) {
      return {};
    }
    return items_.begin()->first;
  }

  // Gets the first named sub-item from this item. This will be nullptr if this
  // item is not a group match.
  const ParsedItem* GetItem(std::string_view name) const {
    auto items = GetItems(name);
    return items.empty() ? nullptr : &items[0];
  }

  // Gets the token from the first named sub-item from this item. This will be
  // an empty token if this item is not a group match.
  Token GetToken(std::string_view name) const {
    auto item = GetItem(name);
    return item != nullptr ? item->GetToken() : Token();
  }

  // Gets the symbol from the first named sub-item from this item. This will be
  // an empty (invalid) symbol if this item is not a group match.
  Symbol GetSymbol(std::string_view name) const {
    auto item = GetItem(name);
    return item != nullptr ? item->GetToken().GetSymbol() : Symbol();
  }

  // Gets the integer from the first named sub-item from this item. This will be
  // 0 if this item is not a group match.
  int64_t GetInt(std::string_view name, int default_value = 0) const {
    auto item = GetItem(name);
    return item != nullptr ? item->GetToken().GetInt() : default_value;
  }

  // Gets the float from the first named sub-item from this item. This will be
  // 0.0 if this item is not a group match.
  double GetFloat(std::string_view name, double default_value = 0.0) const {
    auto item = GetItem(name);
    return item != nullptr ? item->GetToken().GetFloat() : default_value;
  }

  // Gets the string from the first named sub-item from this item. This will be
  // an empty string if this item is not a group match.
  std::string_view GetString(std::string_view name,
                             std::string_view default_value = {}) const {
    auto item = GetItem(name);
    return item != nullptr ? item->GetToken().GetString() : default_value;
  }

 private:
  friend class Parser;

  Token token_;
  ParsedItems items_;
};

//==============================================================================
// ParseResult
//==============================================================================

// This class represents the result of a parse operation.
//
// A parse result is either an error or a successfully parsed item. The result
// is "true" if it is a success, and IsOk() will return true as well. Callers
// mush check it is a success before accessing the item.
//
// Similarly, the error can be accessed only if the result is not successful.
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
  const ParsedItem* GetItem() const { return &*item_; }

  const ParsedItem& operator*() const& { return *item_; }
  const ParsedItem* operator->() const& { return &*item_; }
  ParsedItem operator*() && { return *std::move(item_); }

 private:
  std::optional<ParseError> error_;
  std::optional<ParsedItem> item_;
};

}  // namespace gb

#endif  // GB_PARSE_PARSE_RESULT_H_
