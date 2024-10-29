// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser.h"

#include "absl/strings/str_cat.h"

namespace gb {

std::string ParseError::FormatMessage() const {
  if (location_.line < 0) {
    return message_;
  }
  return absl::StrCat(location_.filename, "(", location_.line, "): ", message_);
}

ParseError Parser::Error(Token token, std::string_view message) {
  return ParseError(lexer_.GetTokenLocation(token),
                    absl::StrCat("Parse error at \"",
                                 lexer_.GetTokenText(token), "\": ", message));
}

ParseResult Parser::Parse(LexerContentId content, std::string_view rule) {
  const ParserItem* root = rules_.GetRule(rule);
  if (root == nullptr) {
    return ParseError(absl::StrCat("Parser rule \"", rule, "\" not found"));
  }
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
        expected = absl::StrCat("'", expected_value, "'");
        break;
      case kTokenInt:
        expected = expected_value.empty()
                       ? std::string("integer value")
                       : absl::StrCat("number ", expected_value);
        break;
      case kTokenFloat:
        expected = expected_value.empty()
                       ? std::string("floating-point value")
                       : absl::StrCat("number ", expected_value);
        break;
      case kTokenChar: {
        std::string_view quote =
            lexer_.GetFlags().IsSet(LexerFlag::kSingleQuoteCharacter) ? "'"
                                                                      : "\"";
        expected = expected_value.empty() ? std::string("character value")
                                          : absl::StrCat("character ", quote,
                                                         expected_value, quote);
      } break;
      case kTokenString: {
        std::string_view quote =
            lexer_.GetFlags().IsSet(LexerFlag::kDoubleQuoteString) ? "\"" : "'";
        expected = expected_value.empty()
                       ? std::string("string value")
                       : absl::StrCat("string ", quote, expected_value, quote);
      } break;
      case kTokenKeyword:
        expected = std::string_view(expected_value);
        break;
      case kTokenIdentifier:
        expected = expected_value.empty()
                       ? std::string("identifier")
                       : absl::StrCat("identifier ", expected_value);
        break;
      case kTokenLineBreak:
        expected = "end of line";
        break;
    }
    return Error(token, absl::StrCat("Expected ", expected));
  };
}

ParseMatch ParserTokenItem::Match(ParserInternal internal,
                                  Parser& parser) const {
  return parser.MatchTokenItem(internal, *this);
}
ParseMatch Parser::MatchTokenItem(ParserInternal, const ParserTokenItem& item) {
  Token token = NextToken();

  TokenType expected_type = item.GetTokenType();
  std::string expected_value = token.ToString();
  if (token.GetType() != expected_type ||
      (!expected_value.empty() && token.ToString() != expected_value)) {
    SetNextToken(token);
    return TokenErrorCallback(token, expected_type, expected_value);
  }

  ParsedItem parsed;
  parsed.SetToken(token);
  return std::move(parsed);
}

ParseMatch ParserGroupItem::Match(ParserInternal internal,
                                  Parser& parser) const {
  switch (GetGroupType()) {
    case GroupType::kSequence:
      return parser.MatchSequence(internal, *this);
    case GroupType::kAlternatives:
      return parser.MatchAlternatives(internal, *this);
  }
  return ParseError("Unimplemented group type");
}
ParseMatch Parser::MatchSequence(ParserInternal, const ParserGroupItem& item) {
  Token group_token = PeekToken();
  ParsedItem result;
  result.SetToken(group_token);
  Token token = group_token;
  for (const auto& sub_item : item.GetSubItems()) {
    auto match = sub_item.item->Match({}, *this);
    if (!match) {
      if (sub_item.repeat.IsSet(ParserRepeat::kRequireOne)) {
        SetNextToken(group_token);
        return match;
      }
      continue;
    }
    Token after_match_token = PeekToken();
    if (after_match_token == token) {
      continue;
    }
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
        if (!token.IsSymbol(',')) {
          token = NextToken();
          break;
        }
      }
      match = sub_item.item->Match({}, *this);
      if (!match) {
        if (with_comma) {
          SetNextToken(group_token);
          return match;
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
ParseMatch Parser::MatchAlternatives(ParserInternal,
                                     const ParserGroupItem& item) {
  Token group_token = PeekToken();
  ParsedItem result;
  result.SetToken(group_token);
  Token token = group_token;
  bool is_success = false;
  std::optional<ParseMatch> first_error;
  for (const auto& sub_item : item.GetSubItems()) {
    auto match = sub_item.item->Match({}, *this);
    if (!match) {
      if (sub_item.repeat.IsSet(ParserRepeat::kRequireOne) && !first_error.has_value()) {
        first_error = std::move(match);
      }
      continue;
    }
    Token after_match_token = PeekToken();
    if (after_match_token == token) {
      is_success = true;
      continue;
    }
    token = after_match_token;

    std::vector<ParsedItem> named_items;
    if (!sub_item.name.empty()) {
      named_items.emplace_back(*std::move(match));
    }
    if (!sub_item.repeat.IsSet(ParserRepeat::kAllowMany)) {
      is_success = true;
    } else {
      const bool with_comma = sub_item.repeat.IsSet(ParserRepeat::kWithComma);
      while (true) {
        if (with_comma) {
          if (!token.IsSymbol(',')) {
            is_success = true;
            token = NextToken();
            break;
          }
        }
        match = sub_item.item->Match({}, *this);
        if (!match) {
          if (with_comma && !first_error.has_value()) {
            first_error = std::move(match);
          }
          break;
        }
        after_match_token = PeekToken();
        if (after_match_token == token) {
          is_success = true;
          break;
        }
        token = after_match_token;
        if (!sub_item.name.empty()) {
          named_items.emplace_back(*std::move(match));
        }
      }
    }
    if (is_success) {
      for (auto& named_item : named_items) {
        result.AddItem(sub_item.name, std::move(named_item));
      }
      break;
    }
    token = group_token;
    SetNextToken(token);
  }
  if (is_success || !first_error.has_value()) {
    return std::move(result);
  }
  return *std::move(first_error);
}

ParseMatch ParserRuleItem::Match(ParserInternal internal,
                                 Parser& parser) const {
  return parser.MatchRuleItem(internal, *this);
}
ParseMatch Parser::MatchRuleItem(ParserInternal, const ParserRuleItem& item) {
  const ParserItem* rule = rules_.GetRule(item.GetRuleName());
  if (rule == nullptr) {
    return ParseError(
        absl::StrCat("Parser rule \"", item.GetRuleName(), "\" not found"));
  }
  return rule->Match({}, *this);
}

}  // namespace gb
