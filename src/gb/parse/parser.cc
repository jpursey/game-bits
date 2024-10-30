// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser.h"

#include "absl/strings/str_cat.h"

namespace gb {

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
  auto match = root->Match({}, *this);
  if (!match) {
    return match.GetError();
  }
  return *std::move(match);
}

Callback<ParseError()> Parser::TokenErrorCallback(
    gb::Token token, TokenType expected_type, std::string_view expected_value) {
  return [this, token, expected_type, expected_value] {
    std::string expected;
    switch (expected_type) {
      case kTokenSymbol:
        expected = expected_value.empty()
                       ? "symbol"
                       : absl::StrCat("'", expected_value, "'");
        break;
      case kTokenInt:
        expected = expected_value.empty()
                       ? "integer value"
                       : absl::StrCat("number ", expected_value);
        break;
      case kTokenFloat:
        expected = expected_value.empty()
                       ? "floating-point value"
                       : absl::StrCat("number ", expected_value);
        break;
      case kTokenChar: {
        std::string_view quote =
            lexer_.GetFlags().IsSet(LexerFlag::kSingleQuoteCharacter) ? "'"
                                                                      : "\"";
        expected = expected_value.empty() ? "character value"
                                          : absl::StrCat("character ", quote,
                                                         expected_value, quote);
      } break;
      case kTokenString: {
        std::string_view quote =
            lexer_.GetFlags().IsSet(LexerFlag::kDoubleQuoteString) ? "\"" : "'";
        expected = expected_value.empty()
                       ? "string value"
                       : absl::StrCat("string ", quote, expected_value, quote);
      } break;
      case kTokenKeyword:
        expected = expected_value.empty() ? "keyword" : expected_value;
        break;
      case kTokenIdentifier:
        expected = expected_value.empty()
                       ? "identifier"
                       : absl::StrCat("identifier ", expected_value);
        break;
      case kTokenLineBreak:
        expected = "end of line";
        break;
    }
    return Error(token, absl::StrCat("Expected ", expected));
  };
}

ParseMatch Parser::MatchTokenItem(ParserInternal,
                                  const ParserToken& parser_token) {
  Token token = NextToken();
  if (token.GetType() == kTokenError) {
    return ParseMatch::Abort(Error(token, token.GetString()));
  }

  TokenType expected_type = parser_token.GetTokenType();
  std::string_view expected_value = parser_token.GetValue();
  if (token.GetType() != expected_type ||
      (!expected_value.empty() && token.ToString() != expected_value)) {
    SetNextToken(token);
    return TokenErrorCallback(token, expected_type, expected_value);
  }

  ParsedItem parsed;
  parsed.SetToken(token);
  return std::move(parsed);
}

ParseMatch Parser::MatchRuleItem(ParserInternal,
                                 const ParserRuleName& parser_rule_name) {
  const ParserRuleItem* rule = rules_.GetRule(parser_rule_name.GetRuleName());
  if (rule == nullptr) {
    return ParseMatch::Abort(absl::StrCat(
        "Parser rule \"", parser_rule_name.GetRuleName(), "\" not found"));
  }
  return rule->Match({}, *this);
}

ParseMatch Parser::MatchSequence(ParserInternal, const ParserGroup& group) {
  Token group_token = PeekToken();
  if (group_token.IsError()) {
    return ParseMatch::Abort(Error(group_token, group_token.GetString()));
  }
  ParsedItem result;
  Token token = group_token;
  for (const auto& sub_item : group.GetSubItems()) {
    auto match = sub_item.item->Match({}, *this);
    if (!match) {
      if (match.IsAbort() || sub_item.repeat.IsSet(ParserRepeat::kRequireOne)) {
        SetNextToken(group_token);
        return match;
      }
      continue;
    }
    Token after_match_token = PeekToken();
    if (after_match_token == token) {
      continue;
    }
    result.SetToken(group_token);
    token = after_match_token;
    if (!sub_item.name.empty()) {
      result.AddItem(sub_item.name, *std::move(match));
    }
    if (!sub_item.repeat.IsSet(ParserRepeat::kAllowMany)) {
      continue;
    }
    const bool with_comma = sub_item.repeat.IsSet(ParserRepeat::kWithComma);
    while (true) {
      if (with_comma) {
        if (token.IsSymbol(',')) {
          token = NextToken();
        } else {
          break;
        }
      }
      match = sub_item.item->Match({}, *this);
      if (!match) {
        if (match.IsAbort() || with_comma) {
          SetNextToken(group_token);
          return ParseMatch::Abort(match.GetError());
        }
        break;
      }
      after_match_token = PeekToken();
      if (after_match_token == token) {
        break;
      }
      token = after_match_token;
      if (!sub_item.name.empty()) {
        result.AddItem(sub_item.name, *std::move(match));
      }
    }
  }
  return std::move(result);
}
ParseMatch Parser::MatchAlternatives(ParserInternal, const ParserGroup& group) {
  Token group_token = PeekToken();
  if (group_token.IsError()) {
    return ParseMatch::Abort(Error(group_token, group_token.GetString()));
  }
  ParsedItem result;
  Token token = group_token;
  bool is_success = false;
  std::optional<ParseMatch> first_error;
  for (const auto& sub_item : group.GetSubItems()) {
    auto match = sub_item.item->Match({}, *this);
    if (!match) {
      if (match.IsAbort()) {
        return match;
      }
      if (sub_item.repeat.IsSet(ParserRepeat::kRequireOne) &&
          !first_error.has_value()) {
        first_error = std::move(match);
      }
      continue;
    }
    is_success = true;
    Token after_match_token = PeekToken();
    if (after_match_token == token) {
      continue;
    }
    result.SetToken(group_token);
    token = after_match_token;

    if (!sub_item.name.empty()) {
      result.AddItem(sub_item.name, *std::move(match));
    }
    if (!sub_item.repeat.IsSet(ParserRepeat::kAllowMany)) {
      return std::move(result);
    }
    const bool with_comma = sub_item.repeat.IsSet(ParserRepeat::kWithComma);
    while (true) {
      if (with_comma) {
        if (token.IsSymbol(',')) {
          token = NextToken();
        } else {
          is_success = true;
          break;
        }
      }
      match = sub_item.item->Match({}, *this);
      if (!match) {
        if (match.IsAbort() || with_comma) {
          SetNextToken(group_token);
          return ParseMatch::Abort(match.GetError());
        }
        break;
      }
      after_match_token = PeekToken();
      if (after_match_token == token) {
        break;
      }
      token = after_match_token;
      if (!sub_item.name.empty()) {
        result.AddItem(sub_item.name, *std::move(match));
      }
    }
    return std::move(result);
  }
  if (is_success || !first_error.has_value()) {
    return std::move(result);
  }
  return *std::move(first_error);
}

}  // namespace gb
