// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_PARSER_H_
#define GB_PARSE_PARSER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "gb/base/callback.h"
#include "gb/parse/lexer.h"
#include "gb/parse/parse_types.h"
#include "gb/parse/symbol.h"
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
  absl::Span<const ParsedItem> GetItem(std::string_view name) const {
    absl::Span<const ParsedItem> result;
    if (auto it = items_.find(name); it != items_.end()) {
      result = absl::MakeConstSpan(it->second);
    }
    return result;
  }

 private:
  Token token_;
  absl::flat_hash_map<std::string, std::vector<ParsedItem>> items_;
};

class ParseError final {
 public:
  explicit ParseError(std::string message) : message_(std::move(message)) {}
  ParseError(LexerLocation location, std::string message)
      : location_(location), message_(std::move(message)) {}
  ParseError(const ParseError&) = default;
  ParseError& operator=(const ParseError&) = default;
  ParseError(ParseError&&) = default;
  ParseError& operator=(ParseError&&) = default;
  ~ParseError() = default;

  LexerLocation GetLocation() const { return location_; }
  std::string_view GetMessage() const { return message_; }

  std::string FormatMessage() const;

 private:
  LexerLocation location_;
  std::string message_;
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

  bool IsOk() const { return item_.has_value(); }
  const ParseError& GetError() const { return *error_; }
  const ParsedItem& GetItem() const& { return *item_; }
  ParsedItem GetItem() && { return *std::move(item_); }

  const ParsedItem& operator*() const& { return *item_; }
  const ParsedItem* operator->() const& { return &*item_; }
  ParsedItem operator*() && { return *std::move(item_); }

 private:
  std::optional<ParseError> error_;
  std::optional<ParsedItem> item_;
};

class ParseMatch {
 public:
  ParseMatch(Callback<ParseError()> error_callback)
      : error_callback_(std::move(error_callback)) {}
  ParseMatch(ParseError error) : error_callback_([error] { return error; }) {}
  ParseMatch(ParsedItem item) : item_(std::move(item)) {}
  ParseMatch(const ParseMatch&) = delete;
  ParseMatch& operator=(const ParseMatch&) = delete;
  ParseMatch(ParseMatch&&) = default;
  ParseMatch& operator=(ParseMatch&&) = default;
  ~ParseMatch() = default;

  operator bool() const { return item_.has_value(); }

  ParseError GetError() const { return error_callback_(); }

  const ParsedItem& operator*() const& { return *item_; }
  const ParsedItem* operator->() const& { return &*item_; }
  ParsedItem operator*() && { return *std::move(item_); }

 private:
  Callback<ParseError()> error_callback_;
  std::optional<ParsedItem> item_;
};

enum class ParserRepeat {
  kRequireOne,
  kAllowMany,
  kWithComma,
};
using ParserRepeatFlags = Flags<ParserRepeat>;

class ParserItem {
 public:
  ParserItem(const ParserItem&) = delete;
  ParserItem& operator=(const ParserItem&) = delete;
  virtual ~ParserItem() = default;

  virtual ParseMatch Match(ParserInternal, Parser& parser) const = 0;

 protected:
  ParserItem() = default;
};

class ParserTokenItem final : public ParserItem {
 public:
  ParserTokenItem(TokenType token_type, std::string value)
      : token_type_(token_type), value_(std::move(value)) {}

  TokenType GetTokenType() const { return token_type_; }
  std::string_view GetValue() const { return value_; }

  ParseMatch Match(ParserInternal, Parser& parser) const override;

 private:
  const TokenType token_type_;
  const std::string value_;
};

class ParserGroupItem final : public ParserItem {
 public:
  enum class GroupType {
    kSequence,
    kAlternatives,
  };

  struct SubItem {
    SubItem(std::string_view name, std::unique_ptr<ParserItem> item,
            ParserRepeatFlags repeat)
        : name(name), item(std::move(item)), repeat(repeat) {}
    std::string name;
    std::unique_ptr<const ParserItem> item;
    ParserRepeatFlags repeat;
  };

  ParserGroupItem(GroupType group_type) : group_type_(group_type) {}

  void AddSubItem(std::string_view name, std::unique_ptr<ParserItem> item,
                  ParserRepeatFlags repeat) {
    sub_items_.emplace_back(name, std::move(item), repeat);
  }

  GroupType GetGroupType() const { return group_type_; }
  absl::Span<const SubItem> GetSubItems() const { return sub_items_; }

  ParseMatch Match(ParserInternal, Parser& parser) const override;

 private:
  const GroupType group_type_;
  std::vector<SubItem> sub_items_;
};

class ParserRuleItem final : public ParserItem {
 public:
  ParserRuleItem(std::string name, std::string rule_name, ParserRepeat repeat)
      : rule_name_(std::move(rule_name)) {}

  std::string_view GetRuleName() const { return rule_name_; }

  ParseMatch Match(ParserInternal, Parser& parser) const override;

 private:
  const std::string rule_name_;
};

class ParserRules final {
 public:
  ParserRules() = default;
  ParserRules(const ParserRules&) = delete;
  ParserRules& operator=(const ParserRules&) = delete;
  ParserRules(ParserRules&&) = default;
  ParserRules& operator=(ParserRules&&) = default;
  ~ParserRules() = default;

  ParserRules& AddRule(std::string_view name,
                       std::unique_ptr<ParserGroupItem> item) {
    rules_.emplace(std::string(name), std::move(item));
    return *this;
  }

  const ParserItem* GetRule(std::string_view name) const {
    auto it = rules_.find(std::string(name));
    return it != rules_.end() ? it->second.get() : nullptr;
  }

 private:
  absl::flat_hash_map<std::string, std::unique_ptr<const ParserItem>> rules_;
};

class Parser final {
 public:
  Parser(Lexer& lexer, ParserRules rules)
      : lexer_(lexer), rules_(std::move(rules)) {}
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;
  ~Parser();

  ParseResult Parse(LexerContentId content, std::string_view rule);

  ParseMatch MatchTokenItem(ParserInternal internal,
                            const ParserTokenItem& item);
  ParseMatch MatchSequence(ParserInternal internal,
                           const ParserGroupItem& item);
  ParseMatch MatchAlternatives(ParserInternal internal,
                               const ParserGroupItem& item);
  ParseMatch MatchRuleItem(ParserInternal internal, const ParserRuleItem& item);

 private:
  ParseError Error(gb::Token token, std::string_view message);

  Callback<ParseError()> TokenErrorCallback(gb::Token token,
                                            TokenType expected_type,
                                            std::string_view expected_value);

  Token NextToken() { return lexer_.NextToken(content_); }
  void RewindToken() { lexer_.RewindToken(content_); }
  Token PeekToken() {
    Token token = NextToken();
    RewindToken();
    return token;
  }
  void SetNextToken(Token token) { lexer_.SetNextToken(token); }

  Lexer& lexer_;
  const ParserRules rules_;
  LexerContentId content_ = kNoLexerContent;
};

}  // namespace gb

#endif  // GB_PARSE_PARSER_H_
