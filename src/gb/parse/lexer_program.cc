// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer_program.h"

#include <memory>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"

namespace gb {

const std::string_view LexerProgram::kErrorDuplicateSymbolSpec =
    "Duplicate symbol specification";
const std::string_view LexerProgram::kErrorInvalidSymbolSpec =
    "Symbol specification has non-ASCII or whitespace characters";
const std::string_view LexerProgram::kErrorConflictingStringAndCharSpec =
    "Character and String specifications share the same quote type";
const std::string_view LexerProgram::kErrorConflictingIdentifierSpec =
    "Identifiers cannot be set to force both lower and upper case";
const std::string_view LexerProgram::kErrorConflictingCommentSpec =
    "Multiple line and/or block comment starts share a common prefix";
const std::string_view LexerProgram::kErrorEmptyCommentSpec =
    "Empty string used in line or block comment specification";
const std::string_view LexerProgram::kErrorDuplicateKeywordSpec =
    "Duplicate keyword specification";
const std::string_view LexerProgram::kErrorEmptyKeywordSpec =
    "Empty string used in keyword specification";
const std::string_view LexerProgram::kErrorNoTokenSpec =
    "No token specification (from symbols, keywords, or flags)";
const std::string_view LexerProgram::kErrorInvalidUserTokenType =
    "Invalid user token type (it must be >= kTokenUser)";
const std::string_view LexerProgram::kErrorInvalidUserTokenRegex =
    "Invalid user token regex";

namespace {

bool CreateTokenPattern(std::string& token_pattern, const LexerConfig& config,
                        std::string& error) {
  const LexerFlags flags = config.flags;

  if (LexerSupportsIntegers(flags)) {
    bool first = true;
    if (flags.IsSet(LexerFlag::kBinaryIntegers)) {
      DCHECK(first);
      first = false;
      absl::StrAppend(&token_pattern, "(", RE2::QuoteMeta(config.binary_prefix),
                      "[01]+", RE2::QuoteMeta(config.binary_suffix), ")");
    }
    if (flags.IsSet(LexerFlag::kOctalIntegers)) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      first = false;
      absl::StrAppend(&token_pattern, "(", RE2::QuoteMeta(config.octal_prefix),
                      "[0-7]+", RE2::QuoteMeta(config.octal_suffix), ")");
    }
    if (flags.IsSet(LexerFlag::kDecimalIntegers)) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      first = false;
      absl::StrAppend(&token_pattern, "(",
                      RE2::QuoteMeta(config.decimal_prefix));
      if (flags.IsSet(LexerFlag::kNegativeIntegers)) {
        absl::StrAppend(&token_pattern, "-?");
      }
      absl::StrAppend(&token_pattern, "[0-9]+",
                      RE2::QuoteMeta(config.decimal_suffix), ")");
    }
    if (flags.Intersects(
            {LexerFlag::kHexUpperIntegers, LexerFlag::kHexLowerIntegers})) {
      if (!first) {
        absl::StrAppend(&token_pattern, "|");
      }
      absl::StrAppend(&token_pattern, "(", RE2::QuoteMeta(config.hex_prefix),
                      "[0-9");
      if (flags.IsSet(LexerFlag::kHexUpperIntegers)) {
        absl::StrAppend(&token_pattern, "A-F");
      }
      if (flags.IsSet(LexerFlag::kHexLowerIntegers)) {
        absl::StrAppend(&token_pattern, "a-f");
      }
      absl::StrAppend(&token_pattern, "]+", RE2::QuoteMeta(config.hex_suffix),
                      ")");
    }
    absl::StrAppend(&token_pattern, "|");
  }

  if (LexerSupportsFloats(flags)) {
    absl::StrAppend(&token_pattern, "(", RE2::QuoteMeta(config.float_prefix));
    if (flags.IsSet(LexerFlag::kNegativeFloats)) {
      absl::StrAppend(&token_pattern, "-?");
    }
    absl::StrAppend(&token_pattern, "[0-9]+(?:\\.[0-9]+)?");
    if (flags.IsSet(LexerFlag::kExponentFloats)) {
      absl::StrAppend(&token_pattern, "(?:[eE][-+]?[0-9]+)");
      if (flags.IsSet(LexerFlag::kDecimalFloats)) {
        absl::StrAppend(&token_pattern, "?");
      }
    }
    absl::StrAppend(&token_pattern, RE2::QuoteMeta(config.float_suffix), ")|");
  }

  if (flags.IsSet(
          {LexerFlag::kDoubleQuoteCharacter, LexerFlag::kDoubleQuoteString}) ||
      flags.IsSet(
          {LexerFlag::kSingleQuoteCharacter, LexerFlag::kSingleQuoteString})) {
    error = LexerProgram::kErrorConflictingStringAndCharSpec;
    return false;
  }

  std::string escape_char;
  std::string escape_hex;
  if (flags.IsSet(LexerFlag::kEscapeCharacter) && config.escape != 0) {
    escape_char = RE2::QuoteMeta(std::string_view(&config.escape, 1));
    if (config.escape_hex != 0) {
      escape_hex = absl::StrCat(
          escape_char, RE2::QuoteMeta(std::string_view(&config.escape_hex, 1)),
          "[0-9a-fA-F]{2}");
    }
  }

  if (LexerSupportsCharacters(flags)) {
    absl::StrAppend(&token_pattern, "(");
    auto quote_re = [&](std::string_view quote) {
      absl::StrAppend(&token_pattern, "(?:", quote, "(?:");
      absl::StrAppend(&token_pattern, "[^", quote, escape_char, "]");
      if (flags.IsSet(LexerFlag::kQuoteQuoteEscape)) {
        absl::StrAppend(&token_pattern, "|", quote, quote);
      }
      if (flags.IsSet(LexerFlag::kEscapeCharacter) && config.escape != 0) {
        if (config.escape_hex) {
          absl::StrAppend(&token_pattern, "|", escape_hex);
        }
        absl::StrAppend(&token_pattern, "|", escape_char, ".");
      }
      absl::StrAppend(&token_pattern, ")", quote, ")");
    };
    if (flags.IsSet(LexerFlag::kDoubleQuoteCharacter)) {
      quote_re("\"");
    }
    if (flags.IsSet(LexerFlag::kSingleQuoteCharacter)) {
      if (flags.IsSet(LexerFlag::kDoubleQuoteCharacter)) {
        absl::StrAppend(&token_pattern, "|");
      }
      quote_re("'");
    }
    absl::StrAppend(&token_pattern, ")|");
  }

  if (LexerSupportsStrings(flags)) {
    absl::StrAppend(&token_pattern, "(");
    auto quote_re = [&](std::string_view quote) {
      absl::StrAppend(&token_pattern, "(?:", quote, "(?:");
      absl::StrAppend(&token_pattern, "[^", quote, escape_char, "]");
      if (flags.IsSet(LexerFlag::kQuoteQuoteEscape)) {
        absl::StrAppend(&token_pattern, "|", quote, quote);
      }
      if (flags.IsSet(LexerFlag::kEscapeCharacter) && config.escape != 0) {
        if (config.escape_hex) {
          absl::StrAppend(&token_pattern, "|", escape_hex);
        }
        absl::StrAppend(&token_pattern, "|", escape_char, ".");
      }
      absl::StrAppend(&token_pattern, ")*", quote, ")");
    };
    if (flags.IsSet(LexerFlag::kDoubleQuoteString)) {
      quote_re("\"");
    }
    if (flags.IsSet(LexerFlag::kSingleQuoteString)) {
      if (flags.IsSet(LexerFlag::kDoubleQuoteString)) {
        absl::StrAppend(&token_pattern, "|");
      }
      quote_re("'");
    }
    absl::StrAppend(&token_pattern, ")|");
  }

  if (!config.keywords.empty()) {
    absl::flat_hash_set<std::string_view> keywords;
    absl::StrAppend(&token_pattern, "(");
    if (flags.IsSet(LexerFlag::kKeywordCaseInsensitive)) {
      absl::StrAppend(&token_pattern, "(?i)");
    }
    for (const std::string_view& keyword : config.keywords) {
      if (keyword.empty()) {
        error = LexerProgram::kErrorEmptyKeywordSpec;
        return false;
      }
      if (!keywords.insert(keyword).second) {
        error = LexerProgram::kErrorDuplicateKeywordSpec;
        return false;
      }
      absl::StrAppend(&token_pattern, RE2::QuoteMeta(keyword), "|");
    }
    token_pattern.pop_back();
    absl::StrAppend(&token_pattern, ")|");
  }

  if (LexerSupportsIdentifiers(flags)) {
    if (flags.IsSet(
            {LexerFlag::kIdentForceUpper, LexerFlag::kIdentForceLower})) {
      error = LexerProgram::kErrorConflictingIdentifierSpec;
      return false;
    }
    absl::StrAppend(&token_pattern, "(", RE2::QuoteMeta(config.ident_prefix),
                    "[");
    if (flags.IsSet(LexerFlag::kIdentUnderscore) &&
        !flags.IsSet(LexerFlag::kIdentNonLeadUnderscore)) {
      absl::StrAppend(&token_pattern, "_");
    }
    if (flags.IsSet(LexerFlag::kIdentDigit) &&
        !flags.IsSet(LexerFlag::kIdentNonLeadDigit)) {
      absl::StrAppend(&token_pattern, "0-9");
    }
    if (flags.Intersects({LexerFlag::kIdentUpper, LexerFlag::kIdentForceLower,
                          LexerFlag::kIdentForceUpper})) {
      absl::StrAppend(&token_pattern, "A-Z");
    }
    if (flags.Intersects({LexerFlag::kIdentLower, LexerFlag::kIdentForceLower,
                          LexerFlag::kIdentForceUpper})) {
      absl::StrAppend(&token_pattern, "a-z");
    }
    absl::StrAppend(&token_pattern, "][");
    if (flags.Intersects({LexerFlag::kIdentUnderscore,
                          LexerFlag::kIdentNonLeadUnderscore})) {
      absl::StrAppend(&token_pattern, "_");
    }
    if (flags.Intersects(
            {LexerFlag::kIdentDigit, LexerFlag::kIdentNonLeadDigit})) {
      absl::StrAppend(&token_pattern, "0-9");
    }
    if (flags.Intersects({LexerFlag::kIdentUpper, LexerFlag::kIdentForceLower,
                          LexerFlag::kIdentForceUpper})) {
      absl::StrAppend(&token_pattern, "A-Z");
    }
    if (flags.Intersects({LexerFlag::kIdentLower, LexerFlag::kIdentForceLower,
                          LexerFlag::kIdentForceUpper})) {
      absl::StrAppend(&token_pattern, "a-z");
    }
    absl::StrAppend(&token_pattern, "]*", RE2::QuoteMeta(config.ident_suffix),
                    ")|");
  }

  for (const auto& user_token : config.user_tokens) {
    if (user_token.type < kTokenUser) {
      error = LexerProgram::kErrorInvalidUserTokenType;
      return false;
    }
    if (RE2 re(user_token.regex);
        re.error_code() != RE2::NoError || re.NumberOfCapturingGroups() != 1) {
      error = LexerProgram::kErrorInvalidUserTokenRegex;
      return false;
    }
    absl::StrAppend(&token_pattern, "(?:", user_token.regex, ")|");
  }

  if (!token_pattern.empty()) {
    token_pattern.pop_back();
  }

  return true;
}

bool CreateSymbolPattern(std::string& symbol_pattern, std::string& symbol_chars,
                         const LexerConfig& config, std::string& error) {
  if (config.symbols.empty()) {
    return true;
  }
  absl::flat_hash_set<Symbol> symbols;
  absl::StrAppend(&symbol_pattern, "(");
  for (const Symbol& symbol : config.symbols) {
    if (!symbol.IsValid()) {
      error = LexerProgram::kErrorInvalidSymbolSpec;
      return false;
    }
    if (!symbols.insert(symbol).second) {
      error = LexerProgram::kErrorDuplicateSymbolSpec;
      return false;
    }
    const char first_char = symbol.GetString()[0];
    if (symbol_chars.find(first_char) == std::string::npos) {
      symbol_chars.push_back(first_char);
    }
    absl::StrAppend(&symbol_pattern, RE2::QuoteMeta(symbol.GetString()));
    absl::StrAppend(&symbol_pattern, "|");
  }
  symbol_pattern.back() = ')';
  return true;
}

bool CreateWhitespacePattern(std::string& whitespace_pattern,
                             std::string& whitespace_chars,
                             const LexerConfig& config, std::string& error) {
  absl::flat_hash_set<std::string_view> comment_starts;
  if (whitespace_chars.find(' ') == std::string::npos) {
    whitespace_chars.push_back(' ');
  }
  if (whitespace_chars.find('\t') == std::string::npos) {
    whitespace_chars.push_back('\t');
  }
  if (config.block_comments.empty()) {
    whitespace_pattern = "[ \\t]*";
  } else {
    absl::StrAppend(&whitespace_pattern, "(?:[ \t]|");
    for (const auto& [start, end] : config.block_comments) {
      if (start.empty() || end.empty()) {
        error = LexerProgram::kErrorEmptyCommentSpec;
        return false;
      }
      if (!comment_starts.insert(start).second) {
        error = LexerProgram::kErrorConflictingCommentSpec;
        return false;
      }
      const char first_char = start[0];
      if (whitespace_chars.find(first_char) == std::string::npos) {
        whitespace_chars.push_back(first_char);
      }
      absl::StrAppend(&whitespace_pattern, RE2::QuoteMeta(start), ".*?",
                      RE2::QuoteMeta(end), "|");
    }
    whitespace_pattern.pop_back();
    absl::StrAppend(&whitespace_pattern, ")*");
  }
  if (!config.line_comments.empty()) {
    absl::StrAppend(&whitespace_pattern, "(?:(?:");
    for (const std::string_view& line_comment : config.line_comments) {
      if (line_comment.empty()) {
        error = LexerProgram::kErrorEmptyCommentSpec;
        return false;
      }
      if (!comment_starts.insert(line_comment).second) {
        error = LexerProgram::kErrorConflictingCommentSpec;
        return false;
      }
      const char first_char = line_comment[0];
      if (whitespace_chars.find(first_char) == std::string::npos) {
        whitespace_chars.push_back(first_char);
      }
      absl::StrAppend(&whitespace_pattern, RE2::QuoteMeta(line_comment), "|");
    }
    whitespace_pattern.pop_back();
    absl::StrAppend(&whitespace_pattern, ").*)?");
  }
  return true;
}

const RE2::Options Re2MatchLongest() {
  static absl::once_flag once;
  static RE2::Options options;
  absl::call_once(once, [] { options.set_longest_match(true); });
  return options;
}

}  // namespace

std::unique_ptr<const LexerProgram> LexerProgram::Create(
    LexerConfig config, std::string* error_message) {
  State state;

  std::string temp_error;
  std::string& error = (error_message != nullptr ? *error_message : temp_error);
  error.clear();

  state.flags = config.flags;

  std::string token_pattern;
  if (!CreateTokenPattern(token_pattern, config, error)) {
    return nullptr;
  }

  int index = 0;
  int binary_index = -1;
  int octal_index = -1;
  int decimal_index = -1;
  int hex_index = -1;
  int float_index = -1;
  int char_index = -1;
  int string_index = -1;
  int keyword_index = -1;
  int ident_index = -1;
  if (LexerSupportsIntegers(config.flags)) {
    if (config.flags.IsSet(LexerFlag::kBinaryIntegers)) {
      binary_index = index++;
    }
    if (config.flags.IsSet(LexerFlag::kOctalIntegers)) {
      octal_index = index++;
    }
    if (config.flags.IsSet(LexerFlag::kDecimalIntegers)) {
      decimal_index = index++;
    }
    if (config.flags.Intersects(
            {LexerFlag::kHexUpperIntegers, LexerFlag::kHexLowerIntegers})) {
      hex_index = index++;
    }
  }
  if (LexerSupportsFloats(config.flags)) {
    float_index = index++;
  }
  if (LexerSupportsCharacters(config.flags)) {
    char_index = index++;
  }
  if (LexerSupportsStrings(config.flags)) {
    string_index = index++;
  }
  if (!config.keywords.empty()) {
    keyword_index = index++;
  }
  if (LexerSupportsIdentifiers(config.flags)) {
    ident_index = index++;
  }
  const int builtin_pattern_count = index;
  const int token_pattern_count =
      builtin_pattern_count + config.user_tokens.size();

  std::string token_end_chars;
  std::string symbol_pattern;
  if (!CreateSymbolPattern(symbol_pattern, token_end_chars, config, error)) {
    return nullptr;
  }

  if (token_pattern.empty() && symbol_pattern.empty()) {
    error = LexerProgram::kErrorNoTokenSpec;
    return nullptr;
  }

  std::string whitespace_pattern;
  if (!CreateWhitespacePattern(whitespace_pattern, token_end_chars, config,
                               error)) {
    return nullptr;
  }

  token_end_chars = RE2::QuoteMeta(token_end_chars);
  std::string token_end_pattern = absl::StrCat("[", token_end_chars, "]+");
  std::string not_token_end_pattern = absl::StrCat("[^", token_end_chars, "]*");

  state.binary_config.prefix = config.binary_prefix.size();
  state.binary_config.size_offset =
      state.binary_config.prefix + config.binary_suffix.size();
  state.octal_config.prefix = config.octal_prefix.size();
  state.octal_config.size_offset =
      state.octal_config.prefix + config.octal_suffix.size();
  state.decimal_config.prefix = config.decimal_prefix.size();
  state.decimal_config.size_offset =
      state.decimal_config.prefix + config.decimal_suffix.size();
  state.hex_config.prefix = config.hex_prefix.size();
  state.hex_config.size_offset =
      state.hex_config.prefix + config.hex_suffix.size();
  state.float_config.prefix = config.float_prefix.size();
  state.float_config.size_offset =
      state.float_config.prefix + config.float_suffix.size();
  state.ident_config.prefix = config.ident_prefix.size();
  state.ident_config.size_offset =
      state.ident_config.prefix + config.ident_suffix.size();
  state.escape = config.escape;
  state.escape_newline = config.escape_newline;
  state.escape_tab = config.escape_tab;
  state.escape_hex = config.escape_hex;

  state.re_whitespace = std::make_unique<RE2>(whitespace_pattern);
  state.re_symbol = std::make_unique<RE2>(symbol_pattern, Re2MatchLongest());
  state.re_token_end = std::make_unique<RE2>(token_end_pattern);
  state.re_not_token_end = std::make_unique<RE2>(not_token_end_pattern);
  state.re_token = std::make_unique<RE2>(token_pattern, Re2MatchLongest());
  state.re_args.resize(token_pattern_count);
  if (binary_index >= 0) {
    const int i = binary_index;
    state.re_args[i].type = kTokenInt;
    state.re_args[i].int_parse_type = IntParseType::kBinary;
  }
  if (octal_index >= 0) {
    const int i = octal_index;
    state.re_args[i].type = kTokenInt;
    state.re_args[i].int_parse_type = IntParseType::kOctal;
  }
  if (decimal_index >= 0) {
    const int i = decimal_index;
    state.re_args[i].type = kTokenInt;
  }
  if (hex_index >= 0) {
    const int i = hex_index;
    state.re_args[i].type = kTokenInt;
    state.re_args[i].int_parse_type = IntParseType::kHex;
  }
  if (float_index >= 0) {
    const int i = float_index;
    state.re_args[float_index].type = kTokenFloat;
  }
  if (char_index >= 0) {
    const int i = char_index;
    state.re_args[char_index].type = kTokenChar;
  }
  if (string_index >= 0) {
    const int i = string_index;
    state.re_args[string_index].type = kTokenString;
  }
  if (keyword_index >= 0) {
    const int i = keyword_index;
    state.re_args[keyword_index].type = kTokenKeyword;
  }
  if (ident_index >= 0) {
    const int i = ident_index;
    state.re_args[ident_index].type = kTokenIdentifier;
  }
  for (int j = 0; j < config.user_tokens.size(); ++j) {
    const int i = builtin_pattern_count + j;
    const LexerConfig::UserToken& user_token = config.user_tokens[j];
    if (!user_token.name.empty()) {
      state.user_token_names[user_token.type] = user_token.name;
    }
    state.re_args[i].type = user_token.type;
  }
  if (LexerSupportsIntegers(config.flags)) {
    if (config.flags.IsSet(LexerFlag::kInt64)) {
      state.max_int = std::numeric_limits<int64_t>::max();
      state.min_int = std::numeric_limits<int64_t>::min();
      state.int_sign_extend = 0;
    } else if (config.flags.IsSet(LexerFlag::kInt32)) {
      state.max_int = std::numeric_limits<int32_t>::max();
      state.min_int = std::numeric_limits<int32_t>::min();
      state.int_sign_extend = 0xFFFFFFFF00000000;
    } else if (config.flags.IsSet(LexerFlag::kInt16)) {
      state.max_int = std::numeric_limits<int16_t>::max();
      state.min_int = std::numeric_limits<int16_t>::min();
      state.int_sign_extend = 0xFFFFFFFFFFFF0000;
    } else if (config.flags.IsSet(LexerFlag::kInt8)) {
      state.max_int = std::numeric_limits<int8_t>::max();
      state.min_int = std::numeric_limits<int8_t>::min();
      state.int_sign_extend = 0xFFFFFFFFFFFFFF00;
    }
  }
  if (config.flags.IsSet(LexerFlag::kKeywordCaseInsensitive)) {
    for (const std::string_view& keyword : config.keywords) {
      state.keywords[absl::AsciiStrToLower(keyword)] = keyword;
    }
  }
  for (const auto& [start, end] : config.block_comments) {
    state.block_comments.emplace_back(std::string(start), std::string(end));
  }

  return absl::WrapUnique(new LexerProgram(state));
}

}  // namespace gb
