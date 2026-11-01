// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_command_line.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gb/cli/cli_help.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Optional;
using ::testing::StartsWith;

bool RunNothing(const CliCommandLine&) { return true; }
bool RunFails(const CliCommandLine&) { return false; }

// Runs `command_line`, which must succeed, and returns any help it writes.
std::string RunForHelp(const CliCommandLine& command_line) {
  std::string help;
  EXPECT_TRUE(command_line.Run(
      [&help](std::string_view text) { absl::StrAppend(&help, text); }));
  return help;
}

absl::Status ValidateNotEmpty(const std::string_view& value) {
  if (value.empty()) {
    return absl::InvalidArgumentError("is empty");
  }
  return absl::OkStatus();
}

absl::Status ValidateNoSpaces(const std::string_view& value) {
  if (value.find(' ') != std::string_view::npos) {
    return absl::InvalidArgumentError("contains a space");
  }
  return absl::OkStatus();
}

//==============================================================================
// A program with flags and positional arguments.
//==============================================================================

const CliFlag<std::string_view> kOutput = {.name = "output", .short_name = 'o'};
const CliFlag<std::string_view> kMode = {.name = "mode",
                                         .default_value = "fast"};
const CliFlag<std::string_view> kLabel = {
    .name = "label",
    .validators = {ValidateNotEmpty, ValidateNoSpaces},
};
const CliFlag<std::string_view> kTitle = {
    .name = "title",
    .default_value = "",
    .validators = {ValidateNotEmpty},
};
const CliFlag<bool> kVerbose = {.name = "verbose", .short_name = 'v'};
const CliFlag<bool> kQuiet = {.name = "quiet", .short_name = 'q'};
const CliArg<std::string_view> kInput = {
    .name = "input file",
    .validators = {ValidateNoSpaces},
};
const CliArg<std::string_view> kExtra = {
    .name = "extra file",
    .optional = true,
    .default_value = "extra.txt",
};
const CliArg<std::string_view> kLast = {.name = "last file", .optional = true};
const CliCommand kConvert = {
    .name = "convert",
    .flags = {&kOutput, &kMode, &kLabel, &kTitle, &kVerbose, &kQuiet},
    .args = {&kInput, &kExtra, &kLast},
    .run = RunNothing,
};

//==============================================================================
// A program with subcommands.
//==============================================================================

const CliFlag<bool> kForce = {.name = "force", .short_name = 'f'};
const CliArg<std::string_view> kName = {.name = "name"};
const CliCommand kAdd = {
    .name = "add",
    .flags = {&kForce},
    .args = {&kName},
    .run = RunNothing,
};
const CliCommand kRemove = {
    .name = "remove",
    .flags = {&kForce},
    .args = {&kName},
    .run = RunNothing,
};
const CliCommand kRemote = {.name = "remote", .commands = {&kAdd, &kRemove}};
const CliCommand kStatus = {.name = "status", .run = RunNothing};
const CliCommand kTool = {
    .name = "tool",
    .flags = {&kVerbose},
    .commands = {&kRemote, &kStatus, &kCliHelpCommand},
    .run = RunNothing,
};

//==============================================================================
// A program with a flag or argument of each value type, and choices.
//==============================================================================

const CliFlag<int64_t> kCount = {.name = "count"};
const CliFlag<int64_t> kLimit = {.name = "limit", .default_value = 10};
const CliFlag<double> kRatio = {.name = "ratio"};
const CliFlag<double> kScale = {.name = "scale", .default_value = 1.5};
const CliFlag<std::string_view> kShape = {
    .name = "shape",
    .choices = {{.name = "circle"}, {.name = "square"}},
    .default_value = "circle",
};
const CliArg<std::string_view> kOutline = {
    .name = "outline",
    .optional = true,
    .choices = {{.name = "circle"}, {.name = "square"}},
};
const CliCommand kTypes = {
    .name = "types",
    .flags = {&kCount, &kLimit, &kRatio, &kScale, &kShape},
    .args = {&kOutline},
    .run = RunNothing,
};

//==============================================================================
// A program with number arguments.
//==============================================================================

const CliFlag<bool> kRelative = {.name = "relative", .short_name = 'r'};
const CliArg<int64_t> kOffset = {.name = "offset"};
const CliArg<double> kFactor = {.name = "factor", .optional = true};
const CliCommand kMove = {
    .name = "move",
    .flags = {&kRelative},
    .args = {&kOffset, &kFactor},
    .run = RunNothing,
};

// Returns the error message from parsing `args`, or an empty string if they
// parse.
std::string ParseError(const CliCommand* program,
                       const std::vector<std::string>& args) {
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(program, args);
  if (command_line.ok()) {
    return "";
  }
  EXPECT_EQ(command_line.status().code(), absl::StatusCode::kInvalidArgument);
  return std::string(command_line.status().message());
}

//==============================================================================
// Flags
//==============================================================================

TEST(ParseCommandLineTest, LongFlagWithValue) {
  const std::vector<std::string> args = {"--output", "out.txt", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput),
              Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, LongFlagWithAttachedValue) {
  const std::vector<std::string> args = {"--output=out.txt", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput),
              Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, AttachedValueWithEquals) {
  const std::vector<std::string> args = {"--output=a=b", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput), Optional(std::string_view("a=b")));
}

TEST(ParseCommandLineTest, AttachedEmptyValue) {
  const std::vector<std::string> args = {"--output=", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput), Optional(std::string_view("")));
}

TEST(ParseCommandLineTest, ShortFlagWithValue) {
  const std::vector<std::string> args = {"-o", "out.txt", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput),
              Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, ValueStartingWithDash) {
  const std::vector<std::string> args = {"--output", "-v", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput), Optional(std::string_view("-v")));
  EXPECT_FALSE(command_line->Get(kVerbose));
}

TEST(ParseCommandLineTest, FlagNotGiven) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->Get(kOutput), std::nullopt);
}

TEST(ParseCommandLineTest, FlagDefault) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kMode), Optional(std::string_view("fast")));
}

TEST(ParseCommandLineTest, DefaultIsNotValidated) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kTitle), Optional(std::string_view("")));
  EXPECT_EQ(ParseError(&kConvert, {"--title=", "in.txt"}),
            "convert: \"--title\": is empty");
}

TEST(ParseCommandLineTest, FlagOverridesDefault) {
  const std::vector<std::string> args = {"--mode", "slow", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kMode), Optional(std::string_view("slow")));
}

TEST(ParseCommandLineTest, BoolFlags) {
  const std::vector<std::string> args = {"--verbose", "-q", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();

  // A bool flag reads as a plain bool, not a std::optional<bool>, which would
  // be true whenever it holds a value.
  static_assert(std::is_same_v<decltype(command_line->Get(kVerbose)), bool>);
  EXPECT_TRUE(command_line->Get(kVerbose));
  EXPECT_TRUE(command_line->Get(kQuiet));
}

TEST(ParseCommandLineTest, BoolFlagNotGiven) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_FALSE(command_line->Get(kVerbose));
}

TEST(ParseCommandLineTest, FlagsBetweenArgs) {
  const std::vector<std::string> args = {"-v", "in.txt", "-o", "out.txt",
                                         "more.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Get(kVerbose));
  EXPECT_THAT(command_line->Get(kOutput),
              Optional(std::string_view("out.txt")));
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("in.txt")));
  EXPECT_THAT(command_line->Get(kExtra),
              Optional(std::string_view("more.txt")));
}

TEST(ParseCommandLineTest, ValidatorsPass) {
  const std::vector<std::string> args = {"--label", "name", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kLabel), Optional(std::string_view("name")));
}

TEST(ParseCommandLineTest, UnknownFlag) {
  EXPECT_EQ(ParseError(&kConvert, {"--nope", "in.txt"}),
            "convert: unknown flag \"--nope\"");
  EXPECT_EQ(ParseError(&kConvert, {"--nope=1", "in.txt"}),
            "convert: unknown flag \"--nope\"");
  EXPECT_EQ(ParseError(&kConvert, {"-n", "in.txt"}),
            "convert: unknown flag \"-n\"");
}

TEST(ParseCommandLineTest, ShortFlagsDontCombine) {
  EXPECT_EQ(ParseError(&kConvert, {"-vq", "in.txt"}),
            "convert: unknown flag \"-vq\"");
}

TEST(ParseCommandLineTest, ShortFlagValueIsNotAttached) {
  EXPECT_EQ(ParseError(&kConvert, {"-o=out.txt", "in.txt"}),
            "convert: unknown flag \"-o=out.txt\"");
}

TEST(ParseCommandLineTest, FlagGivenTwice) {
  EXPECT_EQ(ParseError(&kConvert, {"-v", "--verbose", "in.txt"}),
            "convert: \"--verbose\" was given twice");
  EXPECT_EQ(ParseError(&kConvert, {"-o", "a", "-o", "b", "in.txt"}),
            "convert: \"-o\" was given twice");
}

TEST(ParseCommandLineTest, BoolFlagWithValue) {
  EXPECT_EQ(ParseError(&kConvert, {"--verbose=yes", "in.txt"}),
            "convert: \"--verbose\" doesn't take a value");
}

TEST(ParseCommandLineTest, MissingValue) {
  EXPECT_EQ(ParseError(&kConvert, {"in.txt", "--output"}),
            "convert: \"--output\" needs a value");
}

TEST(ParseCommandLineTest, ValidatorsFail) {
  EXPECT_EQ(ParseError(&kConvert, {"--label=", "in.txt"}),
            "convert: \"--label\": is empty");
  EXPECT_EQ(ParseError(&kConvert, {"--label", "a b", "in.txt"}),
            "convert: \"--label\": contains a space");
}

//==============================================================================
// Positional arguments
//==============================================================================

TEST(ParseCommandLineTest, RequiredArg) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("in.txt")));
}

TEST(ParseCommandLineTest, OptionalArgs) {
  const std::vector<std::string> args = {"in.txt", "more.txt", "last.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kExtra),
              Optional(std::string_view("more.txt")));
  EXPECT_THAT(command_line->Get(kLast), Optional(std::string_view("last.txt")));
}

TEST(ParseCommandLineTest, OptionalArgsNotGiven) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kExtra),
              Optional(std::string_view("extra.txt")));
  EXPECT_EQ(command_line->Get(kLast), std::nullopt);
}

TEST(ParseCommandLineTest, DoubleDashEndsFlags) {
  const std::vector<std::string> args = {"-v", "--", "--quiet", "-o"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Get(kVerbose));
  EXPECT_FALSE(command_line->Get(kQuiet));
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("--quiet")));
  EXPECT_THAT(command_line->Get(kExtra), Optional(std::string_view("-o")));
}

TEST(ParseCommandLineTest, SingleDashIsArg) {
  const std::vector<std::string> args = {"-"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("-")));
}

TEST(ParseCommandLineTest, MissingArg) {
  EXPECT_EQ(ParseError(&kConvert, {}), "convert: missing <input file>");
  EXPECT_EQ(ParseError(&kConvert, {"-v"}), "convert: missing <input file>");
}

TEST(ParseCommandLineTest, UnexpectedArg) {
  EXPECT_EQ(ParseError(&kConvert, {"a", "b", "c", "d"}),
            "convert: unexpected argument \"d\"");
}

TEST(ParseCommandLineTest, ArgValidatorFails) {
  EXPECT_EQ(ParseError(&kConvert, {"in file.txt"}),
            "convert: <input file>: contains a space");
}

//==============================================================================
// Commands
//==============================================================================

TEST(ParseCommandLineTest, Program) {
  const std::vector<std::string> args = {};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->GetProgram(), &kTool);
  EXPECT_EQ(command_line->GetCommand(), &kTool);
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kTool));
}

TEST(ParseCommandLineTest, Subcommand) {
  const std::vector<std::string> args = {"remote", "add", "origin"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->GetProgram(), &kTool);
  EXPECT_EQ(command_line->GetCommand(), &kAdd);
  EXPECT_THAT(command_line->GetCommandPath(),
              ElementsAre(&kTool, &kRemote, &kAdd));
  EXPECT_THAT(command_line->Get(kName), Optional(std::string_view("origin")));
}

TEST(ParseCommandLineTest, ParentFlagBeforeSubcommand) {
  const std::vector<std::string> args = {"--verbose", "remote", "add",
                                         "origin"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Get(kVerbose));
}

TEST(ParseCommandLineTest, ParentFlagAfterSubcommand) {
  const std::vector<std::string> args = {"remote", "add", "origin", "-v", "-f"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Get(kVerbose));
  EXPECT_TRUE(command_line->Get(kForce));
}

TEST(ParseCommandLineTest, SameFlagInSiblingCommands) {
  const std::vector<std::string> args = {"remote", "remove", "-f", "origin"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->GetCommand(), &kRemove);
  EXPECT_TRUE(command_line->Get(kForce));
}

TEST(ParseCommandLineTest, SubcommandFlagBeforeSubcommand) {
  EXPECT_EQ(ParseError(&kTool, {"remote", "--force", "add", "origin"}),
            "tool remote: unknown flag \"--force\"");
}

TEST(ParseCommandLineTest, UnknownCommand) {
  EXPECT_EQ(ParseError(&kTool, {"nope"}), "tool: unknown command \"nope\"");
}

TEST(ParseCommandLineTest, NoCommandAndNoRunFunction) {
  const std::vector<std::string> args = {"remote"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kTool, &kRemote));
  EXPECT_THAT(RunForHelp(*command_line),
              StartsWith("Usage: tool remote [<flags>] <command>\n"));
}

TEST(ParseCommandLineTest, ErrorNamesCommandPath) {
  EXPECT_EQ(ParseError(&kTool, {"remote", "add"}),
            "tool remote add: missing <name>");
}

TEST(ParseCommandLineTest, DoubleDashBeforeCommand) {
  EXPECT_EQ(ParseError(&kTool, {"--", "status"}),
            "tool: unexpected argument \"status\"");
}

//==============================================================================
// Help
//==============================================================================

TEST(CliCommandLineTest, RunRunsTheCommand) {
  const std::vector<std::string> args = {"status"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(RunForHelp(*command_line), "");
}

TEST(CliCommandLineTest, RunReturnsWhetherTheCommandSucceeded) {
  static const CliCommand kFails = {.name = "fails", .run = RunFails};
  const std::vector<std::string> args = {};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kFails, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_FALSE(command_line->Run(
      [](std::string_view text) { ADD_FAILURE() << "Wrote help: " << text; }));
}

TEST(CliCommandLineTest, RunWritesHelpForACommandWithNoRunFunction) {
  static const CliCommand kNoRun = {.name = "prog"};
  const std::vector<std::string> args = {};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kNoRun, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(RunForHelp(*command_line), StartsWith("Usage: prog [<flags>]\n"));
}

TEST(ParseCommandLineTest, HelpFlag) {
  for (const std::string_view flag : {"--help", "-h"}) {
    // The help is for the command so far, and needs nothing that is missing.
    const std::vector<std::string> args = {"remote", "add", std::string(flag)};
    absl::StatusOr<CliCommandLine> command_line =
        ParseCommandLine(&kTool, args);
    ASSERT_TRUE(command_line.ok()) << command_line.status();
    EXPECT_THAT(command_line->GetCommandPath(),
                ElementsAre(&kTool, &kRemote, &kAdd));
    EXPECT_THAT(RunForHelp(*command_line),
                StartsWith("Usage: tool remote add [<flags>] <name>\n"));
  }
}

TEST(ParseCommandLineTest, HelpFlagStopsParsing) {
  const std::vector<std::string> args = {"-v", "-h", "nope", "--nope", "-v"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kTool));
  EXPECT_THAT(RunForHelp(*command_line), StartsWith("Usage: tool [<flags>]"));
}

TEST(ParseCommandLineTest, ErrorBeforeHelpFlag) {
  EXPECT_EQ(ParseError(&kTool, {"--nope", "--help"}),
            "tool: unknown flag \"--nope\"");
  EXPECT_EQ(ParseError(&kTool, {"nope", "--help"}),
            "tool: unknown command \"nope\"");
}

TEST(ParseCommandLineTest, HelpFlagWithValue) {
  EXPECT_EQ(ParseError(&kTool, {"--help=yes"}),
            "tool: \"--help\" doesn't take a value");
}

TEST(ParseCommandLineTest, HelpFlagAfterDoubleDash) {
  const std::vector<std::string> args = {"--", "--help"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("--help")));
  EXPECT_EQ(RunForHelp(*command_line), "");
}

TEST(ParseCommandLineTest, HelpCommand) {
  const std::vector<std::string> args = {"help", "remote", "add"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(),
              ElementsAre(&kTool, &kRemote, &kAdd));
  EXPECT_THAT(RunForHelp(*command_line),
              StartsWith("Usage: tool remote add [<flags>] <name>\n"));
}

TEST(ParseCommandLineTest, HelpCommandAlone) {
  const std::vector<std::string> args = {"-v", "help"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kTool));
  EXPECT_THAT(RunForHelp(*command_line), StartsWith("Usage: tool [<flags>]"));
}

TEST(ParseCommandLineTest, HelpForHelpCommand) {
  const std::vector<std::string> args = {"help", "help"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(),
              ElementsAre(&kTool, &kCliHelpCommand));
  EXPECT_THAT(RunForHelp(*command_line),
              StartsWith("Usage: tool help [<flags>]"));
}

TEST(ParseCommandLineTest, HelpCommandOnlyNamesCommands) {
  EXPECT_EQ(ParseError(&kTool, {"help", "nope"}),
            "tool: unknown command \"nope\"");
  EXPECT_EQ(ParseError(&kTool, {"help", "remote", "add", "origin"}),
            "tool remote add: unknown command \"origin\"");
}

//==============================================================================
// Value types
//==============================================================================

TEST(ParseCommandLineTest, Integers) {
  const std::vector<std::string> args = {"--count", "-12", "--limit", "+5"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kCount), Optional(-12));
  EXPECT_THAT(command_line->Get(kLimit), Optional(5));
}

TEST(ParseCommandLineTest, IntegerDefault) {
  const std::vector<std::string> args = {};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->Get(kCount), std::nullopt);
  EXPECT_THAT(command_line->Get(kLimit), Optional(10));
}

TEST(ParseCommandLineTest, IntegerLimits) {
  const std::vector<std::string> args = {"--count=9223372036854775807",
                                         "--limit=-9223372036854775808"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kCount),
              Optional(std::numeric_limits<int64_t>::max()));
  EXPECT_THAT(command_line->Get(kLimit),
              Optional(std::numeric_limits<int64_t>::min()));
}

TEST(ParseCommandLineTest, NotAnInteger) {
  EXPECT_EQ(ParseError(&kTypes, {"--count=abc"}),
            "types: \"--count\": \"abc\" is not an integer");
  EXPECT_EQ(ParseError(&kTypes, {"--count=1.5"}),
            "types: \"--count\": \"1.5\" is not an integer");
  EXPECT_EQ(ParseError(&kTypes, {"--count=0x10"}),
            "types: \"--count\": \"0x10\" is not an integer");
  EXPECT_EQ(ParseError(&kTypes, {"--count= 5"}),
            "types: \"--count\": \" 5\" is not an integer");
  EXPECT_EQ(ParseError(&kTypes, {"--count=-"}),
            "types: \"--count\": \"-\" is not an integer");
  EXPECT_EQ(ParseError(&kTypes, {"--limit="}),
            "types: \"--limit\": \"\" is not an integer");
}

TEST(ParseCommandLineTest, IntegerOutOfRange) {
  EXPECT_EQ(ParseError(&kTypes, {"--count=9223372036854775808"}),
            "types: \"--count\": \"9223372036854775808\" is out of range");
  EXPECT_EQ(ParseError(&kTypes, {"--count=-9223372036854775809"}),
            "types: \"--count\": \"-9223372036854775809\" is out of range");
}

TEST(ParseCommandLineTest, FloatingPoint) {
  const std::vector<std::string> args = {"--ratio", "0.25", "--scale", "-1e3"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kRatio), Optional(0.25));
  EXPECT_THAT(command_line->Get(kScale), Optional(-1000.0));
}

TEST(ParseCommandLineTest, FloatingPointDefault) {
  const std::vector<std::string> args = {};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->Get(kRatio), std::nullopt);
  EXPECT_THAT(command_line->Get(kScale), Optional(1.5));
}

TEST(ParseCommandLineTest, NotANumber) {
  EXPECT_EQ(ParseError(&kTypes, {"--ratio=abc"}),
            "types: \"--ratio\": \"abc\" is not a number");
  EXPECT_EQ(ParseError(&kTypes, {"--ratio=1.5x"}),
            "types: \"--ratio\": \"1.5x\" is not a number");
  EXPECT_EQ(ParseError(&kTypes, {"--scale=1.5 "}),
            "types: \"--scale\": \"1.5 \" is not a number");
  EXPECT_EQ(ParseError(&kTypes, {"--scale="}),
            "types: \"--scale\": \"\" is not a number");
}

TEST(ParseCommandLineTest, NotAFiniteNumber) {
  EXPECT_EQ(ParseError(&kTypes, {"--scale=inf"}),
            "types: \"--scale\": \"inf\" is not a finite number");
  EXPECT_EQ(ParseError(&kTypes, {"--scale=nan"}),
            "types: \"--scale\": \"nan\" is not a finite number");
  EXPECT_EQ(ParseError(&kTypes, {"--scale=1e999"}),
            "types: \"--scale\": \"1e999\" is out of range");
  EXPECT_EQ(ParseError(&kTypes, {"--ratio=-1e999"}),
            "types: \"--ratio\": \"-1e999\" is out of range");
}

TEST(ParseCommandLineTest, NumberArgs) {
  const std::vector<std::string> args = {"12", "2.5"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kMove, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOffset), Optional(12));
  EXPECT_THAT(command_line->Get(kFactor), Optional(2.5));
}

TEST(ParseCommandLineTest, NegativeNumberArgs) {
  const std::vector<std::string> args = {"-r", "-5", "-.5"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kMove, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Get(kRelative));
  EXPECT_THAT(command_line->Get(kOffset), Optional(-5));
  EXPECT_THAT(command_line->Get(kFactor), Optional(-0.5));
}

TEST(ParseCommandLineTest, NegativeNumberLooksLikeAnArg) {
  // A word starting with "-" and a digit is a word whatever the argument's
  // type, so it is reported as what it is for.
  EXPECT_EQ(ParseError(&kMove, {"-5", "-1x"}),
            "move: <factor>: \"-1x\" is not a number");
  EXPECT_EQ(ParseError(&kMove, {"1", "2", "-3"}),
            "move: unexpected argument \"-3\"");
  EXPECT_EQ(ParseError(&kTool, {"-1"}), "tool: unknown command \"-1\"");

  const std::vector<std::string> args = {"-5"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("-5")));
}

TEST(ParseCommandLineTest, Choices) {
  const std::vector<std::string> args = {"--shape", "square", "circle"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kShape), Optional(std::string_view("square")));
  EXPECT_THAT(command_line->Get(kOutline),
              Optional(std::string_view("circle")));
}

TEST(ParseCommandLineTest, ChoiceDefault) {
  const std::vector<std::string> args = {};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kShape), Optional(std::string_view("circle")));
  EXPECT_EQ(command_line->Get(kOutline), std::nullopt);
}

TEST(ParseCommandLineTest, NotAChoice) {
  EXPECT_EQ(ParseError(&kTypes, {"--shape=triangle"}),
            "types: \"--shape\": \"triangle\" is not one of: circle, square");
  EXPECT_EQ(ParseError(&kTypes, {"Square"}),
            "types: <outline>: \"Square\" is not one of: circle, square");
}

//==============================================================================
// Programming errors
//==============================================================================

TEST(ParseCommandLineDeathTest, InvalidProgram) {
  static const CliCommand kInvalid = {
      .name = "prog",
      .flags = {&kVerbose, &kVerbose},
  };
  const std::vector<std::string> args = {};
  EXPECT_DEATH(ParseCommandLine(&kInvalid, args).status().IgnoreError(),
               "duplicate flag");
}

TEST(ParseCommandLineDeathTest, GetOutsideCommandPath) {
  const std::vector<std::string> args = {"status"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_DEATH(command_line->Get(kForce), "doesn't belong");
  EXPECT_DEATH(command_line->Get(kName), "doesn't belong");
}

TEST(ParseCommandLineDeathTest, GetForHelp) {
  const std::vector<std::string> args = {"-v", "--help"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_DEATH(command_line->Get(kVerbose), "asks for help");
}

}  // namespace
}  // namespace gb
