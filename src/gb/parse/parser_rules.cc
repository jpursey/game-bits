// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_rules.h"

#include "absl/log/check.h"
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
    if (token_type_ == kTokenSymbol || token_type_ == kTokenKeyword) {
      return ValidateError(
          context,
          absl::StrCat("Token ", GetTokenTypeString(token_type_, &token_names),
                       " must have a value specified."));
    }
    return true;
  }
  Token token = context.lexer.ParseTokenText(token_text_);
  if (token.GetType() != token_type_) {
    return ValidateError(
        context,
        absl::StrCat("Token text \"", token_text_, "\" is invalid token for ",
                     GetTokenTypeString(token_type_, &token_names)));
  }
  (*context.token_values)[this] = token.GetValue();
  return true;
}

parser_internal::ParseMatch ParserToken::Match(Parser& parser) const {
  return parser.MatchTokenItem(*this);
}

std::string ParserRuleName::ToString() const {
  if (!scope_items_) {
    return absl::StrCat("<", rule_name_, ">");
  }
  return rule_name_;
}

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

bool ParserRuleName::MatchesEmpty(const ParserRules& rules) const {
  const ParserRuleItem* rule = rules.GetRule(rule_name_);
  DCHECK(rule != nullptr);  // Handled during rule validation.
  return rule->MatchesEmpty(rules);
}

parser_internal::ParseMatch ParserRuleName::Match(Parser& parser) const {
  return parser.MatchRuleItem(*this);
}

std::string ParserGroup::ToString() const {
  std::string result;
  for (const auto& [name, item, repeat, error] : sub_items_) {
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
    if (!error.empty()) {
      absl::StrAppend(&result, ":\"", error, "\"");
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
  for (const auto& [name, item, repeat, error] : sub_items_) {
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

      // An item that can match without consuming anything leaves the item after
      // it first as well.
      if (type_ == Type::kSequence && is_first_token &&
          !item->MatchesEmpty(context.rules)) {
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

bool ParserGroup::MatchesEmpty(const ParserRules& rules) const {
  // A sequence matches empty if every required item can, and alternatives if
  // any one can.
  for (const auto& [name, item, repeat, error] : sub_items_) {
    const bool item_matches_empty =
        (!repeat.IsSet(ParserRepeat::kRequireOne) || item->MatchesEmpty(rules));
    if (type_ == Type::kSequence && !item_matches_empty) {
      return false;
    }
    if (type_ == Type::kAlternatives && item_matches_empty) {
      return true;
    }
  }
  return type_ == Type::kSequence;
}

parser_internal::ParseMatch ParserGroup::Match(Parser& parser) const {
  return parser.MatchGroup(*this);
}

std::string ParserNegation::ToString() const {
  if (item_ == nullptr) {
    return "!<null>";
  }
  if (item_->IsGroup()) {
    return absl::StrCat("!(", item_->ToString(), ")");
  }
  return absl::StrCat("!", item_->ToString());
}

bool ParserNegation::Validate(ValidateContext& context) const {
  if (item_ == nullptr) {
    return ValidateError(context, "Negated item cannot be null");
  }
  if (!item_->Validate(context)) {
    return false;
  }

  // An item that can match without consuming anything always matches, so its
  // negation never could.
  if (item_->MatchesEmpty(context.rules)) {
    return ValidateError(context,
                         "Negated item cannot match without consuming a token");
  }
  return true;
}

parser_internal::ParseMatch ParserNegation::Match(Parser& parser) const {
  return parser.MatchNegation(*this);
}

std::string ParserAdjacent::ToString() const {
  if (item_ == nullptr) {
    return "^<null>";
  }
  if (item_->IsGroup()) {
    return absl::StrCat("^(", item_->ToString(), ")");
  }
  return absl::StrCat("^", item_->ToString());
}

bool ParserAdjacent::Validate(ValidateContext& context) const {
  if (item_ == nullptr) {
    return ValidateError(context, "Adjacent item cannot be null");
  }

  // The end of the content has no text, so nothing touches it.
  if (item_->IsEndToken()) {
    return ValidateError(context, "The end of the content cannot be adjacent");
  }
  return item_->Validate(context);
}

parser_internal::ParseMatch ParserAdjacent::Match(Parser& parser) const {
  return parser.MatchAdjacent(*this);
}

std::string ParserMarker::ToString() const { return "%here"; }

bool ParserMarker::Validate(ValidateContext&) const { return true; }

parser_internal::ParseMatch ParserMarker::Match(Parser& parser) const {
  return parser.MatchMarker();
}

bool ParserRules::Validate(const Lexer& lexer,
                           std::string* error_message) const {
  return DoValidate(lexer, error_message) != nullptr;
}

std::shared_ptr<const parser_internal::TokenValues> ParserRules::DoValidate(
    const Lexer& lexer, std::string* error_message) const {
  if (rules_.empty()) {
    if (error_message != nullptr) {
      *error_message = "No rules defined";
    }
    return nullptr;
  }
  if (!duplicate_name_.empty()) {
    if (error_message != nullptr) {
      *error_message =
          absl::StrCat("Duplicate rule name: \"", duplicate_name_, "\"");
    }
    return nullptr;
  }
  auto token_values = std::make_shared<parser_internal::TokenValues>();
  ParserRuleItem::ValidateContext context = {*this, lexer, token_values.get()};
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
  if (!context.error.empty()) {
    return nullptr;
  }
  return token_values;
}

}  // namespace gb