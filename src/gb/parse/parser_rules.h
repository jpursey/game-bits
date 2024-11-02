// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PARSE_PARSER_RULES_H_
#define GB_PARSE_PARSER_RULES_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "gb/base/callback.h"
#include "gb/base/flags.h"
#include "gb/parse/parse_result.h"
#include "gb/parse/parse_types.h"

namespace gb {

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

  static std::unique_ptr<ParserToken> CreateToken(
      TokenType token_type, std::string_view token_text = {});
  static std::unique_ptr<ParserRuleName> CreateRule(std::string rule_name);
  static std::unique_ptr<ParserGroup> CreateSequence();
  static std::unique_ptr<ParserGroup> CreateAlternatives();

 protected:
  friend class Parser;
  friend class ParserRules;
  friend class ParserGroup;

  struct ValidateContext {
    const ParserRules& rules;
    const Lexer& lexer;
    std::string error;
  };

  ParserRuleItem() = default;

  virtual bool Validate(ValidateContext& context) const = 0;
  virtual ParseMatch Match(ParserInternal, Parser& parser) const = 0;
};

class ParserToken final : public ParserRuleItem {
 public:
  ParserToken(TokenType token_type, std::string_view token_text)
      : token_type_(token_type), token_text_(token_text) {}

  TokenType GetTokenType() const { return token_type_; }
  const TokenValue& GetValue() const { return value_; }

 protected:
  bool Validate(ValidateContext& context) const override;
  ParseMatch Match(ParserInternal, Parser& parser) const override;

 private:
  const TokenType token_type_;
  const std::string token_text_;
  mutable TokenValue value_;
};

class ParserRuleName final : public ParserRuleItem {
 public:
  explicit ParserRuleName(std::string rule_name)
      : rule_name_(std::move(rule_name)) {}

  std::string_view GetRuleName() const { return rule_name_; }

 protected:
  bool Validate(ValidateContext& context) const override;
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

 protected:
  bool Validate(ValidateContext& context) const override;
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

  bool Validate(Lexer& lexer, std::string* error_message = nullptr) const;

 private:
  absl::flat_hash_map<std::string, std::unique_ptr<const ParserRuleItem>>
      rules_;
};

inline std::unique_ptr<ParserToken> ParserRuleItem::CreateToken(
    TokenType token_type, std::string_view token_text) {
  return std::make_unique<ParserToken>(token_type, token_text);
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

#endif  // GB_PARSE_PARSER_RULES_H_
