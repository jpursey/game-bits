// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_rules.h"

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "gb/parse/parser.h"

namespace gb {

bool ParserToken::Validate(const ParserRules& rules, const Lexer& lexer,
                           std::string* error_message) const {
  if (token_type_ == kTokenNone || token_type_ == kTokenEnd ||
      token_type_ == kTokenError) {
    if (error_message != nullptr) {
      *error_message =
          absl::StrCat("Invalid token type: ", GetTokenTypeString(token_type_));
    }
    return false;
  }
  if (std::holds_alternative<NoTokenValue>(value_)) {
    return true;
  }
  switch (token_type_) {
    case kTokenInt:
      if (!std::holds_alternative<int64_t>(value_)) {
        if (error_message != nullptr) {
          *error_message = absl::StrCat(
              "Invalid token value for integer token: \"", value_, "\"");
        }
        return false;
      }
      break;
    case kTokenFloat:
      if (!std::holds_alternative<double>(value_)) {
        if (error_message != nullptr) {
          *error_message = absl::StrCat(
              "Invalid token value for floating-point token: \"", value_, "\"");
        }
        return false;
      }
      break;
    case kTokenString:
      if (!std::holds_alternative<std::string>(value_)) {
        if (error_message != nullptr) {
          *error_message = absl::StrCat(
              "Invalid token value for string token: \"", value_, "\"");
        }
        return false;
      }
      break;
    case kTokenChar:
      if (!std::holds_alternative<std::string>(value_) ||
          std::get<std::string>(value_).size() != 1) {
        if (error_message != nullptr) {
          *error_message = absl::StrCat(
              "Invalid token value for character token: \"", value_, "\"");
        }
        return false;
      }
      break;
    case kTokenSymbol:
      if (!std::holds_alternative<Symbol>(value_) ||
          !std::get<Symbol>(value_).IsValid() ||
          lexer.ParseTokenText(absl::StrCat(value_)).GetType() !=
              kTokenSymbol) {
        if (error_message != nullptr) {
          *error_message = absl::StrCat(
              "Invalid token value for symbol token: \"", value_, "\"");
        }
        return false;
      }
      break;
    default:
      if (!std::holds_alternative<std::string>(value_) ||
          lexer.ParseTokenText(absl::StrCat(value_)).GetType() != token_type_) {
        if (error_message != nullptr) {
          *error_message = absl::StrCat("Invalid token value for ",
                                        GetTokenTypeString(token_type_),
                                        " token: \"", value_, "\"");
        }
        return false;
      }
      break;
  }
  return true;
}

ParseMatch ParserToken::Match(ParserInternal internal, Parser& parser) const {
  return parser.MatchTokenItem(internal, *this);
}

bool ParserRuleName::Validate(const ParserRules& rules, const Lexer& lexer,
                              std::string* error_message) const {
  if (rule_name_.empty()) {
    if (error_message != nullptr) {
      *error_message = "Rule name cannot be empty";
    }
    return false;
  }
  if (rules.GetRule(rule_name_) == nullptr) {
    if (error_message != nullptr) {
      *error_message = absl::StrCat("Rule \"", rule_name_, "\" not defined");
    }
    return false;
  }
  return true;
}

ParseMatch ParserRuleName::Match(ParserInternal internal,
                                 Parser& parser) const {
  return parser.MatchRuleItem(internal, *this);
}

bool ParserGroup::Validate(const ParserRules& rules, const Lexer& lexer,
                           std::string* error_message) const {
  bool allow_optional = (type_ == Type::kSequence);
  bool requires_one = false;
  if (sub_items_.empty()) {
    if (error_message != nullptr) {
      *error_message = "Group must contain at least one item";
    }
    return false;
  }
  for (const auto& [name, item, repeat] : sub_items_) {
    if (item == nullptr) {
      if (error_message != nullptr) {
        *error_message = "Sub-item cannot be null";
      }
      return false;
    }
    if (!allow_optional && !repeat.IsSet(ParserRepeat::kRequireOne)) {
      if (error_message != nullptr) {
        *error_message = "Alternative cannot be optional";
      }
      return false;
    }
    if (repeat.IsSet(ParserRepeat::kRequireOne)) {
      requires_one = true;
    }
  }
  if (!requires_one) {
    if (error_message != nullptr) {
      *error_message = "Group must contain at least one required item";
    }
    return false;
  }
  return true;
}

ParseMatch ParserGroup::Match(ParserInternal internal, Parser& parser) const {
  return parser.MatchGroup(internal, *this);
}

bool ParserRules::Validate(Lexer& lexer, std::string* error_message) const {
  for (const auto& [name, rule] : rules_) {
    if (name.empty() || !absl::ascii_isalpha(name[0])) {
      if (error_message != nullptr) {
        *error_message = absl::StrCat("Invalid rule name: \"", name, "\"");
      }
      return false;
    }
    for (const char ch : name) {
      if (!isalnum(ch) && ch != '_') {
        if (error_message != nullptr) {
          *error_message = absl::StrCat("Invalid rule name: \"", name, "\"");
        }
        return false;
      }
    }
    if (!rule->Validate(*this, lexer, error_message)) {
      return false;
    }
  }
  return true;
}

}  // namespace gb