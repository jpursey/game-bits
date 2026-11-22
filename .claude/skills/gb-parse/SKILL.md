---
name: gb-parse
description: How to lex and parse text with Game Bits' gb_parse library (gb/parse/lexer.h, lexer_config.h, parser_program.h, parser.h, parse_result.h), by writing a LexerConfig and a grammar in parser program text, then walking the ParsedItem tree. Use whenever code reads structured text (an asset or data format, a script or command language, expressions, a line-based syntax, or a value inside a config) instead of hand-writing a tokenizer or parser, in Game Bits or any project built on it.
---

# Game Bits parsing (gb_parse)

## Use it

**Any code that reads structured text uses `gb_parse`'s lexer and parser rather
than hand-written tokenizing or parsing.** That includes small cases: a
command syntax, an expression, a value inside a config, a line-based script.
Scanning characters and hand-written recursive descent are what it replaces.
Lean on the grammar for as much of the parsing as it can do, and keep the C++
for what the grammar can't express.

The one exception is text that is only a list of strings separated by a
delimiter, with nothing else to it: no whitespace to skip, comments, quoting,
or nesting. Split that with `absl::StrSplit`.

The lexer and parser aren't fully general, but the text they can read is
generally text that is also easy for people to read. If a format is hard or
impossible to express with them, change the format rather than drop the
library.

**Configuration files use `gb_config`** (`gb/config`), not a new config
language. A config value that has a syntax of its own is read from the config as
a string, and then parsed with `gb_parse`.

## The pieces

- **`LexerConfig`** (`lexer_config.h`) says what the tokens are: which numbers,
  strings, identifiers, symbols, keywords, and comments the text has.
- **`ParserProgram`** (`parser_program.h`) is the grammar, always written as
  program text, in the syntax described at the top of that header. Don't build
  rules in C++ with `ParserRules` and `ParserRuleItem`: they exist only because
  the program text's own grammar can't be defined in program text.
- **`Parser`** (`parser.h`) matches one of the program's rules against a
  `Lexer`'s content, starting at its current token, and returns a
  `ParseResult` (`parse_result.h`): a `ParseError`, or a tree of `ParsedItem`s
  whose parts are found by the names the grammar gives them.
- **`Lexer`** (`lexer.h`) owns the text, and reads tokens from it on demand. Its
  header lists the token rules it always follows, and its limits.

In a project built on Game Bits, these are under `$GB_DIR/src/gb/parse`. Two
readers in the tree show the whole pattern:
- `src/gb/pilot/script/pilot_script.cc`: a line-based script language, parsed
  one line at a time, with custom error messages.
- `src/gb/config/text_config.cc`: the `gb_config` text format, with grammar
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

- **Build each `ParserProgram` once**, in a function-local
  `static absl::NoDestructor`, and `CHECK` that it was created: the grammar is
  part of the code, so an error in it is a bug, not bad input. Creating one
  lexes and validates the whole grammar, which is far more work than a parse.
- **Create a `Parser` (and so a `Lexer`) for each text read.** The lexer holds
  the text and the read position, so it belongs to one read. The program it
  shares is immutable.

## Tokens

**Keep `Token`s rather than turning them into strings early.** A token is a
small value that refers into the lexer's content, and stays valid for as long
as the lexer does: content is never removed from a lexer until it is destroyed.
A token carries its type, its value (`GetInt()`, `GetString()`, ...), and,
through the lexer, its text as written (`GetTokenText()`) and its location
(`GetTokenLocation()`), which every later error message needs. Copy a value out
only when it has to outlive the lexer.

**Each piece of content is stored as one string** (with every line break as
`"\n"`, whatever line endings it was given), so any range of text within it is
a `std::string_view` into that string, with nothing copied. The lexer gives
these views directly:
- `GetContentText()`: the whole content.
- `GetLineText()`: a whole line, and `NextLine()`: the rest of the current
  line.
- `GetTokenText()`: a token as written.
- `GetTokenRangeText(first, last)`: the text from the start of one token to
  the end of another in the same content, with the spaces and comments between
  them as written. This is how to get the raw text of something the grammar
  matched, such as an argument matched as `$arg=!(%linebreak | ',')*`: pass
  the first and last of its items' tokens.

## The lexer

**Use the lexer's own forms for identifiers, numbers, strings, and comments**,
through its flags and the `kLexerFlags_*` presets, so that everything built
with Game Bits reads them the same way. Where a language needs a token they
can't express, define a user token (`LexerConfig::user_tokens`, an RE2 regex),
rather than hand-lexing it. A program names it with `%name = N;`, where `N` is
its offset from `kTokenUser`.

Rules the lexer always follows, which matter when designing a format (see the
top of `lexer.h` for the full list):
- **Every symbol must be listed in `LexerConfig::symbols`**, single characters
  included, and so must every symbol a rule uses as a literal (`":"`). An
  unlisted character is an error token.
- **Tokens never cross a line.** Newlines are whitespace unless
  `LexerFlag::kLineBreak` is set, which makes each one a `%linebreak` token
  that rules must then match.
- **Numbers need both a size and a format flag** (`kInt64` and
  `kDecimalIntegers`, for instance), or none are read.
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

## Writing the grammar

`parser_program.h` gives the full syntax. What matters most when writing one:

- **A program is a set of rules, not one root.** Define the rules each context
  needs, and parse whichever one fits where the text is. A program can serve
  several kinds of text, or several places in one text.
- **The parser is greedy, with no backtracking.** In a rule's alternatives
  (`;`) and in a group's (`|`), the *first* one that matches is taken, even if a
  later one would match more or let the rest of the parse succeed. Optional
  and repeated items take everything they can. So order alternatives longest
  or most specific first: `pilot_script.cc` puts `("wait" "for" ...)` before
  `("wait" $frames=%int)`.
- **`Parse()` doesn't require the whole text to match.** It matches the rule and
  stops, leaving the lexer after what it matched, or where it started if the
  rule didn't match. End a rule with `%end` (and a message) when the rest of
  the text must be empty, or parse again from where it stopped.
- **Literals are written as they lex**: `"{"` for a symbol, `"true"` for an
  identifier or keyword. A literal must lex as exactly one token in the
  program's lexer.
- **`%linebreak` is only a token with `kLineBreak`**; then every rule that can
  span the end of a line has to say so. A line-based format usually parses one
  line per `Parse()` call, with a rule that ends in `%linebreak` or `%end`.
- **Repeats with commas** (`,*`, `,+`) allow no trailing comma. Write
  `$values=value,+ [","]` to allow one, or `($values=value [","])*` to make
  commas optional (both in `text_config.cc`).
- **`!item` matches any one token that isn't `item`**, for "everything up to
  here" (`$tokens=!%linebreak*`). **`^item`** matches only when touching the
  previous token, for formats where spacing matters (`ADD.W`). **`%here`**
  marks a position without consuming anything.

### Precedence

Rules can't be left recursive (a rule can't start with itself). Express
operator precedence by nesting one rule per level, lowest precedence first, each
a repeating group over the next level. From OZ-3's assembler expressions
(`oz3/tools/expression.cc` in jpursey/oz-3), shortened:

```
  Expression {
    $args=ExpressionXor ($ops='|' $args=ExpressionXor)*;
  }
  ExpressionXor {
    $args=ExpressionAnd ($ops='^' $args=ExpressionAnd)*;
  }
  ...
  ExpressionSum {
    $args=ExpressionProduct ($ops=('+' | '-') $args=ExpressionProduct)*;
  }
  ExpressionProduct {
    $args=ExpressionUnary ($ops=('*' | '/' | '%') $args=ExpressionUnary)*;
  }
  ExpressionUnary {
    $op=('-' | '~' | '!') $arg=ExpressionUnary;
    $value=ExpressionValue;
  }
  ExpressionValue {
    $number=%int;
    $name=%ident;
    '(' $group=Expression ')';
  }
```

Each level's item holds its operands in `args` and the operators between them
in `ops`, in order, so the reader applies them left to right (or right to left,
for a right-associative operator).

### Names: what the reader can see

Only named items are kept in the tree. Everything else is matched and dropped.
- **`$name=item`** keeps the item under `name`. A repeated named item keeps
  each match under the same name, in order (`GetItems("args")`).
- **A rule's named items are scoped to the rule**: using `entry` inside another
  rule hides its names, unless that use is named (`$entry=entry`, then
  `GetItem("entry")->GetItem("name")`, or `GetItems("entry.name")`), or
  **promoted** with angle brackets (`<entry>`), which makes its names the
  enclosing rule's own. `text_config.cc` promotes its `Value` rule this way.
- **Name each alternative** (`$number=%int`, `$group=Expression`) when the
  reader needs to know which one matched, then switch on `GetMatchName()`,
  which returns the single name matched in the item.

### Error messages

A failed parse reports the error at the furthest token any alternative reached,
and the last message set at that token wins.
- **Add a message to the item that tells the user what was expected**:
  `%end:"Unexpected text after value"`, `step:"Unknown step"`. Without one, the
  error names the unexpected token.
- **Put messages where the parse fails**, which is the furthest point reached,
  not necessarily the item at fault. The comment above `kScriptProgram` in
  `pilot_script.cc` explains how its messages are placed.

## Text that one grammar can't cover

Prefer one grammar that parses as much as it can. When a text doesn't fit one
grammar, the C++ can drive the parse a piece at a time:
- **Choose the rule in C++.** Parse one rule, look at what it matched (or at the
  next token, with `lexer.NextToken(content, /*advance=*/false)`), and decide
  which rule to parse next.
- **Rewind and try again.** A failed `Parse()` leaves the lexer where it
  started. A token kept from earlier can be returned to with
  `lexer.SetNextToken(token)`, and `RewindToken()` steps back one token.
- **Switch programs on the same lexer.** Programs created from the same
  `LexerProgram` (`ParserProgram::Create(lexer_program, text)`) can each make
  a `Parser` (`Parser::Create(lexer, program)`) over one shared `Lexer`
  (`Lexer::Create(lexer_program)`).
- **Skip or take raw lines.** `lexer.NextLine(content)` returns the rest of the
  current line without lexing it, and moves to the next line. A line-based
  format can embed text in another syntax this way, and hand those lines to
  that syntax's own reader (with its own lexer). Reading a line's last token
  (or its `%linebreak`, which is just the newline as a token) doesn't move to
  the next line, so `NextLine()` then returns that line's empty remainder;
  `GetCurrentLine()` says which line comes next.

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

## Testing a grammar

- Test with real program text, parsing the rules under test directly. A test
  can parse any rule, not only the one a reader starts from, so each rule (or
  group of rules) can be tested on its own where that is clearer.
- Cover each alternative, and each error. Check enough of an error to show it
  is the one expected (its message, and its line and column where they matter),
  rather than its whole formatted text, which changes if `gb_parse` changes how
  it formats errors.
- Test the edges the lexer sets: an empty text, a missing final newline in a
  line-based format, and a value at each end of a number's range.
