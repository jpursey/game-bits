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
#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "gb/base/callback.h"
#include "gb/base/flags.h"
#include "gb/parse/parse_result.h"
#include "gb/parse/parse_types.h"

namespace gb {

namespace parser_internal {
class ParseMatch;
}  // namespace parser_internal

//==============================================================================
// ParserRepeat
//==============================================================================

// ParserRepeat flags are used to specify how many times a rule can be repeated
// within a ParserGroup.
enum class ParserRepeat {
  kRequireOne,  // Must appear at least once.
  kAllowMany,   // Can appear multiple times.
  kWithComma,   // If multiple exist, they must be separated by commas.
};
using ParserRepeatFlags = Flags<ParserRepeat>;

// Common settings for ParserRepeatFlags.
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

//==============================================================================
// ParserRuleItems
//
// Parser rules are defined by a tree of ParserRuleItems rooted in a
// ParserGroup, which are either tokens, rule names, or groups of other
// ParserRuleItems. Together, this tree of rules defines the grammar that the
// parser will use to parse tokens.
//==============================================================================

// Base class for all parser rule items.
//
// There are no public methods on this class, but it does provide factory
// methods to create the various types of parser rule items.
class ParserRuleItem {
 public:
  ParserRuleItem(const ParserRuleItem&) = delete;
  ParserRuleItem& operator=(const ParserRuleItem&) = delete;
  virtual ~ParserRuleItem() = default;

  // Creates a new token rule item. If token_text is provided, it is used as the
  // expected text for the token (in the format that is expected by the lexer
  // that will be used), otherwise any text matching the token type is accepted.
  // kTokenKeyword and kTokenSymbol tokens must have token_text specified.
  static std::unique_ptr<ParserToken> CreateToken(
      TokenType token_type, std::string_view token_text = {});

  // Creates a new rule name rule item.
  //
  // The rule name must be a valid rule name defined in the parser rules. This
  // allows for recursive rules, or just reuse between rules. Rule names cannot
  // be used to create left-recursive rules.
  static std::unique_ptr<ParserRuleName> CreateRuleName(
      std::string_view rule_name, bool scope_items = true);

  // Creates a sequence or alternatives group rule item.
  //
  // Parser groups are the top-level rule items that define the structure of the
  // parser rules. However, they can also be used as sub-items of other groups
  // directly (like any other ParserRuleItem).
  //
  // A sequence group requires all sub-items to match in order, while an
  // alternatives group requires only one sub-item to match.
  static std::unique_ptr<ParserGroup> CreateSequence();
  static std::unique_ptr<ParserGroup> CreateAlternatives();

  // Converts to a string for debugging and testing purposes. The string is
  // in the style of the ParserProgram text format.
  virtual std::string ToString() const = 0;

 protected:
  friend class Parser;
  friend class ParserRules;
  friend class ParserGroup;
  friend class ParserRuleName;

  using ParseMatch = parser_internal::ParseMatch;

  struct ValidateContext {
    const ParserRules& rules;
    const Lexer& lexer;
    std::string error;
    absl::flat_hash_set<std::string_view> validated_rules;
    absl::flat_hash_set<std::string_view> left_recursive_rules;
  };

  ParserRuleItem() = default;

  bool ValidateError(ValidateContext& context, std::string_view message) const;

  virtual bool IsGroup() const { return false; }
  virtual bool Validate(ValidateContext& context) const = 0;
  virtual ParseMatch Match(Parser& parser) const = 0;
};

// A token rule item.
//
// This rule item matches a single token from the lexer. The token type and
// expected text are specified when the token is created. See
// ParserRuleItem::CreateToken for more information.
class ParserToken final : public ParserRuleItem {
 public:
  // Creates a new token rule item.
  ParserToken(TokenType token_type, std::string_view token_text)
      : token_type_(token_type), token_text_(token_text) {}

  TokenType GetTokenType() const { return token_type_; }
  std::string_view GetTokenText() const { return token_text_; }
  const TokenValue& GetValue() const { return value_; }

  std::string ToString() const override;

 protected:
  bool Validate(ValidateContext& context) const override;
  ParseMatch Match(Parser& parser) const override;

 private:
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const ParserToken& token) {
    absl::Format(&sink, "%s", token.ToString());
  }

  const TokenType token_type_;
  const std::string token_text_;
  mutable TokenValue value_;
};

// A rule name rule item.
//
// This rule item matches another rule by name. The rule name must be a valid
// rule name defined in the parser rules. See ParserRuleItem::CreateRuleName for
// more information.
class ParserRuleName final : public ParserRuleItem {
 public:
  // Creates a new rule name rule item.
  explicit ParserRuleName(std::string_view rule_name, bool scope_items = true)
      : rule_name_(rule_name), scope_items_(scope_items) {}

  std::string_view GetRuleName() const { return rule_name_; }
  bool ScopeItems() const { return scope_items_; }

  std::string ToString() const override;

 protected:
  bool Validate(ValidateContext& context) const override;
  ParseMatch Match(Parser& parser) const override;

 private:
  const std::string rule_name_;
  const bool scope_items_;
};

// A group rule item.
//
// This rule item matches a sequence or alternatives group of other rule items.
// See ParserRuleItem::CreateSequence and ParserRuleItem::CreateAlternatives for
// more information.
class ParserGroup final : public ParserRuleItem {
 public:
  // The type of group.
  enum class Type {
    kSequence,      // All sub-items must match in order.
    kAlternatives,  // Only one sub-item must match.
  };

  // A sub-item within a group.
  struct SubItem {
    SubItem(std::string_view in_name, std::unique_ptr<ParserRuleItem> in_item,
            ParserRepeatFlags in_repeat, std::string_view in_error)
        : name(in_name),
          item(std::move(in_item)),
          repeat(in_repeat),
          error(in_error) {}

    // The name of the sub-item, if any. If this is specified, then the value of
    // the sub item will be stored in ParsedItem with this name. Multiple
    // sub-items with the same name are allowed, and will be stored sequentially
    // in the order they appear.
    std::string name;

    // The sub-item to match.
    std::unique_ptr<const ParserRuleItem> item;

    // The repeat flags for this sub-item.
    ParserRepeatFlags repeat;

    // Any explicit error message for a failed match of this sub-item.
    std::string error;
  };

  // Creates a new group rule item of the specified type.
  explicit ParserGroup(Type type) : type_(type) {}

  void AddSubItem(SubItem sub_item) {
    sub_items_.push_back(std::move(sub_item));
  }
  void AddSubItem(std::string_view name, std::unique_ptr<ParserRuleItem> item,
                  ParserRepeatFlags repeat, std::string_view error = "") {
    sub_items_.emplace_back(name, std::move(item), repeat, error);
  }
  void AddSubItem(std::unique_ptr<ParserRuleItem> item,
                  ParserRepeatFlags repeat, std::string_view error = "") {
    sub_items_.emplace_back("", std::move(item), repeat, error);
  }
  void AddSubItem(std::string_view name, std::unique_ptr<ParserRuleItem> item,
                  std::string_view error = "") {
    sub_items_.emplace_back(name, std::move(item), kParserSingle, error);
  }
  void AddSubItem(std::unique_ptr<ParserRuleItem> item,
                  std::string_view error = "") {
    sub_items_.emplace_back("", std::move(item), kParserSingle, error);
  }

  Type GetType() const { return type_; }
  absl::Span<const SubItem> GetSubItems() const { return sub_items_; }

  std::string ToString() const override;

 protected:
  bool IsGroup() const override { return true; }
  bool Validate(ValidateContext& context) const override;
  ParseMatch Match(Parser& parser) const override;

 private:
  const Type type_;
  std::vector<SubItem> sub_items_;
};

//==============================================================================
// ParserRules
//==============================================================================

// The full set of parser rules that define the grammar for a Parser.
class ParserRules final {
 public:
  ParserRules() = default;
  ParserRules(const ParserRules&) = delete;
  ParserRules& operator=(const ParserRules&) = delete;
  ParserRules(ParserRules&&) = default;
  ParserRules& operator=(ParserRules&&) = default;
  ~ParserRules() = default;

  // Adds a rule to the parser rules.
  ParserRules& AddRule(std::string_view name,
                       std::unique_ptr<ParserGroup> item) {
    rules_.emplace(std::string(name), std::move(item));
    return *this;
  }

  // Returns the rule with the specified name, or nullptr if it does not exist.
  const ParserRuleItem* GetRule(std::string_view name) const {
    auto it = rules_.find(std::string(name));
    return it != rules_.end() ? it->second.get() : nullptr;
  }

  // Validates all rules in the parser rules.
  //
  // If this method returns false, then the error_message will be set to a
  // description of the error that occurred. Only valid rules can be used to
  // create a Parser.
  bool Validate(Lexer& lexer, std::string* error_message = nullptr) const;

 private:
  using Rules =
      absl::flat_hash_map<std::string, std::unique_ptr<const ParserRuleItem>>;

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const ParserRules& token) {
    absl::Format(
        &sink, "PaerserRules{%s}",
        absl::StrJoin(rules_, ",",
                      [](std::string* out, const Rules::value_type& value) {
                        absl::StrAppend(out, value.first);
                      }));
  }

  Rules rules_;
};

//==============================================================================
// Inline implementations
//==============================================================================

namespace parser_internal {
// The ParseMatch class is an internal class used during parsing.
class ParseMatch {
 public:
  enum class Type { kAbort, kError, kMatch };

  static ParseMatch Error() { return ParseMatch(Type::kError); }
  static ParseMatch Abort() { return ParseMatch(Type::kAbort); }
  static ParseMatch Item(ParsedItem item) {
    return ParseMatch(Type::kMatch, std::move(item));
  }

  ParseMatch(const ParseMatch&) = delete;
  ParseMatch& operator=(const ParseMatch&) = delete;
  ParseMatch(ParseMatch&&) = default;
  ParseMatch& operator=(ParseMatch&&) = default;
  ~ParseMatch() = default;

  bool IsMatch() const { return type_ == Type::kMatch; }
  bool IsError() const { return type_ != Type::kMatch; }
  bool IsAbort() const { return type_ == Type::kAbort; }

  const ParsedItem& operator*() const& { return *item_; }
  const ParsedItem* operator->() const& { return &*item_; }
  ParsedItem operator*() && { return *std::move(item_); }

 private:
  ParseMatch(Type type) : type_(type) {}
  ParseMatch(Type type, ParsedItem item)
      : type_(type), item_(std::move(item)) {}

  Type type_ = Type::kAbort;
  std::optional<ParsedItem> item_;
};
}  // namespace parser_internal

inline std::unique_ptr<ParserToken> ParserRuleItem::CreateToken(
    TokenType token_type, std::string_view token_text) {
  return std::make_unique<ParserToken>(token_type, token_text);
}

inline std::unique_ptr<ParserRuleName> ParserRuleItem::CreateRuleName(
    std::string_view rule_name, bool scope_items) {
  return std::make_unique<ParserRuleName>(rule_name, scope_items);
}

inline std::unique_ptr<ParserGroup> ParserRuleItem::CreateSequence() {
  return std::make_unique<ParserGroup>(ParserGroup::Type::kSequence);
}

inline std::unique_ptr<ParserGroup> ParserRuleItem::CreateAlternatives() {
  return std::make_unique<ParserGroup>(ParserGroup::Type::kAlternatives);
}

}  // namespace gb

#endif  // GB_PARSE_PARSER_RULES_H_
