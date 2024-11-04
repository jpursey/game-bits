// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser.h"

#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_cat.h"

namespace gb {

std::unique_ptr<Parser> Parser::Create(LexerConfig config, ParserRules rules,
                                       std::string* error_message) {
  auto lexer = Lexer::Create(config, error_message);
  if (lexer == nullptr) {
    return nullptr;
  }
  return Create(std::move(lexer), std::move(rules), error_message);
}

std::unique_ptr<Parser> Parser::Create(std::unique_ptr<Lexer> lexer,
                                       ParserRules rules,
                                       std::string* error_message) {
  if (lexer == nullptr) {
    if (error_message != nullptr) {
      *error_message = "Lexer is null";
    }
    return nullptr;
  }
  if (!rules.Validate(*lexer, error_message)) {
    return nullptr;
  }
  auto parser = absl::WrapUnique(new Parser(*lexer, std::move(rules)));
  parser->owned_lexer_ = std::move(lexer);
  return parser;
}

std::unique_ptr<Parser> Parser::Create(Lexer& lexer, ParserRules rules,
                                       std::string* error_message) {
  if (!rules.Validate(lexer, error_message)) {
    return nullptr;
  }
  return absl::WrapUnique(new Parser(lexer, std::move(rules)));
}

ParseError Parser::Error(Token token, std::string_view message) {
  LexerLocation location = lexer_.GetTokenLocation(token);
  if (token.GetType() == kTokenError) {
    message = token.GetString();
    if (location.line < 0) {
      return ParseError(absl::StrCat("Parse error: ", message));
    }
  }
  return ParseError(
      location, absl::StrCat("Parse error at \"", lexer_.GetTokenText(token),
                             "\": ", message));
}

ParseResult Parser::Parse(LexerContentId content, std::string_view rule) {
  const ParserRuleItem* root = rules_.GetRule(rule);
  if (root == nullptr) {
    return ParseError(absl::StrCat("Parser rule \"", rule, "\" not found"));
  }
  content_ = content;
  auto match = root->Match(*this);
  if (!match) {
    return match.GetError();
  }
  return *std::move(match);
}

Callback<ParseError()> Parser::TokenErrorCallback(
    gb::Token token, TokenType expected_type, std::string_view expected_value) {
  return [this, token, expected_type, expected_value] {
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
            GetTokenTypeString(expected_type, &lexer_.GetUserTokenNames());
        expected = !has_value ? type_name
                              : absl::StrCat(type_name, " ", expected_value);
      } break;
    }
    return Error(token, absl::StrCat("Expected ", expected));
  };
}

parser_internal::ParseMatch Parser::MatchTokenItem(
    const ParserToken& parser_token) {
  Token token = PeekToken();
  if (token.GetType() == kTokenError) {
    return ParseMatch::Abort(Error(token, token.GetString()));
  }

  TokenType expected_type = parser_token.GetTokenType();
  const TokenValue& expected_value = parser_token.GetValue();
  if (token.GetType() != expected_type ||
      (!std::holds_alternative<NoTokenValue>(expected_value) &&
       token.GetValue() != expected_value)) {
    return ParseMatch(token, TokenErrorCallback(token, expected_type,
                                                parser_token.GetTokenText()));
  }

  NextToken();
  ParsedItem parsed;
  parsed.token_ = token;
  return std::move(parsed);
}

parser_internal::ParseMatch Parser::MatchRuleItem(
    const ParserRuleName& parser_rule_name) {
  const ParserRuleItem* rule = rules_.GetRule(parser_rule_name.GetRuleName());
  DCHECK(rule != nullptr);  // Handled during rule validation.
  ParsedItems* old_items = std::exchange(items_, nullptr);
  ParseMatch match = rule->Match(*this);
  items_ = old_items;
  return match;
}

parser_internal::ParseMatch Parser::MatchGroup(const ParserGroup& group) {
  Token group_token = PeekToken();
  if (group_token.IsError()) {
    return ParseMatch::Abort(Error(group_token, group_token.GetString()));
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
  Token token = group_token;
  std::optional<ParseMatch> error;
  for (const auto& sub_item : group.GetSubItems()) {
    ParsedItems* current_items = items_;
    if (!sub_item.name.empty()) {
      items_ = nullptr;
    }
    auto match = sub_item.item->Match(*this);
    items_ = current_items;
    if (!match) {
      const bool is_abort = match.IsAbort();
      if (!error.has_value() ||
          match.GetErrorToken() > error->GetErrorToken()) {
        error = std::move(match);
      }
      if (is_abort ||
          (is_sequence && sub_item.repeat.IsSet(ParserRepeat::kRequireOne))) {
        SetNextToken(group_token);
        return *std::move(error);
      }
      continue;
    }
    if (result.token_.IsNone()) {
      result.token_ = group_token;
    }
    if (!sub_item.name.empty()) {
      (*items_)[sub_item.name].push_back(*std::move(match));
    }
    if (!sub_item.repeat.IsSet(ParserRepeat::kAllowMany)) {
      if (is_alternatives) {
        error = std::nullopt;
        break;
      }
      continue;
    }
    const bool with_comma = sub_item.repeat.IsSet(ParserRepeat::kWithComma);
    while (true) {
      token = PeekToken();
      if (with_comma) {
        if (token.IsSymbol(',')) {
          token = NextToken();
        } else {
          break;
        }
      }
      match = sub_item.item->Match(*this);
      if (!match) {
        const bool is_abort = match.IsAbort();
        if (!error.has_value() ||
            match.GetErrorToken() > error->GetErrorToken()) {
          error = std::move(match);
        }
        if (is_abort || with_comma) {
          SetNextToken(group_token);
          return ParseMatch::Abort(error->GetError());
        }
        break;
      }
      if (!sub_item.name.empty()) {
        (*items_)[sub_item.name].push_back(*std::move(match));
      }
    }
    if (is_alternatives) {
      error = std::nullopt;
      break;
    }
  }
  if (is_alternatives && error.has_value()) {
    return *std::move(error);
  }
  return std::move(result);
}

}  // namespace gb
