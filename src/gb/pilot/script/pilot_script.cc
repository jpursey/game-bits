// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/script/pilot_script.h"

#include <memory>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gb/parse/lexer.h"
#include "gb/parse/lexer_config.h"
#include "gb/parse/parse_error.h"
#include "gb/parse/parse_result.h"
#include "gb/parse/parser.h"
#include "gb/parse/parser_program.h"
#include "gb/parse/token.h"

namespace gb {
namespace {

constexpr std::string_view kLineComments[] = {"#"};

constexpr LexerConfig kLexerConfig = {
    .flags =
        {
            kLexerFlags_CIdentifiers,
            kLexerFlags_CStrings,
            LexerFlag::kInt32,
            LexerFlag::kNegativeIntegers,
            LexerFlag::kDecimalIntegers,
            LexerFlag::kDecodeEscape,
            LexerFlag::kLineBreak,
        },
    .escape = '\\',
    .escape_newline = 'n',
    .escape_tab = 't',
    .escape_hex = 'x',
    .line_comments = kLineComments,
};

// Each line is parsed on its own. The parser keeps the error at the furthest
// token, and the last one set there, so the messages given here replace the
// parser's own at the token where each item starts.
constexpr std::string_view kScriptProgram = R"---(
  line {
    %linebreak;
    $step=step:"Unknown step" end_of_line:"Expected end of line";
  }

  end_of_line { %linebreak; %end; }

  step {
    $hide_window=("hide" "window");
    $wait_for=("wait" "for" $condition=name [$frames=%int]);
    $wait=("wait" $frames=%int);
    $key=("key" $key=name);
    $hold=("hold" $key=name $frames=%int);
    $text=("text" $text=%string);
    $move=("move" $x=%int $y=%int);
    $click=("click" [$button=button] [$label=%string | $x=%int $y=%int]);
    $wheel=("wheel" $amount=%int);
    $run=("run" $command=name $args=name*);
    $expect_error=("expect" "error" $text=%string [$frames=%int]);
    $expect=("expect" $condition=name);
    $save=("save" $name=name);
  }

  name { %ident; %int; %string; }
  button { "left"; "middle"; "right"; }
)---";

std::shared_ptr<const ParserProgram> CreateScriptProgram() {
  std::string error_message;
  std::shared_ptr<const ParserProgram> program =
      ParserProgram::Create(kLexerConfig, kScriptProgram, &error_message);
  CHECK(program != nullptr) << error_message;
  return program;
}

std::shared_ptr<const ParserProgram> GetScriptProgram() {
  static absl::NoDestructor<std::shared_ptr<const ParserProgram>> s_program(
      CreateScriptProgram());
  return *s_program;
}

// Returns an error at `token`, written as the parser writes its own.
absl::Status ScriptError(const Lexer& lexer, const Token& token,
                         std::string_view message) {
  return absl::InvalidArgumentError(
      ParseError(lexer.GetTokenLocation(token),
                 absl::StrCat("Parse error at \"", lexer.GetTokenText(token),
                              "\": ", message))
          .FormatMessage());
}

// Returns a name as written, matched by the `name` rule.
std::string ReadName(const Lexer& lexer, const Token& token) {
  if (token.IsInt()) {
    return std::string(lexer.GetTokenText(token));
  }
  return std::string(token.GetString());
}

// Returns the point in `values`. The lexer reads only 32-bit integers, so each
// coordinate fits in an int.
PilotPoint ReadPoint(const ParsedItem& values) {
  return {static_cast<int>(values.GetInt("x")),
          static_cast<int>(values.GetInt("y"))};
}

// Reads a step other than `hide window`, matched by the `step` rule.
absl::StatusOr<PilotStep> ReadStep(const Lexer& lexer, const ParsedItem& item) {
  const std::string_view type = item.GetMatchName();
  const ParsedItem* values = item.GetItem(type);
  const ParsedItem* frames = values->GetItem("frames");
  if (frames != nullptr && frames->GetToken().GetInt() < 0) {
    return ScriptError(lexer, frames->GetToken(), "Frames can't be negative");
  }
  const int frame_count = static_cast<int>(values->GetInt("frames"));

  if (type == "wait") {
    return PilotWaitStep{.frames = frame_count};
  }
  if (type == "wait_for") {
    PilotWaitForStep step = {
        .condition = ReadName(lexer, values->GetToken("condition"))};
    if (frames != nullptr) {
      step.max_frames = frame_count;
    }
    return step;
  }
  if (type == "key") {
    return PilotKeyStep{.key = ReadName(lexer, values->GetToken("key"))};
  }
  if (type == "hold") {
    return PilotHoldStep{.key = ReadName(lexer, values->GetToken("key")),
                         .frames = frame_count};
  }
  if (type == "text") {
    return PilotTextStep{.text = std::string(values->GetString("text"))};
  }
  if (type == "move") {
    return PilotMouseMoveStep{.position = ReadPoint(*values)};
  }
  if (type == "click") {
    PilotMouseClickStep step;
    const std::string_view button = values->GetString("button");
    if (button == "middle") {
      step.button = PilotMouseButton::kMiddle;
    } else if (button == "right") {
      step.button = PilotMouseButton::kRight;
    }
    if (const ParsedItem* label = values->GetItem("label")) {
      step.target = std::string(label->GetToken().GetString());
    } else if (values->GetItem("x") != nullptr) {
      step.target = ReadPoint(*values);
    }
    return step;
  }
  if (type == "wheel") {
    return PilotMouseWheelStep{.amount =
                                   static_cast<int>(values->GetInt("amount"))};
  }
  if (type == "run") {
    PilotRunStep step = {.command =
                             ReadName(lexer, values->GetToken("command"))};
    for (const ParsedItem& arg : values->GetItems("args")) {
      step.args.push_back(ReadName(lexer, arg.GetToken()));
    }
    return step;
  }
  if (type == "expect_error") {
    PilotExpectErrorStep step = {.text =
                                     std::string(values->GetString("text"))};
    if (frames != nullptr) {
      step.max_frames = frame_count;
    }
    return step;
  }
  if (type == "expect") {
    return PilotExpectStep{.condition =
                               ReadName(lexer, values->GetToken("condition"))};
  }
  if (type == "save") {
    return PilotSaveStep{.name = ReadName(lexer, values->GetToken("name"))};
  }
  LOG(DFATAL) << "Unexpected step: " << type;
  return ScriptError(lexer, item.GetToken(), "Unknown step");
}

}  // namespace

absl::StatusOr<PilotScript> ReadPilotScript(std::string text,
                                            std::string_view filename) {
  std::unique_ptr<Parser> parser = Parser::Create(GetScriptProgram());
  DCHECK(parser != nullptr);
  Lexer& lexer = parser->GetLexer();
  const LexerContentId content =
      filename.empty() ? lexer.AddContent(std::move(text))
                       : lexer.AddFileContent(filename, std::move(text));
  if (content == kNoLexerContent) {
    return absl::InvalidArgumentError(absl::StrCat(
        filename.empty() ? "The script" : filename, " has more than ",
        kMaxLexerLines, " lines, or a line over ", kMaxTokensPerLine,
        " characters"));
  }
  PilotScript script;
  while (!lexer.NextToken(content, /*advance=*/false).IsEnd()) {
    ParseResult result = parser->Parse(content, "line");
    if (!result.IsOk()) {
      return absl::InvalidArgumentError(result.GetError().FormatMessage());
    }
    const ParsedItem* item = result->GetItem("step");
    if (item == nullptr) {
      continue;  // A blank line.
    }
    if (item->GetMatchName() == "hide_window") {
      if (!script.steps.empty()) {
        return ScriptError(lexer, item->GetToken(),
                           "\"hide window\" must come before every other step");
      }
      script.hide_window = true;
      continue;
    }
    absl::StatusOr<PilotStep> step = ReadStep(lexer, *item);
    if (!step.ok()) {
      return step.status();
    }
    script.steps.push_back(*std::move(step));
  }
  return script;
}

}  // namespace gb
