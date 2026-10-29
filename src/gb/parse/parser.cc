// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser.h"

#include <iterator>

#include "absl/log/check.h"
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
  auto token_values = rules->DoValidate(*lexer, error_message);
  if (token_values == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new Parser(std::move(lexer), std::move(rules), std::move(token_values)));
}

std::unique_ptr<Parser> Parser::Create(
    std::shared_ptr<const ParserProgram> program) {
  if (program == nullptr) {
    return nullptr;
  }
  auto lexer = Lexer::Create(program->lexer_program_);
  return absl::WrapUnique(
      new Parser(std::move(lexer), program->rules_, program->token_values_));
}

std::unique_ptr<Parser> Parser::Create(
    std::shared_ptr<Lexer> lexer,
    std::shared_ptr<const ParserProgram> program) {
  if (lexer == nullptr || program == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new Parser(std::move(lexer), program->rules_, program->token_values_));
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
  start_token_ = PeekToken();
  consumed_end_ = false;
  last_error_.reset();
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

parser_internal::ParseMatch Parser::MatchError(
    gb::Token token, TokenType expected_type, std::string_view expected_value) {
  if (suppress_errors_ > 0 ||
      (last_error_.has_value() && last_error_->token > token)) {
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

void Parser::SetError(gb::Token token, std::string_view error_message) {
  if (suppress_errors_ > 0 ||
      (last_error_.has_value() && last_error_->token > token)) {
    return;
  }
  last_error_ = ParseMatchError(token, [this, token, error_message] {
    return Error(token, error_message);
  });
}

parser_internal::ParseMatch Parser::Match(ParsedItem item) {
  return ParseMatch::Item(std::move(item));
}

parser_internal::ParseMatch Parser::Match(Token token) {
  ParsedItem parsed;
  parsed.token_ = token;
  return Match(std::move(parsed));
}

parser_internal::ParseMatch Parser::MatchTokenItem(
    const ParserToken& parser_token) {
  Token token = PeekToken();
  if (token.GetType() == kTokenError) {
    return MatchAbort(Error(token, token.GetString()));
  }

  TokenType expected_type = parser_token.GetTokenType();
  bool is_match = (token.GetType() == expected_type);
  if (is_match && !parser_token.GetTokenText().empty()) {
    auto it = token_values_->find(&parser_token);
    DCHECK(it != token_values_->end());  // Set during rule validation.
    is_match = token.HasValue(it->second);
  }
  if (!is_match) {
    return MatchError(token, expected_type, parser_token.GetTokenText());
  }
  if (token.IsEnd()) {
    if (consumed_end_) {
      SetError(token, "Nothing can follow the end of the content");
      return ParseMatch::Error();
    }
    consumed_end_ = true;
  }

  NextToken();
  return Match(token);
}

parser_internal::ParseMatch Parser::MatchRuleItem(
    const ParserRuleName& parser_rule_name) {
  const ParserRuleItem* rule = rules_->GetRule(parser_rule_name.GetRuleName());
  DCHECK(rule != nullptr);  // Handled during rule validation.
  return rule->Match(*this);
}

parser_internal::ParseMatch Parser::MatchNegation(
    const ParserNegation& negation) {
  Token token = PeekToken();
  if (token.GetType() == kTokenError) {
    return MatchAbort(Error(token, token.GetString()));
  }
  if (token.IsEnd()) {
    SetError(token, "Unexpected end of content");
    return ParseMatch::Error();
  }

  // The negated item is only tried, so it sets no errors, and whatever it
  // matched is rewound. A failed match has already consumed nothing.
  const bool consumed_end = consumed_end_;
  ++suppress_errors_;
  ParseMatch match = negation.GetItem()->Match(*this);
  --suppress_errors_;
  if (match.IsAbort()) {
    return match;
  }
  if (match.IsMatch()) {
    SetNextToken(token, consumed_end);
    SetError(token, "Unexpected token");
    return ParseMatch::Error();
  }

  NextToken();
  return Match(token);
}

parser_internal::ParseMatch Parser::MatchAdjacent(
    const ParserAdjacent& adjacent) {
  Token token = PeekToken();
  if (token.GetType() == kTokenError) {
    return MatchAbort(Error(token, token.GetString()));
  }

  // The parse consumes tokens in order, so unless nothing is consumed yet, the
  // token before this one is the last one it consumed.
  if (token != start_token_ && !lexer_->IsTokenAdjacent(token)) {
    SetError(token, "Unexpected gap before token");
    return ParseMatch::Error();
  }
  return adjacent.GetItem()->Match(*this);
}

parser_internal::ParseMatch Parser::MatchMarker() {
  Token token = PeekToken();
  if (token.GetType() == kTokenError) {
    return MatchAbort(Error(token, token.GetString()));
  }
  return Match(token);
}

void Parser::AddSubItemMatch(const ParserGroup::SubItem& sub_item,
                             ParseMatch match, ParsedItems* items) {
  if (!sub_item.name.empty()) {
    (*items)[sub_item.name].push_back(*std::move(match));
    return;
  }
  if (!sub_item.item->InlinesItems()) {
    return;
  }

  // Merging moves across the names `items` doesn't have yet, leaving only the
  // names both have, which are appended.
  ParsedItems sub_items = (*std::move(match)).items_;
  items->merge(sub_items);
  for (auto& [name, name_items] : sub_items) {
    std::vector<ParsedItem>& to_items = (*items)[name];
    to_items.insert(to_items.end(), std::make_move_iterator(name_items.begin()),
                    std::make_move_iterator(name_items.end()));
  }
}

parser_internal::ParseMatch Parser::MatchGroup(const ParserGroup& group) {
  Token group_token = PeekToken();
  const bool group_consumed_end = consumed_end_;
  if (group_token.IsError()) {
    return MatchAbort(Error(group_token, group_token.GetString()));
  }
  const bool is_sequence = (group.GetType() == ParserGroup::Type::kSequence);
  const bool is_alternatives =
      (group.GetType() == ParserGroup::Type::kAlternatives);

  ParsedItem result;
  bool has_match = false;
  for (const auto& sub_item : group.GetSubItems()) {
    Token item_token = PeekToken();
    auto match = sub_item.item->Match(*this);
    if (match.IsError()) {
      if (!match.IsAbort() && !sub_item.error.empty()) {
        SetError(item_token, sub_item.error);
      }
      if (match.IsAbort() ||
          (is_sequence && sub_item.repeat.IsSet(ParserRepeat::kRequireOne))) {
        SetNextToken(group_token, group_consumed_end);
        return match;
      }
      continue;
    }
    has_match = true;
    if (result.token_.IsNone()) {
      result.token_ = group_token;
    }
    AddSubItemMatch(sub_item, std::move(match), &result.items_);
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
      item_token = PeekToken();
      match = sub_item.item->Match(*this);
      if (match.IsError()) {
        if (!match.IsAbort() && !sub_item.error.empty()) {
          SetError(item_token, sub_item.error);
        }
        if (match.IsAbort()) {
          SetNextToken(group_token, group_consumed_end);
          return match;
        }
        if (with_comma) {
          RewindToken();
        }
        break;
      }
      if (!with_comma && PeekToken() == item_token) {
        // A match that consumed nothing would repeat the same way forever.
        break;
      }
      AddSubItemMatch(sub_item, std::move(match), &result.items_);
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
