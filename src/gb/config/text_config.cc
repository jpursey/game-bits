// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/config/text_config.h"

#include "absl/base/no_destructor.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "gb/parse/lexer_config.h"
#include "gb/parse/parser.h"
#include "gb/parse/parser_program.h"
#include "gb/parse/symbol.h"

namespace gb {

namespace {

//=============================================================================
// WriteConfigToText implementation
//=============================================================================

void WriteConfig(std::string& text, int child_indent, const Config& config,
                 TextConfigFlags flags);

void WriteString(std::string& text, absl::string_view value,
                 TextConfigFlags flags) {
  // Determine quote character to use. By default, we use double quotes, but if
  // single quotes are allowed and the string contains a double quote but not a
  // single quote, we can use single quotes to avoid escaping the double quote.
  char quote_char = '\"';
  if (flags.IsSet(TextConfigFlag::kSingleQuotes)) {
    const bool has_single_quote = (value.find('\'') != absl::string_view::npos);
    const bool has_double_quote = (value.find('"') != absl::string_view::npos);
    if (!has_single_quote && has_double_quote) {
      quote_char = '\'';
    }
  }

  text.push_back(quote_char);

  // This is a quite inefficient way to escape strings, but it is simple and
  // should work well enough for small configs. If performance becomes an issue,
  // this can be optimized.
  for (char c : value) {
    switch (c) {
      case '\n':
        text.append("\\n");
        break;
      case '\t':
        text.append("\\t");
        break;
      case '\\':
        text.append("\\\\");
        break;
      case '"':
      case '\'':
        if (c == quote_char) {
          text.push_back('\\');
        }
        text.push_back(c);
        break;
      default:
        if (c < 0x20) {
          absl::StrAppendFormat(&text, "\\x%02x",
                                static_cast<unsigned char>(c));
        } else {
          text.push_back(c);
        }
        break;
    }
  }

  text.push_back(quote_char);
}

bool IsValidIdentifier(absl::string_view key) {
  return !key.empty() && (absl::ascii_isalpha(key[0]) || key[0] == '_') &&
         std::all_of(key.begin() + 1, key.end(),
                     [](char c) { return absl::ascii_isalnum(c) || c == '_'; });
}

void WriteKey(std::string& text, absl::string_view key, TextConfigFlags flags) {
  // Prefer to write keys as identifiers if possible, since it is more concise
  // and more visually distinct from string values.
  if (flags.IsSet(TextConfigFlag::kIdentifiers) && IsValidIdentifier(key)) {
    absl::StrAppend(&text, key);
  } else {
    WriteString(text, key, flags);
  }
}

void WriteCompactMapValues(std::string& text, int child_indent,
                           const Config::MapValue& map, TextConfigFlags flags) {
  absl::string_view next_value;
  absl::btree_map<std::string, Config> ordered_map(map.begin(), map.end());
  for (const auto& [key, value] : ordered_map) {
    absl::StrAppend(&text, next_value);
    WriteKey(text, key, flags);
    text.push_back(':');
    WriteConfig(text, child_indent, value, flags);
    next_value = ",";
  }
}

void WriteMapValues(std::string& text, int child_indent,
                    const Config::MapValue& map, TextConfigFlags flags) {
  if (flags.IsSet(TextConfigFlag::kCompact)) {
    WriteCompactMapValues(text, child_indent, map, flags);
    return;
  }
  std::string indent(child_indent * kTextConfigIndentSize, ' ');
  absl::string_view next_value;
  absl::btree_map<std::string, Config> ordered_map(map.begin(), map.end());
  for (const auto& [key, value] : ordered_map) {
    absl::StrAppend(&text, next_value, indent);
    WriteKey(text, key, flags);
    absl::StrAppend(&text, ": ");
    WriteConfig(text, child_indent, value, flags);
    next_value = ",\n";
  }
}

void WriteCompactArrayValues(std::string& text, int child_indent,
                             absl::Span<const Config>& array,
                             TextConfigFlags flags) {
  absl::string_view next_value;
  for (const auto& value : array) {
    absl::StrAppend(&text, next_value);
    WriteConfig(text, child_indent, value, flags);
    next_value = ",";
  }
}

std::string::size_type GetLastNewLine(const std::string& text) {
  std::string::size_type line_start = text.rfind('\n');
  if (line_start == std::string::npos) {
    line_start = 0;
  }
  return line_start;
}

void WriteArrayValues(std::string& text, int child_indent,
                      absl::Span<const Config> array, TextConfigFlags flags) {
  if (flags.IsSet(TextConfigFlag::kCompact)) {
    WriteCompactArrayValues(text, child_indent, array, flags);
    return;
  }
  std::string indent(child_indent * kTextConfigIndentSize + 1, ' ');
  indent[0] = '\n';
  int next_indent_size = indent.size() - 1;
  std::string::size_type line_start = GetLastNewLine(text);
  absl::string_view next_value;
  for (const auto& value : array) {
    std::string::size_type value_start = text.size() + next_value.size() / 2;
    absl::StrAppend(&text, next_value);
    WriteConfig(text, child_indent, value, flags);
    if (text.size() - line_start >= kTextConfigMaxArrayLineLength) {
      text.insert(value_start, indent);
      line_start = text.size() - indent.size() + 1;
    }
    next_value = ", ";
    indent.resize(next_indent_size);
  }
}

void WriteConfig(std::string& text, int child_indent, const Config& config,
                 TextConfigFlags flags) {
  switch (config.GetType()) {
    case Config::Type::kNone:
      absl::StrAppend(&text, "null");
      break;
    case Config::Type::kBool:
      absl::StrAppend(&text, config.GetBool() ? "true" : "false");
      break;
    case Config::Type::kInt:
      absl::StrAppend(&text, config.GetInt());
      break;
    case Config::Type::kFloat:
      absl::StrAppend(&text, config.GetFloat());
      break;
    case Config::Type::kString:
      WriteString(text, config.GetString(), flags);
      break;
    case Config::Type::kArray:
      absl::StrAppend(&text, "[");
      WriteArrayValues(text, child_indent + 1, config.GetArray(), flags);
      absl::StrAppend(&text, "]");
      break;
    case Config::Type::kMap: {
      absl::StrAppend(&text, "{");
      if (!flags.IsSet(TextConfigFlag::kCompact)) {
        absl::StrAppend(&text, "\n");
      }
      WriteMapValues(text, child_indent + 1, *config.GetMap(), flags);
      if (!flags.IsSet(TextConfigFlag::kCompact)) {
        std::string close_indent(child_indent * kTextConfigIndentSize, ' ');
        absl::StrAppend(&text, "\n", close_indent);
      }
      absl::StrAppend(&text, "}");
      break;
    }
  }
}

//==============================================================================
// ReadConfigFromText implementation
//==============================================================================

constexpr Symbol kSymbols[] = {
    '[', ']', '{', '}', ':', ',',
};
constexpr LexerConfig kBaseLexerConfig = {
    .flags =
        {
            kLexerFlags_Positive64BitNumbers,
            kLexerFlags_NegativeNumbers,
            LexerFlag::kHexUpperIntegers,
            LexerFlag::kHexLowerIntegers,
            kLexerFlags_AllFloatFormats,
            kLexerFlags_CIdentifiers,
            kLexerFlags_CStrings,
            LexerFlag::kDecodeEscape,
        },
    .hex_prefix = "0x",
    .escape = '\\',
    .escape_newline = 'n',
    .escape_tab = 't',
    .escape_hex = 'x',
    .symbols = kSymbols,
};

// The entire parser program except for the "Key" rule, which must be added in
// conditionally based on the TextConfigFlags.
constexpr std::string_view kBaseParserProgram = R"---(
  Rooted {
    <Value>
   %end:"Unexpected text after value";
  }  

  Rootless {
    $map=RootlessMap;
  }

  RootlessMap {
    $values=MapValue,*
    %end:"Invalid key";
  }

  Array {
    "[" $values=Value,* "]";
  }

  Map {
    "{" $values=MapValue,* "}";
  }

  MapValue {
    $key=<Key>:"Expected string or identifier for key"
    ":"
    $value=Value;
  }

  Value {
    $null="null";
    $bool=("true" | "false");
    $int=%int;
    $float=%float;
    $string=%string;
    $array=Array;
    $map=Map;
  }
)---";

constexpr std::string_view kStringKeyParserProgram = R"---(
  Key {
    %string;
  }
)---";

constexpr std::string_view kIdentKeyParserProgram = R"---(
  Key {
    (%string | %ident);
  }
)---";

std::shared_ptr<ParserProgram> GetParserProgram(TextConfigFlags flags) {
  static absl::NoDestructor<
      absl::flat_hash_map<uint64_t, std::shared_ptr<ParserProgram>>>
      parser_programs;

  // Clear flags that only affect writing.
  flags.Clear({TextConfigFlag::kRootless, TextConfigFlag::kCompact});

  if (auto it = parser_programs->find(flags.GetMask());
      it != parser_programs->end()) {
    return it->second;
  }

  LexerConfig lexer_config = kBaseLexerConfig;
  if (flags.IsSet(TextConfigFlag::kSingleQuotes)) {
    lexer_config.flags.Set(LexerFlag::kSingleQuoteString);
  }
  if (flags.IsSet(TextConfigFlag::kComments)) {
    lexer_config.line_comments = kCStyleLineComments;
    lexer_config.block_comments = kCStyleBlockComments;
  }

  std::string parser_program_text = std::string(kBaseParserProgram);
  if (flags.IsSet(TextConfigFlag::kIdentifiers)) {
    absl::StrAppend(&parser_program_text, kIdentKeyParserProgram);
  } else {
    absl::StrAppend(&parser_program_text, kStringKeyParserProgram);
  }

  std::string error_message;
  auto& parser_program = (*parser_programs)[flags.GetMask()] =
      ParserProgram::Create(lexer_config, parser_program_text, &error_message);
  DCHECK_NE(parser_program, nullptr) << error_message;
  return parser_program;
}

Config GetParsedConfig(const ParsedItem& item) {
  std::string_view item_type = item.GetMatchName();
  if (item_type == "null") {
    return Config::None();
  }
  if (item_type == "bool") {
    return Config::Bool(item.GetToken("bool").GetString() == "true");
  }
  if (item_type == "int") {
    return Config::Int(item.GetToken("int").GetInt64());
  }
  if (item_type == "float") {
    return Config::Float(item.GetToken("float").GetFloat64());
  }
  if (item_type == "string") {
    return Config::String(item.GetToken("string").GetString());
  }
  if (item_type == "array") {
    std::vector<Config> array;
    for (const auto& value_item : item.GetItem("array")->GetItems("values")) {
      array.push_back(GetParsedConfig(value_item));
    }
    return Config::Array(std::move(array));
  }
  if (item_type == "map") {
    Config::MapValue map;
    for (const auto& value_item : item.GetItem("map")->GetItems("values")) {
      std::string_view key = value_item.GetToken("key").GetString();
      map[key] = GetParsedConfig(*value_item.GetItem("value"));
    }
    return Config::Map(std::move(map));
  }
  LOG(DFATAL) << "Unexpected item type: " << item_type;
  return Config::None();
}

}  // namespace

std::string WriteConfigToText(const Config& config, TextConfigFlags flags) {
  std::string text;
  if (!flags.IsSet(TextConfigFlag::kRootless)) {
    WriteConfig(text, 0, config, flags);
  } else if (config.IsMap()) {
    WriteMapValues(text, 0, *config.GetMap(), flags);
  }
  return text;
}

absl::StatusOr<Config> ReadConfigFromText(std::string text,
                                          TextConfigFlags flags) {
  auto parser = Parser::Create(GetParserProgram(flags));
  DCHECK_NE(parser, nullptr);
  LexerContentId content = parser->GetLexer().AddContent(std::move(text));
  ParseResult result = parser->Parse(
      content, flags.IsSet(TextConfigFlag::kRootless) ? "Rootless" : "Rooted");
  if (!result) {
    return absl::InvalidArgumentError(result.GetError().FormatMessage());
  }

  Config config = GetParsedConfig(*result.GetItem());

  return config;
}

}  // namespace gb
