// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_rules.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "gb/parse/parser.h"

namespace gb {

bool ParserToken::Validate(ValidateContext& context) const {
  const TokenTypeNames& token_names = context.lexer.GetUserTokenNames();
  if (!context.lexer.IsValidTokenType(token_type_)) {
    context.error = absl::StrCat("Invalid token type: ",
                                 GetTokenTypeString(token_type_, &token_names));
    return false;
  }
  if (token_text_.empty()) {
    DCHECK(std::holds_alternative<NoTokenValue>(value_));
    if (token_type_ == kTokenSymbol || token_type_ == kTokenKeyword) {
      context.error =
          absl::StrCat("Token ", GetTokenTypeString(token_type_, &token_names),
                       " must have a value specified.");
      return false;
    }
    return true;
  }
  if (!std::holds_alternative<NoTokenValue>(value_)) {
    return true;
  }
  Token token = context.lexer.ParseTokenText(token_text_);
  if (token.GetType() != token_type_) {
    context.error =
        absl::StrCat("Token text \"", token_text_, "\" is invalid token for ",
                     GetTokenTypeString(token_type_, &token_names));
    return false;
  }
  value_ = token.GetValue();
  return true;
}

parser_internal::ParseMatch ParserToken::Match(Parser& parser) const {
  return parser.MatchTokenItem(*this);
}

bool ParserRuleName::Validate(ValidateContext& context) const {
  if (rule_name_.empty()) {
    context.error = "Rule name cannot be empty";
    return false;
  }
  const ParserRuleItem* rule = context.rules.GetRule(rule_name_);
  if (rule == nullptr) {
    context.error = absl::StrCat("Rule \"", rule_name_, "\" not defined");
    return false;
  }
  if (context.left_recursive_rules.contains(rule_name_)) {
    context.error = absl::StrCat("Rule \"", rule_name_, "\" is left-recursive");
    return false;
  }
  if (context.validated_rules.insert(rule_name_).second) {
    context.left_recursive_rules.insert(rule_name_);
    if (!rule->Validate(context)) {
      return false;
    }
    context.left_recursive_rules.erase(rule_name_);
  }
  return true;
}

parser_internal::ParseMatch ParserRuleName::Match(Parser& parser) const {
  return parser.MatchRuleItem(*this);
}

bool ParserGroup::Validate(ValidateContext& context) const {
  bool allow_optional = (type_ == Type::kSequence);
  bool requires_one = false;
  if (sub_items_.empty()) {
    context.error = "Group must contain at least one item";
    return false;
  }
  bool is_first_token = true;
  absl::flat_hash_set<std::string_view> left_recursive_rules;
  for (const auto& [name, item, repeat] : sub_items_) {
    if (item == nullptr) {
      context.error = "Sub-item cannot be null";
      return false;
    }
    if (!allow_optional && !repeat.IsSet(ParserRepeat::kRequireOne)) {
      context.error = "Alternative cannot be optional";
      return false;
    }
    if (!item->Validate(context)) {
      return false;
    }
    if (repeat.IsSet(ParserRepeat::kRequireOne)) {
      requires_one = true;
      if (type_ == Type::kSequence && is_first_token) {
        is_first_token = false;
        left_recursive_rules.swap(context.left_recursive_rules);
      }
    }
  }
  if (!requires_one) {
    context.error = "Group must contain at least one required item";
    return false;
  }
  if (!is_first_token) {
    context.left_recursive_rules.swap(left_recursive_rules);
  }
  return true;
}

parser_internal::ParseMatch ParserGroup::Match(Parser& parser) const {
  return parser.MatchGroup(*this);
}

bool ParserRules::Validate(Lexer& lexer, std::string* error_message) const {
  if (rules_.empty()) {
    if (error_message != nullptr) {
      *error_message = "No rules defined";
    }
    return false;
  }
  ParserRuleItem::ValidateContext context = {*this, lexer};
  for (const auto& [name, rule] : rules_) {
    if (name.empty()) {
      context.error = absl::StrCat("Invalid rule name: \"", name, "\"");
      break;
    }
    if (context.validated_rules.insert(name).second) {
      context.left_recursive_rules.insert(name);
      if (!rule->Validate(context)) {
        break;
      }
      context.left_recursive_rules.erase(name);
    }
  }
  if (error_message != nullptr) {
    *error_message = context.error;
  }
  return context.error.empty();
}

}  // namespace gb