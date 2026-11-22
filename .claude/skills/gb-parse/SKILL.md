---
name: gb-parse
description: How to read a text format with Game Bits' gb_parse library (gb/parse/lexer.h, lexer_config.h, parser_program.h, parser.h, parse_result.h) by writing a LexerConfig and a grammar in parser program text, then walking the ParsedItem tree. Use when adding or changing a text format, config syntax, script language, or command syntax that a program reads, or code that reads gb_parse results, in Game Bits or any project built on it.
---

# Game Bits parsing (gb_parse)

`gb_parse` reads text formats: asset and config files, scripts, and small
command languages. A format is defined by two pieces of data, and the code that
reads it walks the tree the parser builds:
- **A `LexerConfig`** says what the tokens are: which numbers, strings,
  identifiers, symbols, keywords, and comments the format has.
- **A parser program** is the grammar, written as text in the format described
  at the top of `src/gb/parse/parser_program.h`.
- **`Parser::Parse()`** matches a rule from the program against the text and
  returns a `ParseResult`: a `ParseError`, or a tree of `ParsedItem`s whose
  parts are found by the names the grammar gives them.

The headers are the reference: `src/gb/parse/parser_program.h` (the grammar
syntax), `lexer_config.h` (the lexer's flags and presets), `lexer.h` (token
rules the lexer always follows, and its limits), and `parse_result.h` (reading
the tree). In a project built on Game Bits, they are under `$GB_DIR`. This skill
covers how they are used together.

Two formats in the tree show the whole pattern, and are the best place to start
when writing a new one:
- `src/gb/pilot/script/pilot_script.cc`: a line-based script language, parsed
  one line at a time, with custom error messages.
- `src/gb/config/text_config.cc`: a JSON-like config format, with grammar
  pieces chosen by flags, and recursive values.

## The shape of a reader

```cpp
namespace {

constexpr Symbol kSymbols[] = {'{', '}', ':', ','};

constexpr LexerConfig kLexerConfig = {
    .flags = {kLexerFlags_CIdentifiers, kLexerFlags_CStrings,
              kLexerFlags_Positive64BitNumbers, kLexerFlags_NegativeNumbers,
              LexerFlag::kDecodeEscape},
    .escape = '\\',
    .escape_newline = 'n',
    .escape_tab = 't',
    .escape_hex = 'x',
    .line_comments = kCStyleLineComments,
    .symbols = kSymbols,
};

constexpr std::string_view kProgram = R"---(
  file {
    $entries=entry* %end:"Expected an entry";
  }
  entry {
    $name=%ident ":" "{" $values=value,* "}";
  }
  value { $int=%int; $string=%string; }
)---";

std::shared_ptr<const ParserProgram> CreateProgram() {
  std::string error_message;
  std::shared_ptr<const ParserProgram> program =
      ParserProgram::Create(kLexerConfig, kProgram, &error_message);
  CHECK(program != nullptr) << error_message;
  return program;
}

std::shared_ptr<const ParserProgram> GetProgram() {
  static absl::NoDestructor<std::shared_ptr<const ParserProgram>> s_program(
      CreateProgram());
  return *s_program;
}

}  // namespace

absl::StatusOr<Thing> ReadThing(std::string text, std::string_view filename) {
  std::unique_ptr<Parser> parser = Parser::Create(GetProgram());
  Lexer& lexer = parser->GetLexer();
  const LexerContentId content = lexer.AddFileContent(filename, std::move(text));
  if (content == kNoLexerContent) {
    return absl::InvalidArgumentError("Too many lines, or a line too long");
  }
  ParseResult result = parser->Parse(content, "file");
  if (!result.IsOk()) {
    return absl::InvalidArgumentError(result.GetError().FormatMessage());
  }
  for (const ParsedItem& entry : result->GetItems("entries")) { ... }
}
```

- **Build the `ParserProgram` once.** Creating one lexes and validates the
  grammar, which is far more work than a parse. Keep it in a function-local
  `static absl::NoDestructor` (or one per variant, as `text_config.cc` does
  for its flags), and `CHECK` that it was created: the grammar is part of the
  code, so an error in it is a bug, not bad input.
- **Create a `Parser` for each text read.** The parser owns its `Lexer`, which
  owns the text it was given and the tokens it reads from it. Parsers are
  cheap, and the program they share is immutable, so each read (and each
  thread) gets its own.
- **`ParsedItem`s and `Token`s point into the lexer**, so read everything
  needed from the tree (or copy out strings) before the parser is destroyed.

## The lexer

The lexer decides what a token is, so get it right before writing rules. The
lexer has a few fixed rules, listed at the top of `lexer.h`, that matter when
designing a format:
- **Every symbol must be listed in `LexerConfig::symbols`**, single characters
  included, and so must every symbol a rule uses as a literal (`":"`). An
  unlisted character is an error token.
- **Tokens never cross a line.** Newlines are whitespace unless
  `LexerFlag::kLineBreak` is set, which makes each a `%linebreak` token that
  rules must then match.
- **Numbers need both a size and a format flag** (`kInt64` and
  `kDecimalIntegers`, for instance), or none are read. The `kLexerFlags_*`
  presets bundle common sets.
- **Escapes are only decoded with `kDecodeEscape`** (and the escape characters
  set); without it, `GetString()` returns a string's text as written.
- **With negative numbers enabled, a `-` at the start or after a symbol starts
  a number, and after anything else is a symbol**, so `-3-2` is `-3`, `-`,
  `2`. If a format needs `-` to always be its own token, leave negative
  numbers out of the lexer and handle the sign in the grammar.
- **Content is limited** to `kMaxLexerLines` lines across all of a lexer's
  content, and `kMaxTokensPerLine` characters per line. `AddContent` and
  `AddFileContent` return `kNoLexerContent` past either, which the reader
  must report.
- **Keywords** (`LexerConfig::keywords`) are only needed when a word must never
  be an identifier. A literal like `"wait"` in a rule matches the identifier
  `wait` without being a keyword, which keeps it usable as a name elsewhere.
- **`user_tokens`** add token types matched by an RE2 regex, for things the
  built-in types can't express. A program names them with `%name = N;`, where
  `N` is the type's offset from `kTokenUser`.

## Writing the grammar

`parser_program.h` gives the full syntax. What matters most when writing one:

- **The parser is greedy, with no backtracking.** In a rule's alternatives
  (`;`) and in a group's (`|`), the *first* one that matches is taken, even if a
  later one would match more or let the rest of the parse succeed. Optional
  and repeated items take everything they can. So:
  - **Order alternatives longest or most specific first.** `pilot_script.cc`
    puts `("wait" "for" ...)` before `("wait" $frames=%int)`, and
    `("move" "by" ...)` before `("move" ...)`.
  - A repeated item followed by something it could also match never leaves it
    for what follows.
- **No left recursion.** A rule can't start with itself. Write precedence
  levels as repeating groups (`term (("+" | "-") term)*`), and apply left
  associativity when reading the tree.
- **`Parse()` doesn't require the whole text to match.** It matches the rule and
  stops, leaving the rest. End the top rule with `%end` (and a message), or loop
  until the content is used up, as `pilot_script.cc` does line by line.
- **Literals are written as they lex**: `"{"` for a symbol, `"true"` for an
  identifier or keyword. A literal must lex as exactly one token in the
  format's lexer.
- **`%linebreak` is only a token with `kLineBreak`**; then every rule that can
  span the end of a line has to say so. A line-based format usually parses
  one line per `Parse()` call, with a rule that ends in `%linebreak` or `%end`.
- **Repeats with commas** (`,*`, `,+`) allow no trailing comma. Write
  `$values=value,+ [","]` to allow one, or `($values=value [","])*` to make
  commas optional (both in `text_config.cc`).
- **`!item` matches any one token that isn't `item`**, for "everything up to
  here" (`$tokens=!%linebreak*`). **`^item`** matches only when touching the
  previous token, for formats where spacing matters (`ADD.W`). **`%here`**
  marks a position without consuming anything.

### Names: what the reader can see

Only named items are kept in the tree. Everything else is matched and dropped.
- **`$name=item`** keeps the item under `name`. A repeated named item keeps
  each match under the same name, in order (`GetItems("values")`).
- **A rule's named items are scoped to the rule**: using `entry` inside another
  rule hides its names, unless that use is named (`$entry=entry`, then
  `GetItem("entry")->GetItem("name")`, or `GetItems("entry.name")`), or
  **promoted** with angle brackets (`<entry>`), which makes its names the
  enclosing rule's own. `text_config.cc` promotes its `Value` rule this way.
- **Name each alternative** (`$wait=(...)`, `$int=%int`) when the reader needs
  to know which one matched, then switch on `GetMatchName()`, which returns
  the single name matched in the item.

### Error messages

A failed parse reports the error at the furthest token any alternative reached,
and the last message set at that token wins.
- **Add a message to the item that tells the user what was expected**:
  `%end:"Unexpected text after value"`, `step:"Unknown step"`. Without one, the
  error names the unexpected token.
- **Put messages where the parse fails**, which is the furthest point
  reached, not necessarily the item at fault. The comment above
  `kScriptProgram` in `pilot_script.cc` explains how its messages are placed.

## Reading the tree

- `result->GetItems("name")`, `GetItem("name")` (null if absent),
  `GetToken("name")`, and the typed `GetInt`/`GetFloat`/`GetString("name",
  default)` read the named items of an item. Names can be dotted to reach
  into a named sub-item.
- `item.GetToken()` is an item's first token. Its `IsInt()`, `IsString()`, and
  so on tell which kind of token matched.
- Check what the grammar can't: ranges (`"Frames can't be negative"`), and
  combinations. Report them at the offending token, formatted the way the
  parser formats its own errors, with
  `ParseError(lexer.GetTokenLocation(token), message).FormatMessage()`, so
  every error in the format reads the same (`ScriptError()` in
  `pilot_script.cc`).
- `lexer.GetTokenText(token)` returns the token as written, which is what to
  show the user, or to keep when a value's spelling matters (as `ReadName()`
  in `pilot_script.cc` does for an integer used as a name).

## Testing a format

- Test through the reader's public function (`ReadPilotScript()`,
  `ReadConfigFromText()`), with the text written inline, rather than testing
  the grammar on its own.
- Cover each alternative, and each error message: check the whole formatted
  error, including its line and column, so a change in where the parse fails is
  caught.
- Test the edges the lexer sets: an empty text, a missing final newline in a
  line-based format, and a value at each end of a number's range.
