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

class ParseMatch {
 public:
  static ParseMatch Abort(std::string message) {
    return ParseMatch(ParseError(std::move(message)), true);
  }
  static ParseMatch Abort(ParseError error) {
    return ParseMatch(std::move(error), true);
  }

  ParseMatch(Callback<ParseError()> error_callback)
      : error_callback_(std::move(error_callback)) {}
  ParseMatch(ParseError error, bool abort)
      : abort_(abort), error_callback_([error] { return error; }) {}
  ParseMatch(ParsedItem item) : item_(std::move(item)) {}
  ParseMatch(const ParseMatch&) = delete;
  ParseMatch& operator=(const ParseMatch&) = delete;
  ParseMatch(ParseMatch&&) = default;
  ParseMatch& operator=(ParseMatch&&) = default;
  ~ParseMatch() = default;

  operator bool() const { return item_.has_value(); }

  ParseError GetError() const { return error_callback_(); }
  bool IsAbort() const { return abort_; }

  const ParsedItem& operator*() const& { return *item_; }
  const ParsedItem* operator->() const& { return &*item_; }
  ParsedItem operator*() && { return *std::move(item_); }

 private:
  bool abort_ = false;
  Callback<ParseError()> error_callback_;
  std::optional<ParsedItem> item_;
};

enum class ParserRepeat {
  kRequireOne,
  kAllowMany,
  kWithComma,
};
using ParserRepeatFlags = Flags<ParserRepeat>;

constexpr ParserRepeatFlags kParserOptional = {};
constexpr ParserRepeatFlags kParserSingle = {ParserRepeat::kRequireOne};
constexpr ParserRepeatFlags kParserZeroOrMore = {ParserRepeat::kAllowMany};
constexpr ParserRepeatFlags kParserOneOrMore = {ParserRepeat::kRequireOne,
                                                ParserRepeat::kAllowMany};
constexpr ParserRepeatFlags kParserZeroOrMoreWithComma = {
    ParserRepeat::kAllowMany, ParserRepeat::kWithComma};
constexpr ParserRepeatFlags kParserOneOrMoreWithComma = {
    ParserRepeat::kRequireOne, ParserRepeat::kAllowMany,
    ParserRepeat::kWithComma};

class ParserToken;
class ParserGroup;
class ParserRuleName;

class ParserRuleItem {
 public:
  ParserRuleItem(const ParserRuleItem&) = delete;
  ParserRuleItem& operator=(const ParserRuleItem&) = delete;
  virtual ~ParserRuleItem() = default;

  static std::unique_ptr<ParserToken> CreateToken(TokenType token_type,
                                                  std::string_view value = {});
  static std::unique_ptr<ParserRuleName> CreateRule(std::string rule_name);
  static std::unique_ptr<ParserGroup> CreateSequence();
  static std::unique_ptr<ParserGroup> CreateAlternatives();

  virtual ParseMatch Match(ParserInternal, Parser& parser) const = 0;

 protected:
  ParserRuleItem() = default;
};

class ParserToken final : public ParserRuleItem {
 public:
  ParserToken(TokenType token_type, std::string_view value = {})
      : token_type_(token_type), value_(value) {}

  TokenType GetTokenType() const { return token_type_; }
  std::string_view GetValue() const { return value_; }

  ParseMatch Match(ParserInternal, Parser& parser) const override;

 private:
  const TokenType token_type_;
  const std::string value_;
};

class ParserRuleName final : public ParserRuleItem {
 public:
  explicit ParserRuleName(std::string rule_name)
      : rule_name_(std::move(rule_name)) {}

  std::string_view GetRuleName() const { return rule_name_; }

  ParseMatch Match(ParserInternal, Parser& parser) const override;

 private:
  const std::string rule_name_;
};

class ParserGroup final : public ParserRuleItem {
 public:
  enum class Type {
    kSequence,
    kAlternatives,
  };

  struct SubItem {
    SubItem(std::string_view name, std::unique_ptr<ParserRuleItem> item,
            ParserRepeatFlags repeat)
        : name(name), item(std::move(item)), repeat(repeat) {}
    std::string name;
    std::unique_ptr<const ParserRuleItem> item;
    ParserRepeatFlags repeat;
  };

  ParserGroup(Type type) : type_(type) {}

  void AddSubItem(std::string_view name, std::unique_ptr<ParserRuleItem> item,
                  ParserRepeatFlags repeat = kParserSingle) {
    sub_items_.emplace_back(name, std::move(item), repeat);
  }
  void AddSubItem(std::unique_ptr<ParserRuleItem> item,
                  ParserRepeatFlags repeat = kParserSingle) {
    AddSubItem("", std::move(item), repeat);
  }

  Type GetType() const { return type_; }
  absl::Span<const SubItem> GetSubItems() const { return sub_items_; }

  ParseMatch Match(ParserInternal, Parser& parser) const override;

 private:
  const Type type_;
  std::vector<SubItem> sub_items_;
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
                       std::unique_ptr<ParserGroup> item) {
    rules_.emplace(std::string(name), std::move(item));
    return *this;
  }

  const ParserRuleItem* GetRule(std::string_view name) const {
    auto it = rules_.find(std::string(name));
    return it != rules_.end() ? it->second.get() : nullptr;
  }

 private:
  absl::flat_hash_map<std::string, std::unique_ptr<const ParserRuleItem>>
      rules_;
};

class Parser final {
 public:
  Parser(Lexer& lexer, ParserRules rules)
      : lexer_(lexer), rules_(std::move(rules)) {}
  Parser(const Parser&) = delete;
  Parser& operator=(const Parser&) = delete;
  ~Parser() = default;

  ParseResult Parse(LexerContentId content, std::string_view rule);

  ParseMatch MatchTokenItem(ParserInternal internal, const ParserToken& item);
  ParseMatch MatchSequence(ParserInternal internal, const ParserGroup& item);
  ParseMatch MatchAlternatives(ParserInternal internal, const ParserGroup& item);
  ParseMatch MatchRuleItem(ParserInternal internal, const ParserRuleName& item);

 private:
  ParseError Error(gb::Token token, std::string_view message);

  Callback<ParseError()> TokenErrorCallback(gb::Token token,
                                            TokenType expected_type,
                                            std::string_view expected_value);

  Token NextToken() { return lexer_.NextToken(content_); }
  Token PeekToken() { return lexer_.NextToken(content_, false); }
  void SetNextToken(Token token) { lexer_.SetNextToken(token); }

  Lexer& lexer_;
  const ParserRules rules_;
  LexerContentId content_ = kNoLexerContent;
};

inline std::unique_ptr<ParserToken> ParserRuleItem::CreateToken(
    TokenType token_type, std::string_view value) {
  return std::make_unique<ParserToken>(token_type, value);
}

inline std::unique_ptr<ParserRuleName> ParserRuleItem::CreateRule(
    std::string rule_name) {
  return std::make_unique<ParserRuleName>(std::move(rule_name));
}

inline std::unique_ptr<ParserGroup> ParserRuleItem::CreateSequence() {
  return std::make_unique<ParserGroup>(ParserGroup::Type::kSequence);
}

inline std::unique_ptr<ParserGroup> ParserRuleItem::CreateAlternatives() {
  return std::make_unique<ParserGroup>(ParserGroup::Type::kAlternatives);
}

}  // namespace gb

#endif  // GB_PARSE_PARSER_H_
