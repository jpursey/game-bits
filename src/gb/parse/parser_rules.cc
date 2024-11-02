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
  if (token_type_ == kTokenNone || token_type_ == kTokenEnd ||
      token_type_ == kTokenError) {
    context.error =
        absl::StrCat("Invalid token type: ", GetTokenTypeString(token_type_));
    return false;
  }
  if (token_text_.empty()) {
    DCHECK(std::holds_alternative<NoTokenValue>(value_));
    if (token_type_ == kTokenSymbol || token_type_ == kTokenKeyword) {
      context.error = absl::StrCat("Token ", GetTokenTypeString(token_type_),
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
    context.error = absl::StrCat("Token text \"", token_text_,
                                 "\" is invalid token for ",
                                 GetTokenTypeString(token_type_));
    return false;
  }
  value_ = token.GetValue();
  return true;
}

ParseMatch ParserToken::Match(ParserInternal internal, Parser& parser) const {
  return parser.MatchTokenItem(internal, *this);
}

bool ParserRuleName::Validate(ValidateContext& context) const {
  if (rule_name_.empty()) {
    context.error = "Rule name cannot be empty";
    return false;
  }
  if (context.rules.GetRule(rule_name_) == nullptr) {
    context.error = absl::StrCat("Rule \"", rule_name_, "\" not defined");
    return false;
  }
  return true;
}

ParseMatch ParserRuleName::Match(ParserInternal internal,
                                 Parser& parser) const {
  return parser.MatchRuleItem(internal, *this);
}

bool ParserGroup::Validate(ValidateContext& context) const {
  bool allow_optional = (type_ == Type::kSequence);
  bool requires_one = false;
  if (sub_items_.empty()) {
    context.error = "Group must contain at least one item";
    return false;
  }
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
    }
  }
  if (!requires_one) {
    context.error = "Group must contain at least one required item";
    return false;
  }
  return true;
}

ParseMatch ParserGroup::Match(ParserInternal internal, Parser& parser) const {
  return parser.MatchGroup(internal, *this);
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
    if (!rule->Validate(context)) {
      break;
    }
  }
  if (error_message != nullptr) {
    *error_message = context.error;
  }
  return context.error.empty();
}

}  // namespace gb