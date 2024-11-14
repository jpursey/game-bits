// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_rules.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "gb/parse/parser.h"

namespace gb {

bool ParserRuleItem::ValidateError(ValidateContext& context,
                                   std::string_view message) const {
  context.error = absl::StrCat("{ ", ToString(), " }: ", message);
  return false;
}

std::string ParserToken::ToString() const {
  if (!token_text_.empty()) {
    std::string_view quote =
        (token_text_.find('"') != std::string::npos ? "'" : "\"");
    return absl::StrCat(quote, token_text_, quote);
  }
  switch (token_type_) {
    case kTokenNone:
      return "%none";
    case kTokenEnd:
      return "%end";
    case kTokenError:
      return "%error";
    case kTokenSymbol:
      return "%symbol";
    case kTokenInt:
      return "%int";
    case kTokenFloat:
      return "%float";
    case kTokenChar:
      return "%char";
    case kTokenString:
      return "%string";
    case kTokenKeyword:
      return "%keyword";
    case kTokenIdentifier:
      return "%ident";
    case kTokenLineBreak:
      return "%linebreak";
  }
  return absl::StrCat("%", static_cast<int>(token_type_) - kTokenUser);
}

bool ParserToken::Validate(ValidateContext& context) const {
  const TokenTypeNames& token_names = context.lexer.GetUserTokenNames();
  if (!context.lexer.IsValidTokenType(token_type_)) {
    return ValidateError(
        context, absl::StrCat("Invalid token type: ",
                              GetTokenTypeString(token_type_, &token_names)));
  }
  if (token_text_.empty()) {
    DCHECK(std::holds_alternative<NoTokenValue>(value_));
    if (token_type_ == kTokenSymbol || token_type_ == kTokenKeyword) {
      return ValidateError(
          context,
          absl::StrCat("Token ", GetTokenTypeString(token_type_, &token_names),
                       " must have a value specified."));
    }
    return true;
  }
  if (!std::holds_alternative<NoTokenValue>(value_)) {
    return true;
  }
  Token token = context.lexer.ParseTokenText(token_text_);
  if (token.GetType() != token_type_) {
    return ValidateError(
        context,
        absl::StrCat("Token text \"", token_text_, "\" is invalid token for ",
                     GetTokenTypeString(token_type_, &token_names)));
  }
  value_ = token.GetValue();
  return true;
}

parser_internal::ParseMatch ParserToken::Match(Parser& parser) const {
  return parser.MatchTokenItem(*this);
}

std::string ParserRuleName::ToString() const { return rule_name_; }

bool ParserRuleName::Validate(ValidateContext& context) const {
  if (rule_name_.empty()) {
    return ValidateError(context, "Rule name cannot be empty");
  }
  const ParserRuleItem* rule = context.rules.GetRule(rule_name_);
  if (rule == nullptr) {
    return ValidateError(context,
                         absl::StrCat("Rule \"", rule_name_, "\" not defined"));
  }
  if (context.left_recursive_rules.contains(rule_name_)) {
    return ValidateError(
        context, absl::StrCat("Rule \"", rule_name_, "\" is left-recursive"));
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
  return parser.MatchRuleItem(*this, scope_items_);
}

std::string ParserGroup::ToString() const {
  std::string result;
  for (const auto& [name, item, repeat] : sub_items_) {
    if (!result.empty()) {
      absl::StrAppend(&result, " ");
      if (type_ == Type::kAlternatives) {
        absl::StrAppend(&result, "| ");
      }
    }
    if (item == nullptr) {
      absl::StrAppend(&result, "<null>");
      continue;
    }
    if (!name.empty()) {
      absl::StrAppend(&result, "$", name, "=");
    }
    if (repeat == kParserOptional) {
      absl::StrAppend(&result, "[");
    } else if (item->IsGroup()) {
      absl::StrAppend(&result, "(");
    }
    absl::StrAppend(&result, item->ToString());
    if (repeat == kParserOptional) {
      absl::StrAppend(&result, "]");
    } else if (item->IsGroup()) {
      absl::StrAppend(&result, ")");
    }
    if (repeat.IsSet(ParserRepeat::kWithComma)) {
      absl::StrAppend(&result, ",");
    }
    if (repeat.IsSet(ParserRepeat::kAllowMany)) {
      if (repeat.IsSet(ParserRepeat::kRequireOne)) {
        absl::StrAppend(&result, "+");
      } else {
        absl::StrAppend(&result, "*");
      }
    }
  }
  return result;
}

bool ParserGroup::Validate(ValidateContext& context) const {
  bool allow_optional = (type_ == Type::kSequence);
  bool requires_one = false;
  if (sub_items_.empty()) {
    return ValidateError(context, "Group must contain at least one item");
  }
  bool is_first_token = true;
  absl::flat_hash_set<std::string_view> left_recursive_rules;
  for (const auto& [name, item, repeat] : sub_items_) {
    if (item == nullptr) {
      return ValidateError(context, "Sub-item cannot be null");
    }
    if (!allow_optional && !repeat.IsSet(ParserRepeat::kRequireOne)) {
      return ValidateError(context, "Alternative cannot be optional");
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
    return ValidateError(context,
                         "Group must contain at least one required item");
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