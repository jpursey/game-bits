// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_program.h"

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "gb/parse/parser.h"

namespace gb {
namespace {

constexpr TokenType kTokenTokenType = kTokenUser + 0;
constexpr TokenType kTokenMatchName = kTokenUser + 1;

constexpr LexerConfig::UserToken kProgramUserTokens[] = {
    {.name = "token type",
     .type = kTokenTokenType,
     .regex = R"-(\%([a-zA-Z]\w*))-"},
    {.name = "match name",
     .type = kTokenMatchName,
     .regex = R"-(\$([a-zA-Z]\w*))-"},
};

inline constexpr Symbol kProgramSymbols[] = {
    '+', '-', '*', '/', '%', '~', '&', '|', '^', '!', '<', '>', '=',  '.', ',',
    ';', ':', '?', '$', '#', '@', '(', ')', '[', ']', '{', '}', ",*", ",+"};

constexpr LexerConfig kProgramLexerConfig = {
    .flags = {kLexerFlags_CIdentifiers, LexerFlag::kInt8,
              LexerFlag::kDecimalIntegers, LexerFlag::kDoubleQuoteString,
              LexerFlag::kSingleQuoteString, LexerFlag::kDecodeEscape},
    .escape = '\\',
    .escape_newline = 'n',
    .escape_tab = 't',
    .escape_hex = 'x',
    .line_comments = kCStyleLineComments,
    .symbols = kProgramSymbols,
    .user_tokens = kProgramUserTokens,
};

ParserRules CreateProgramRules() {
  ParserRules rules;

  // token %token_type = 0;
  // token %match_name = 1;

  // rule program {
  //   ($tokens=token_def | $rules=rule_def)*;
  // }
  auto program = ParserRuleItem::CreateSequence();
  auto program_alternatives = ParserRuleItem::CreateAlternatives();
  program_alternatives->AddSubItem("tokens",
                                   ParserRuleItem::CreateRuleName("token_def"));
  program_alternatives->AddSubItem("rules",
                                   ParserRuleItem::CreateRuleName("rule_def"));
  program->AddSubItem(std::move(program_alternatives), kParserZeroOrMore);
  rules.AddRule("program", std::move(program));

  // rule token_def {
  //   $name=%token_type "=" value=%int ";";
  // }
  auto token_def = ParserRuleItem::CreateSequence();
  token_def->AddSubItem("name", ParserRuleItem::CreateToken(kTokenTokenType));
  token_def->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "="));
  token_def->AddSubItem("value", ParserRuleItem::CreateToken(kTokenInt));
  token_def->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ";"));
  rules.AddRule("token_def", std::move(token_def));

  // rule rule_def {
  //   $name=%ident "{" ($options=group_alternative ";")+ "}";
  // }
  auto rule_def = ParserRuleItem::CreateSequence();
  rule_def->AddSubItem("name", ParserRuleItem::CreateToken(kTokenIdentifier));
  rule_def->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "{"));
  auto rule_def_sequence = ParserRuleItem::CreateSequence();
  rule_def_sequence->AddSubItem(
      "options", ParserRuleItem::CreateRuleName("group_alternative"));
  rule_def_sequence->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, ";"));
  rule_def->AddSubItem(std::move(rule_def_sequence), kParserOneOrMore);
  rule_def->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "}"));
  rules.AddRule("rule_def", std::move(rule_def));

  // rule group_alternative {
  //   $items=group_sequence ("|" $items=group_sequence)*;
  // }
  auto group_alternative = ParserRuleItem::CreateSequence();
  group_alternative->AddSubItem(
      "items", ParserRuleItem::CreateRuleName("group_sequence"));
  auto group_alternative_sequence = ParserRuleItem::CreateSequence();
  group_alternative_sequence->AddSubItem(
      ParserRuleItem::CreateToken(kTokenSymbol, "|"));
  group_alternative_sequence->AddSubItem(
      "items", ParserRuleItem::CreateRuleName("group_sequence"));
  group_alternative->AddSubItem(std::move(group_alternative_sequence),
                                kParserZeroOrMore);
  rules.AddRule("group_alternative", std::move(group_alternative));

  // rule group_sequence {
  //   $items=group_item+;
  // }
  auto group_sequence = ParserRuleItem::CreateSequence();
  group_sequence->AddSubItem(
      "items", ParserRuleItem::CreateRuleName("group_item"), kParserOneOrMore);
  rules.AddRule("group_sequence", std::move(group_sequence));

  // rule group_item {
  //   [$match_name=%match_name "="]
  //   $item=group_item_inner
  //   $repeat=["+" | "*" | ",+" | ",*"];
  // }
  auto group_item = ParserRuleItem::CreateSequence();
  auto group_item_match = ParserRuleItem::CreateSequence();
  group_item_match->AddSubItem("match_name",
                               ParserRuleItem::CreateToken(kTokenMatchName));
  group_item_match->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "="));
  group_item->AddSubItem(std::move(group_item_match), kParserOptional);
  group_item->AddSubItem("item",
                         ParserRuleItem::CreateRuleName("group_item_inner"));
  auto group_item_repeat = ParserRuleItem::CreateAlternatives();
  group_item_repeat->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "+"));
  group_item_repeat->AddSubItem(ParserRuleItem::CreateToken(kTokenSymbol, "*"));
  group_item_repeat->AddSubItem(
      ParserRuleItem::CreateToken(kTokenSymbol, ",+"));
  group_item_repeat->AddSubItem(
      ParserRuleItem::CreateToken(kTokenSymbol, ",*"));
  group_item->AddSubItem("repeat", std::move(group_item_repeat),
                         kParserOptional);
  rules.AddRule("group_item", std::move(group_item));

  // rule group_item_inner {
  //   $token=%token_type;
  //   $literal=%string;
  //   $rule=%ident;
  //   "[" $optional=group_alternative "]";
  //   "(" $group=group_alternative ")";
  // }
  auto group_item_inner = ParserRuleItem::CreateAlternatives();
  group_item_inner->AddSubItem("token",
                               ParserRuleItem::CreateToken(kTokenTokenType));
  group_item_inner->AddSubItem("literal",
                               ParserRuleItem::CreateToken(kTokenString));
  group_item_inner->AddSubItem("rule",
                               ParserRuleItem::CreateToken(kTokenIdentifier));
  auto group_item_inner_optional = ParserRuleItem::CreateSequence();
  group_item_inner_optional->AddSubItem(
      ParserRuleItem::CreateToken(kTokenSymbol, "["));
  group_item_inner_optional->AddSubItem(
      "optional", ParserRuleItem::CreateRuleName("group_alternative"));
  group_item_inner_optional->AddSubItem(
      ParserRuleItem::CreateToken(kTokenSymbol, "]"));
  group_item_inner->AddSubItem(std::move(group_item_inner_optional));
  auto group_item_inner_group = ParserRuleItem::CreateSequence();
  group_item_inner_group->AddSubItem(
      ParserRuleItem::CreateToken(kTokenSymbol, "("));
  group_item_inner_group->AddSubItem(
      "group", ParserRuleItem::CreateRuleName("group_alternative"));
  group_item_inner_group->AddSubItem(
      ParserRuleItem::CreateToken(kTokenSymbol, ")"));
  group_item_inner->AddSubItem(std::move(group_item_inner_group));
  rules.AddRule("group_item_inner", std::move(group_item_inner));

  return rules;
}

struct ParseContext {
  Lexer& lexer;
  std::string& error;
  absl::flat_hash_map<std::string_view, TokenType> token_types;
};

std::unique_ptr<ParserGroup::SubItem> ParseGroupItem(
    ParseContext& context, const ParsedItem& parsed_item);

std::unique_ptr<ParserGroup> ParseSequence(ParseContext& context,
                                           const ParsedItem& parsed_sequence) {
  auto sequence = ParserGroup::CreateSequence();
  auto parsed_items = parsed_sequence.GetItems("items");
  for (const ParsedItem& parsed_item : parsed_items) {
    auto sub_item = ParseGroupItem(context, parsed_item);
    if (sub_item == nullptr) {
      return nullptr;
    }
    sequence->AddSubItem(std::move(*sub_item));
  }
  return sequence;
}

std::unique_ptr<ParserGroup::SubItem> ParseSequenceAsSubItem(
    ParseContext& context, const ParsedItem& parsed_sequence) {
  auto parsed_items = parsed_sequence.GetItems("items");
  if (parsed_items.size() == 1) {
    return ParseGroupItem(context, parsed_items[0]);
  }
  auto sequence = ParseSequence(context, parsed_sequence);
  if (sequence == nullptr) {
    return nullptr;
  }
  return std::make_unique<ParserGroup::SubItem>("", std::move(sequence),
                                                kParserSingle);
}

std::unique_ptr<ParserGroup> ParseAlternative(
    ParseContext& context, const ParsedItem& parsed_alternative) {
  auto parsed_items = parsed_alternative.GetItems("items");
  if (parsed_items.size() == 1) {
    return ParseSequence(context, parsed_items[0]);
  }
  auto alternative = ParserGroup::CreateAlternatives();
  for (const ParsedItem& parsed_item : parsed_items) {
    auto sub_item = ParseSequenceAsSubItem(context, parsed_item);
    if (sub_item == nullptr) {
      return nullptr;
    }
    alternative->AddSubItem(std::move(*sub_item));
  }
  return alternative;
}

std::unique_ptr<ParserGroup::SubItem> ParseGroupItem(
    ParseContext& context, const ParsedItem& parsed_item) {
  std::string_view match_name = parsed_item.GetString("match_name");

  const Symbol parsed_repeat = parsed_item.GetSymbol("repeat");
  ParserRepeatFlags repeat;
  if (parsed_repeat == '+') {
    repeat = kParserOneOrMore;
  } else if (parsed_repeat == '*') {
    repeat = kParserZeroOrMore;
  } else if (parsed_repeat == ",+") {
    repeat = kParserOneOrMoreWithComma;
  } else if (parsed_repeat == ",*") {
    repeat = kParserZeroOrMoreWithComma;
  } else {
    repeat = kParserSingle;
  }

  const ParsedItem* parsed_inner = parsed_item.GetItem("item");
  DCHECK(parsed_inner != nullptr);
  std::string_view parsed_type = parsed_inner->GetMatchName();
  if (parsed_type == "token") {
    std::string_view type_name = parsed_inner->GetString("token");
    auto it = context.token_types.find(type_name);
    if (it == context.token_types.end()) {
      context.error = absl::StrCat("Unknown token type: ", type_name);
      return nullptr;
    }
    return std::make_unique<ParserGroup::SubItem>(
        match_name, ParserRuleItem::CreateToken(it->second), repeat);
  } else if (parsed_type == "literal") {
    std::string_view literal = parsed_inner->GetString("literal");
    Token token = context.lexer.ParseTokenText(literal);
    if (token.GetType() == kTokenError) {
      context.error = absl::StrCat("Invalid token literal: ", literal);
      return nullptr;
    }
    return std::make_unique<ParserGroup::SubItem>(
        match_name, ParserRuleItem::CreateToken(token.GetType(), literal),
        repeat);
  } else if (parsed_type == "rule") {
    return std::make_unique<ParserGroup::SubItem>(
        match_name,
        ParserRuleItem::CreateRuleName(parsed_inner->GetString("rule")),
        repeat);
  } else if (parsed_type == "optional") {
    auto optional_item = ParseAlternative(context, *parsed_inner);
    if (optional_item == nullptr) {
      return nullptr;
    }
    repeat.Clear(ParserRepeat::kRequireOne);
    return std::make_unique<ParserGroup::SubItem>(
        match_name, std::move(optional_item), repeat);
  } else if (parsed_type == "group") {
    auto group_item = ParseAlternative(context, *parsed_inner);
    if (group_item == nullptr) {
      return nullptr;
    }
    return std::make_unique<ParserGroup::SubItem>(
        match_name, std::move(group_item), repeat);
  }
  context.error = absl::StrCat("Internal error, unhandled parse type: \"",
                               parsed_type, "\"");
  return nullptr;
}

std::optional<ParserRules> ParseProgram(Lexer& lexer, std::string program_text,
                                        std::string* error_message) {
  std::string error_storage;
  ParseContext context = {
      .lexer = lexer,
      .error = (error_message != nullptr ? *error_message : error_storage)};

  auto program_parser =
      Parser::Create(kProgramLexerConfig, CreateProgramRules(), error_message);
  if (program_parser == nullptr) {
    return std::nullopt;
  }
  auto parsed = program_parser->Parse(
      program_parser->GetLexer().AddContent(std::move(program_text)),
      "program");
  if (!parsed.IsOk()) {
    context.error = parsed.GetError().FormatMessage();
    return std::nullopt;
  }

  auto parsed_tokens = parsed->GetItems("tokens");
  for (const ParsedItem& parsed_token : parsed_tokens) {
    TokenType token_type = kTokenUser + parsed_token.GetInt("value");
    std::string_view token_name = parsed_token.GetString("name");
    if (!context.token_types.insert({token_name, token_type}).second) {
      context.error = absl::StrCat("Duplicate token type: ", token_name);
      return std::nullopt;
    }
    if (!lexer.IsValidTokenType(token_type)) {
      context.error = absl::StrCat("Undefined token type value ",
                                   static_cast<int>(token_type),
                                   " for token name ", token_name);
      return std::nullopt;
    }
  }

  auto parsed_rules = parsed->GetItems("rules");
  if (parsed_rules.empty()) {
    context.error = "No rules found in program";
    return std::nullopt;
  }

  ParserRules rules;
  for (const ParsedItem& parsed_rule : parsed_rules) {
    std::string_view rule_name = parsed_rule.GetString("name");
    auto parsed_options = parsed_rule.GetItems("options");
    if (parsed_options.size() == 1) {
      auto group = ParseSequence(context, parsed_options[0]);
      if (group == nullptr) {
        return std::nullopt;
      }
      rules.AddRule(std::string(rule_name), std::move(group));
    } else {
      auto rule = ParserGroup::CreateAlternatives();
      for (const ParsedItem& parsed_option : parsed_options) {
        auto option = ParseAlternative(context, parsed_option);
        if (option == nullptr) {
          return std::nullopt;
        }
        rule->AddSubItem(std::move(option));
      }
      rules.AddRule(std::string(rule_name), std::move(rule));
    }
  }

  if (!rules.Validate(lexer, &context.error)) {
    return std::nullopt;
  }
  return rules;
}

}  // namespace

std::unique_ptr<ParserProgram> ParserProgram::Create(
    LexerConfig config, std::string program_text, std::string* error_message) {
  auto lexer = Lexer::Create(config, error_message);
  if (lexer == nullptr) {
    return nullptr;
  }
  return Create(std::move(lexer), std::move(program_text), error_message);
}

std::unique_ptr<ParserProgram> ParserProgram::Create(
    std::unique_ptr<Lexer> lexer, std::string program_text,
    std::string* error_message) {
  if (lexer == nullptr) {
    if (error_message != nullptr) {
      *error_message = "Lexer is null";
    }
    return nullptr;
  }
  auto rules = ParseProgram(*lexer, std::move(program_text), error_message);
  if (!rules.has_value()) {
    return nullptr;
  }
  return absl::WrapUnique(
      new ParserProgram(std::move(lexer), *std::move(rules)));
}

std::unique_ptr<ParserProgram> ParserProgram::Create(
    Lexer* lexer, std::string program_text, std::string* error_message) {
  auto rules = ParseProgram(*lexer, std::move(program_text), error_message);
  if (!rules.has_value()) {
    return nullptr;
  }
  return absl::WrapUnique(new ParserProgram(lexer, *std::move(rules)));
}

}  // namespace gb
