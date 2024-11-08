// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser.h"

#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_cat.h"

namespace gb {

std::unique_ptr<Parser> Parser::Create(LexerConfig config,
                                       std::shared_ptr<const ParserRules> rules,
                                       std::string* error_message) {
  auto lexer = Lexer::Create(config, error_message);
  if (lexer == nullptr) {
    return nullptr;
  }
  return Create(std::move(lexer), std::move(rules), error_message);
}

std::unique_ptr<Parser> Parser::Create(std::shared_ptr<Lexer> lexer,
                                       std::shared_ptr<const ParserRules> rules,
                                       std::string* error_message) {
  if (lexer == nullptr) {
    if (error_message != nullptr) {
      *error_message = "Lexer is null";
    }
    return nullptr;
  }
  if (rules == nullptr) {
    if (error_message != nullptr) {
      *error_message = "Rules are null";
    }
    return nullptr;
  }
  if (!rules->Validate(*lexer, error_message)) {
    return nullptr;
  }
  return absl::WrapUnique(new Parser(std::move(lexer), std::move(rules)));
}

std::unique_ptr<Parser> Parser::Create(std::unique_ptr<ParserProgram> program) {
  if (program == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new Parser(std::move(program->lexer_), std::move(program->rules_)));
}

ParseError Parser::Error(Token token, std::string_view message) {
  LexerLocation location = lexer_->GetTokenLocation(token);
  if (token.GetType() == kTokenError) {
    message = token.GetString();
    if (location.line < 0) {
      return ParseError(absl::StrCat("Parse error: ", message));
    }
  }
  return ParseError(
      location, absl::StrCat("Parse error at \"", lexer_->GetTokenText(token),
                             "\": ", message));
}

ParseResult Parser::Parse(LexerContentId content, std::string_view rule) {
  const ParserRuleItem* root = rules_->GetRule(rule);
  if (root == nullptr) {
    return ParseError(absl::StrCat("Parser rule \"", rule, "\" not found"));
  }
  content_ = content;
  last_error_.reset();
  items_ = nullptr;
  auto match = root->Match(*this);
  if (match.IsError()) {
    DCHECK(last_error_.has_value() && last_error_->error_callback != nullptr);
    return last_error_->error_callback();
  }
  return *std::move(match);
}

parser_internal::ParseMatch Parser::MatchAbort(ParseError error) {
  last_error_ = ParseMatchError{error};
  return ParseMatch::Abort();
}

parser_internal::ParseMatch Parser::MatchAbort(std::string_view message) {
  last_error_ = ParseMatchError{ParseError(message)};
  return ParseMatch::Abort();
}

parser_internal::ParseMatch Parser::MatchError(
    gb::Token token, TokenType expected_type, std::string_view expected_value) {
  if (last_error_.has_value() && last_error_->token > token) {
    return ParseMatch::Error();
  }
  last_error_ =
      ParseMatchError(token, [this, token, expected_type, expected_value] {
        std::string expected;
        const bool has_value = !expected_value.empty();
        switch (expected_type) {
          case kTokenSymbol:
            expected =
                !has_value ? "symbol" : absl::StrCat("'", expected_value, "'");
            break;
          case kTokenInt:
            expected = !has_value ? "integer value" : expected_value;
            break;
          case kTokenFloat:
            expected = !has_value ? "floating-point value" : expected_value;
            break;
          case kTokenChar:
            expected = !has_value ? "character value" : expected_value;
            break;
          case kTokenString:
            expected = !has_value ? "string value" : expected_value;
            break;
          case kTokenKeyword:
            expected = !has_value ? "keyword" : expected_value;
            break;
          case kTokenIdentifier:
            expected = !has_value ? "identifier"
                                  : absl::StrCat("identifier ", expected_value);
            break;
          case kTokenLineBreak:
            expected = "end of line";
            break;
          case kTokenEnd:
            expected = "end of file";
            break;
          default: {
            const std::string type_name =
                GetTokenTypeString(expected_type, &lexer_->GetUserTokenNames());
            expected = !has_value
                           ? type_name
                           : absl::StrCat(type_name, " ", expected_value);
          } break;
        }
        return Error(token, absl::StrCat("Expected ", expected));
      });
  return ParseMatch::Error();
}

parser_internal::ParseMatch Parser::Match(ParsedItem item) {
  return ParseMatch::Item(std::move(item));
}

parser_internal::ParseMatch Parser::MatchTokenItem(
    const ParserToken& parser_token) {
  Token token = PeekToken();
  if (token.GetType() == kTokenError) {
    return MatchAbort(Error(token, token.GetString()));
  }

  TokenType expected_type = parser_token.GetTokenType();
  const TokenValue& expected_value = parser_token.GetValue();
  if (token.GetType() != expected_type ||
      (!std::holds_alternative<NoTokenValue>(expected_value) &&
       token.GetValue() != expected_value)) {
    return MatchError(token, expected_type, parser_token.GetTokenText());
  }

  NextToken();
  ParsedItem parsed;
  parsed.token_ = token;
  return Match(std::move(parsed));
}

parser_internal::ParseMatch Parser::MatchRuleItem(
    const ParserRuleName& parser_rule_name) {
  const ParserRuleItem* rule = rules_->GetRule(parser_rule_name.GetRuleName());
  DCHECK(rule != nullptr);  // Handled during rule validation.
  ParsedItems* old_items = std::exchange(items_, nullptr);
  ParseMatch match = rule->Match(*this);
  items_ = old_items;
  return match;
}

parser_internal::ParseMatch Parser::MatchGroup(const ParserGroup& group) {
  Token group_token = PeekToken();
  if (group_token.IsError()) {
    return MatchAbort(Error(group_token, group_token.GetString()));
  }
  const bool is_sequence = (group.GetType() == ParserGroup::Type::kSequence);
  const bool is_alternatives =
      (group.GetType() == ParserGroup::Type::kAlternatives);
  ParsedItem result;
  ParsedItems* old_items = items_;
  if (items_ == nullptr) {
    items_ = &result.items_;
  }
  absl::Cleanup cleanup = [this, old_items] { items_ = old_items; };
  bool has_match = false;
  for (const auto& sub_item : group.GetSubItems()) {
    ParsedItems* current_items = items_;
    if (!sub_item.name.empty()) {
      items_ = nullptr;
    }
    auto match = sub_item.item->Match(*this);
    items_ = current_items;
    if (match.IsError()) {
      if (match.IsAbort() ||
          (is_sequence && sub_item.repeat.IsSet(ParserRepeat::kRequireOne))) {
        SetNextToken(group_token);
        return match;
      }
      continue;
    }
    has_match = true;
    if (result.token_.IsNone()) {
      result.token_ = group_token;
    }
    if (!sub_item.name.empty()) {
      (*items_)[sub_item.name].push_back(*std::move(match));
    }
    if (!sub_item.repeat.IsSet(ParserRepeat::kAllowMany)) {
      if (is_alternatives) {
        break;
      }
      continue;
    }
    const bool with_comma = sub_item.repeat.IsSet(ParserRepeat::kWithComma);
    while (true) {
      if (with_comma) {
        if (PeekToken().IsSymbol(',')) {
          NextToken();
        } else {
          break;
        }
      }
      match = sub_item.item->Match(*this);
      if (match.IsError()) {
        if (match.IsAbort()) {
          SetNextToken(group_token);
          return match;
        }
        if (with_comma) {
          RewindToken();
        }
        break;
      }
      if (!sub_item.name.empty()) {
        (*items_)[sub_item.name].push_back(*std::move(match));
      }
    }
    if (is_alternatives) {
      break;
    }
  }
  if (is_alternatives && !has_match) {
    return ParseMatch::Error();
  }
  return Match(std::move(result));
}

}  // namespace gb
