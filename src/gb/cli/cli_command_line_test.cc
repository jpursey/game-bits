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

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/strip.h"
#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli_help.h"
#include "gb/cli/cli_writer.h"
#include "gb/cli/fake_cli_writer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Optional;
using ::testing::StartsWith;

bool RunNothing(ValidatedContext&, const CliCommandLine&) { return true; }
bool RunFails(ValidatedContext&, const CliCommandLine&) { return false; }

bool RunWithAMistake(ValidatedContext& context, const CliCommandLine&) {
  context.SetValue<std::string>(kKeyCliUsageError,
                                "<input> and <output> are the same file");
  return false;
}
bool RunSucceedsWithAMistake(ValidatedContext& context, const CliCommandLine&) {
  context.SetValue<std::string>(kKeyCliUsageError,
                                "<input> and <output> are the same file");
  return true;
}

// Parses command lines with a context, which holds a writer that keeps what is
// written to it, and keeps the errors ValidatedContext reports.
class CliCommandLineRunTest : public ::testing::Test {
 protected:
  CliCommandLineRunTest() {
    context_.SetValue<CliWriter>(writer_.GetWriter());
    ValidatedContext::SetGlobalErrorCallback(
        [this](const std::string& message) {
          context_errors_.push_back(message);
        });
  }
  ~CliCommandLineRunTest() override {
    ValidatedContext::SetGlobalErrorCallback(nullptr);
  }

  absl::StatusOr<CliCommandLine> Parse(
      const CliCommand* program, const std::vector<std::string_view>& args) {
    return ParseCommandLine(&context_, program, args);
  }

  // Runs `command_line`, which must succeed, and returns any help it writes.
  std::string RunForHelp(CliCommandLine& command_line) {
    EXPECT_TRUE(command_line.Run());
    return writer_.TakeHelp();
  }

  // The writer is first, as the context's CliWriter refers to it.
  FakeCliWriter writer_;
  Context context_;

  // The errors ValidatedContext has reported.
  std::vector<std::string> context_errors_;
};

absl::Status ValidateNotEmpty(const ValidatedContext&,
                              const std::string_view& value) {
  if (value.empty()) {
    return absl::InvalidArgumentError("is empty");
  }
  return absl::OkStatus();
}

absl::Status ValidateNoSpaces(const ValidatedContext&,
                              const std::string_view& value) {
  if (value.find(' ') != std::string_view::npos) {
    return absl::InvalidArgumentError("contains a space");
  }
  return absl::OkStatus();
}

const CliValidator<std::string_view> kNotEmpty = {.validate = ValidateNotEmpty};
const CliValidator<std::string_view> kNoSpaces = {.validate = ValidateNoSpaces};

//==============================================================================
// A program with flags and positional arguments.
//==============================================================================

const CliFlag<std::string_view> kOutput = {.name = "output", .short_name = 'o'};
const CliFlag<std::string_view> kMode = {.name = "mode",
                                         .default_value = "fast"};
const CliFlag<std::string_view> kLabel = {
    .name = "label",
    .validators = {&kNotEmpty, &kNoSpaces},
};
const CliFlag<std::string_view> kTitle = {
    .name = "title",
    .default_value = "",
    .validators = {&kNotEmpty},
};
const CliFlag<bool> kVerbose = {.name = "verbose", .short_name = 'v'};
const CliFlag<bool> kQuiet = {.name = "quiet", .short_name = 'q'};
const CliArg<std::string_view> kInput = {
    .name = "input file",
    .validators = {&kNoSpaces},
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

// Returns the error message from parsing `args`, without the hint for help
// that ends every one, or an empty string if they parse. The hint must be for
// the command the error is for, which the message starts with.
std::string ParseError(const CliCommand* program,
                       const std::vector<std::string_view>& args) {
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(program, args);
  if (command_line.ok()) {
    return "";
  }
  EXPECT_EQ(command_line.status().code(), absl::StatusCode::kInvalidArgument);
  std::string_view message = command_line.status().message();
  const std::string_view path_name = message.substr(0, message.find(':'));
  EXPECT_TRUE(absl::ConsumeSuffix(
      &message, absl::StrCat(". Run \"", path_name, " --help\" for help.")))
      << message;
  return std::string(message);
}

//==============================================================================
// Flags
//==============================================================================

TEST(ParseCommandLineTest, LongFlagWithValue) {
  const std::vector<std::string_view> args = {"--output", "out.txt", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput),
              Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, LongFlagWithAttachedValue) {
  const std::vector<std::string_view> args = {"--output=out.txt", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput),
              Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, AttachedValueWithEquals) {
  const std::vector<std::string_view> args = {"--output=a=b", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput), Optional(std::string_view("a=b")));
}

TEST(ParseCommandLineTest, AttachedEmptyValue) {
  const std::vector<std::string_view> args = {"--output=", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput), Optional(std::string_view("")));
}

TEST(ParseCommandLineTest, ShortFlagWithValue) {
  const std::vector<std::string_view> args = {"-o", "out.txt", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput),
              Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, ValueStartingWithDash) {
  const std::vector<std::string_view> args = {"--output", "-v", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOutput), Optional(std::string_view("-v")));
  EXPECT_FALSE(command_line->Get(kVerbose));
}

TEST(ParseCommandLineTest, FlagNotGiven) {
  const std::vector<std::string_view> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->Get(kOutput), std::nullopt);
}

TEST(ParseCommandLineTest, FlagDefault) {
  const std::vector<std::string_view> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kMode), Optional(std::string_view("fast")));
}

TEST(ParseCommandLineTest, DefaultIsNotValidated) {
  const std::vector<std::string_view> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kTitle), Optional(std::string_view("")));
  EXPECT_EQ(ParseError(&kConvert, {"--title=", "in.txt"}),
            "convert: \"--title\": is empty");
}

TEST(ParseCommandLineTest, FlagOverridesDefault) {
  const std::vector<std::string_view> args = {"--mode", "slow", "in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kMode), Optional(std::string_view("slow")));
}

TEST(ParseCommandLineTest, BoolFlags) {
  const std::vector<std::string_view> args = {"--verbose", "-q", "in.txt"};
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
  const std::vector<std::string_view> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_FALSE(command_line->Get(kVerbose));
}

TEST(ParseCommandLineTest, FlagsBetweenArgs) {
  const std::vector<std::string_view> args = {"-v", "in.txt", "-o", "out.txt",
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
  const std::vector<std::string_view> args = {"--label", "name", "in.txt"};
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
  const std::vector<std::string_view> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("in.txt")));
}

TEST(ParseCommandLineTest, OptionalArgs) {
  const std::vector<std::string_view> args = {"in.txt", "more.txt", "last.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kExtra),
              Optional(std::string_view("more.txt")));
  EXPECT_THAT(command_line->Get(kLast), Optional(std::string_view("last.txt")));
}

TEST(ParseCommandLineTest, OptionalArgsNotGiven) {
  const std::vector<std::string_view> args = {"in.txt"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kExtra),
              Optional(std::string_view("extra.txt")));
  EXPECT_EQ(command_line->Get(kLast), std::nullopt);
}

TEST(ParseCommandLineTest, DoubleDashEndsFlags) {
  const std::vector<std::string_view> args = {"-v", "--", "--quiet", "-o"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Get(kVerbose));
  EXPECT_FALSE(command_line->Get(kQuiet));
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("--quiet")));
  EXPECT_THAT(command_line->Get(kExtra), Optional(std::string_view("-o")));
}

TEST(ParseCommandLineTest, SingleDashIsArg) {
  const std::vector<std::string_view> args = {"-"};
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
  const std::vector<std::string_view> args = {};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->GetProgram(), &kTool);
  EXPECT_EQ(command_line->GetCommand(), &kTool);
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kTool));
}

TEST(ParseCommandLineTest, Subcommand) {
  const std::vector<std::string_view> args = {"remote", "add", "origin"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->GetProgram(), &kTool);
  EXPECT_EQ(command_line->GetCommand(), &kAdd);
  EXPECT_THAT(command_line->GetCommandPath(),
              ElementsAre(&kTool, &kRemote, &kAdd));
  EXPECT_THAT(command_line->Get(kName), Optional(std::string_view("origin")));
}

TEST(ParseCommandLineTest, ParentFlagBeforeSubcommand) {
  const std::vector<std::string_view> args = {"--verbose", "remote", "add",
                                              "origin"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Get(kVerbose));
}

TEST(ParseCommandLineTest, ParentFlagAfterSubcommand) {
  const std::vector<std::string_view> args = {"remote", "add", "origin", "-v",
                                              "-f"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Get(kVerbose));
  EXPECT_TRUE(command_line->Get(kForce));
}

TEST(ParseCommandLineTest, SameFlagInSiblingCommands) {
  const std::vector<std::string_view> args = {"remote", "remove", "-f",
                                              "origin"};
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

TEST_F(CliCommandLineRunTest, NoCommandAndNoRunFunction) {
  const std::vector<std::string_view> args = {"remote"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kTool, &kRemote));
  EXPECT_THAT(RunForHelp(*command_line),
              StartsWith("Usage: tool remote [<flags>] <command>\n"));
}

TEST(ParseCommandLineTest, ErrorNamesCommandPath) {
  EXPECT_EQ(ParseError(&kTool, {"remote", "add"}),
            "tool remote add: missing <name>");
}

TEST(ParseCommandLineTest, ErrorSaysHowToGetHelp) {
  const std::vector<std::string_view> args = {"remote", "add", "--nope"};
  EXPECT_EQ(ParseCommandLine(&kTool, args).status().message(),
            "tool remote add: unknown flag \"--nope\". Run \"tool remote add "
            "--help\" for help.");
}

TEST(ParseCommandLineTest, DoubleDashBeforeCommand) {
  EXPECT_EQ(ParseError(&kTool, {"--", "status"}),
            "tool: unexpected argument \"status\"");
}

//==============================================================================
// Arguments before subcommands, and required flags
//==============================================================================

const CliFlag<int64_t> kSize = {.name = "size", .optional = false};
const CliArg<std::string_view> kImage = {.name = "image"};
const CliArg<std::string_view> kSide = {.name = "side"};
const CliArg<std::string_view> kVolume = {.name = "volume"};
const CliCommand kFormat = {
    .name = "format",
    .flags = {&kSize},
    .args = {&kVolume},
    .run = RunNothing,
};
const CliCommand kList = {.name = "list", .run = RunNothing};
const CliCommand kDisk = {
    .name = "disk",
    .flags = {&kVerbose},
    .args = {&kImage, &kSide},
    .commands = {&kFormat, &kList, &kCliHelpCommand},
};

TEST(ParseCommandLineTest, ArgsBeforeSubcommand) {
  const std::vector<std::string_view> args = {"work.ozd", "a",    "-v",
                                              "format",   "Work", "--size=5"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kDisk, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kDisk, &kFormat));
  EXPECT_THAT(command_line->Get(kImage),
              Optional(std::string_view("work.ozd")));
  EXPECT_THAT(command_line->Get(kSide), Optional(std::string_view("a")));
  EXPECT_THAT(command_line->Get(kVolume), Optional(std::string_view("Work")));
  EXPECT_THAT(command_line->Get(kSize), Optional(5));
  EXPECT_TRUE(command_line->Get(kVerbose));
}

TEST(ParseCommandLineTest, ArgsAreNeverSubcommands) {
  const std::vector<std::string_view> args = {"list", "format", "list"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kDisk, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kDisk, &kList));
  EXPECT_THAT(command_line->Get(kImage), Optional(std::string_view("list")));
  EXPECT_THAT(command_line->Get(kSide), Optional(std::string_view("format")));
}

TEST(ParseCommandLineTest, ArgsWithoutSubcommand) {
  const std::vector<std::string_view> args = {"work.ozd", "a"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kDisk, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kDisk));
  EXPECT_THAT(command_line->Get(kSide), Optional(std::string_view("a")));
}

TEST(ParseCommandLineTest, MissingArgBeforeSubcommand) {
  EXPECT_EQ(ParseError(&kDisk, {"work.ozd"}), "disk: missing <side>");
}

TEST(ParseCommandLineTest, UnknownCommandAfterArgs) {
  EXPECT_EQ(ParseError(&kDisk, {"work.ozd", "a", "nope"}),
            "disk: unknown command \"nope\"");
}

TEST(ParseCommandLineTest, DoubleDashBeforeArgsAndCommand) {
  EXPECT_EQ(ParseError(&kDisk, {"--", "work.ozd", "a", "list"}),
            "disk: unexpected argument \"list\"");
}

TEST(ParseCommandLineTest, HintForHelpShowsArgsBeforeSubcommand) {
  const std::vector<std::string_view> args = {"work.ozd", "a", "format",
                                              "--nope"};
  EXPECT_EQ(ParseCommandLine(&kDisk, args).status().message(),
            "disk format: unknown flag \"--nope\". Run \"disk <image> <side> "
            "format --help\" for help.");
}

TEST_F(CliCommandLineRunTest, HelpCommandAfterArgs) {
  const std::vector<std::string_view> args = {"work.ozd", "a", "help",
                                              "format"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kDisk, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kDisk, &kFormat));
  EXPECT_THAT(
      RunForHelp(*command_line),
      StartsWith("Usage: disk <image> <side> format [<flags>] <volume>\n"));
}

TEST(ParseCommandLineTest, MissingRequiredFlag) {
  const std::vector<std::string_view> args = {"work.ozd", "a", "format",
                                              "Work"};
  EXPECT_EQ(ParseCommandLine(&kDisk, args).status().message(),
            "disk format: missing \"--size\". Run \"disk <image> <side> "
            "format --help\" for help.");
}

TEST(ParseCommandLineTest, MissingRequiredFlagOfParent) {
  static const CliFlag<std::string_view> kDrive = {.name = "drive",
                                                   .optional = false};
  static const CliCommand kProgram = {
      .name = "prog",
      .flags = {&kDrive},
      .commands = {&kList},
  };
  EXPECT_EQ(ParseError(&kProgram, {"list"}), "prog list: missing \"--drive\"");
  const std::vector<std::string_view> args = {"list", "--drive", "a"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kProgram, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kDrive), Optional(std::string_view("a")));
}

TEST_F(CliCommandLineRunTest, RequiredFlagNotNeededForHelp) {
  const std::vector<std::string_view> args = {"work.ozd", "a", "format",
                                              "--help"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kDisk, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(RunForHelp(*command_line),
              StartsWith("Usage: disk <image> <side> format [<flags>]"));
}

//==============================================================================
// Help
//==============================================================================

TEST_F(CliCommandLineRunTest, RunRunsTheCommand) {
  const std::vector<std::string_view> args = {"status"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(RunForHelp(*command_line), "");
}

TEST_F(CliCommandLineRunTest, RunReturnsWhetherTheCommandSucceeded) {
  static const CliCommand kFails = {.name = "fails", .run = RunFails};
  const std::vector<std::string_view> args = {};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kFails, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_FALSE(command_line->Run());
  EXPECT_EQ(writer_.TakeHelp(), "");
}

TEST_F(CliCommandLineRunTest, RunWritesAUsageError) {
  static const CliCommand kCopy = {.name = "copy", .run = RunWithAMistake};
  static const CliCommand kProgram = {.name = "prog", .commands = {&kCopy}};
  const std::vector<std::string_view> args = {"copy"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kProgram, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_FALSE(command_line->Run());
  EXPECT_EQ(writer_.TakeError(),
            "prog copy: <input> and <output> are the same file. Run \"prog "
            "copy --help\" for help.");
  EXPECT_EQ(writer_.TakeHelp(), "");
}

TEST_F(CliCommandLineRunTest, RunFailsWithAUsageErrorEvenIfTheCommandSucceeds) {
  static const CliCommand kProgram = {.name = "prog",
                                      .run = RunSucceedsWithAMistake};
  const std::vector<std::string_view> args = {};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kProgram, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_FALSE(command_line->Run());
  EXPECT_EQ(writer_.TakeError(),
            "prog: <input> and <output> are the same file. Run \"prog --help\" "
            "for help.");
}

TEST_F(CliCommandLineRunTest, RunClearsAPreviousUsageError) {
  context_.SetValue<std::string>(kKeyCliUsageError, "an old mistake");
  const std::vector<std::string_view> args = {"status"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Run());
  EXPECT_FALSE(context_.Exists<std::string>(kKeyCliUsageError));
  EXPECT_EQ(writer_.TakeError(), "");
}

TEST_F(CliCommandLineRunTest, RunWritesHelpForACommandWithNoRunFunction) {
  static const CliCommand kNoRun = {.name = "prog"};
  const std::vector<std::string_view> args = {};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kNoRun, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(RunForHelp(*command_line), StartsWith("Usage: prog [<flags>]\n"));
}

TEST_F(CliCommandLineRunTest, RunWrapsHelpToTheContextsWidth) {
  constexpr int kWidth = 40;
  context_.SetValue<int>(kKeyCliHelpWidth, kWidth);
  const std::vector<std::string_view> args = {"--help"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  const std::string help =
      FormatCliHelp(command_line->GetCommandPath(), kWidth);
  EXPECT_NE(help, FormatCliHelp(command_line->GetCommandPath(),
                                kDefaultCliHelpWidth));
  EXPECT_EQ(RunForHelp(*command_line), help);
}

TEST_F(CliCommandLineRunTest, HelpFlag) {
  for (const std::string_view flag : {"--help", "-h"}) {
    // The help is for the command so far, and needs nothing that is missing.
    const std::vector<std::string_view> args = {"remote", "add", flag};
    absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
    ASSERT_TRUE(command_line.ok()) << command_line.status();
    EXPECT_THAT(command_line->GetCommandPath(),
                ElementsAre(&kTool, &kRemote, &kAdd));
    EXPECT_THAT(RunForHelp(*command_line),
                StartsWith("Usage: tool remote add [<flags>] <name>\n"));
  }
}

TEST_F(CliCommandLineRunTest, HelpFlagStopsParsing) {
  const std::vector<std::string_view> args = {"-v", "-h", "nope", "--nope",
                                              "-v"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
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

TEST_F(CliCommandLineRunTest, HelpFlagAfterDoubleDash) {
  const std::vector<std::string_view> args = {"--", "--help"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("--help")));
  EXPECT_EQ(RunForHelp(*command_line), "");
}

TEST_F(CliCommandLineRunTest, HelpCommand) {
  const std::vector<std::string_view> args = {"help", "remote", "add"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(),
              ElementsAre(&kTool, &kRemote, &kAdd));
  EXPECT_THAT(RunForHelp(*command_line),
              StartsWith("Usage: tool remote add [<flags>] <name>\n"));
}

TEST_F(CliCommandLineRunTest, HelpCommandAlone) {
  const std::vector<std::string_view> args = {"-v", "help"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->GetCommandPath(), ElementsAre(&kTool));
  EXPECT_THAT(RunForHelp(*command_line), StartsWith("Usage: tool [<flags>]"));
}

TEST_F(CliCommandLineRunTest, HelpForHelpCommand) {
  const std::vector<std::string_view> args = {"help", "help"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
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
// Context
//==============================================================================

// A value a program's commands read from the context.
struct Settings {
  int level = 0;
};

GB_CONTEXT_CONSTRAINT(kConstraintSettings, kInRequired, Settings);
GB_CONTEXT_CONSTRAINT_NAMED(kConstraintResult, kOutRequired, int, "result");

// Returns whether the context holds Settings with level 5, and that this runs
// for kInner.
bool RunInner(ValidatedContext& context, const CliCommandLine& command_line);

const CliCommand kInner = {
    .name = "inner",
    .constraints = {&kConstraintResult},
    .run = RunInner,
};
const CliCommand kOuter = {
    .name = "outer",
    .commands = {&kInner},
    .constraints = {&kConstraintSettings},
};

bool RunInner(ValidatedContext& context, const CliCommandLine& command_line) {
  if (command_line.GetCommand() != &kInner) {
    ADD_FAILURE() << "Ran for " << command_line.GetCommand()->name;
    return false;
  }

  // kOuter's constraint allows reading Settings, and kInner's allows (and
  // requires) setting the result.
  const Settings* settings = context.GetPtr<Settings>();
  context.SetValue<int>("result", 42);
  return settings != nullptr && settings->level == 5;
}

TEST(ParseCommandLineTest, AddsTheDefaults) {
  Context context;
  const std::vector<std::string_view> args = {"status"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&context, &kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(context.Exists<CliWriter>());
  EXPECT_EQ(context.GetValue<int>(kKeyCliHelpWidth), kDefaultCliHelpWidth);
}

TEST_F(CliCommandLineRunTest, RunGetsTheContext) {
  Settings settings = {.level = 5};
  context_.SetPtr<Settings>(&settings);
  const std::vector<std::string_view> args = {"inner"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kOuter, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_TRUE(command_line->Run());
  EXPECT_EQ(context_.GetValue<int>("result"), 42);
}

TEST_F(CliCommandLineRunTest, RunWithoutARequiredValue) {
  const std::vector<std::string_view> args = {"inner"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kOuter, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_FALSE(command_line->Run());
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr("Settings")));
  EXPECT_FALSE(context_.Exists<int>("result"));
}

TEST_F(CliCommandLineRunTest, RunWithoutAValueTheCommandAdds) {
  static const CliCommand kForgets = {
      .name = "forgets",
      .constraints = {&kConstraintResult},
      .run = RunNothing,
  };
  const std::vector<std::string_view> args = {};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kForgets, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_FALSE(command_line->Run());
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr("result")));
}

TEST_F(CliCommandLineRunTest, ParseWithAnInvalidContext) {
  // A width that isn't an int fails kConstraintCliHelpWidth.
  context_.SetValue<int64_t>(kKeyCliHelpWidth, 40);
  const std::vector<std::string_view> args = {"status"};
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTool, args);
  EXPECT_TRUE(absl::IsFailedPrecondition(command_line.status()))
      << command_line.status();
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr(kKeyCliHelpWidth)));
}

//==============================================================================
// Validators that read the context
//==============================================================================

// Validates that a number is at most the Settings level in the context, which
// kConstraintSettings requires. A negative level is the program's mistake.
absl::Status ValidateAtMostLevel(const ValidatedContext& context,
                                 const int64_t& value) {
  const Settings* settings = context.GetPtr<Settings>();
  if (settings->level < 0) {
    return absl::FailedPreconditionError("the level is negative");
  }
  if (value > settings->level) {
    return absl::InvalidArgumentError(
        absl::StrCat(value, " is more than level ", settings->level));
  }
  return absl::OkStatus();
}

const CliValidator<int64_t> kAtMostLevel = {
    .validate = ValidateAtMostLevel,
    .constraints = {&kConstraintSettings},
};

const CliFlag<int64_t> kDepth = {.name = "depth",
                                 .validators = {&kAtMostLevel}};
const CliArg<int64_t> kAmount = {.name = "amount",
                                 .validators = {&kAtMostLevel}};
const CliCommand kLeaf = {
    .name = "leaf", .args = {&kAmount}, .run = RunNothing};
const CliCommand kTree = {
    .name = "tree",
    .flags = {&kDepth},
    .commands = {&kLeaf},
    .run = RunNothing,
};

TEST_F(CliCommandLineRunTest, ValidatorsReadTheContext) {
  Settings settings = {.level = 5};
  context_.SetPtr<Settings>(&settings);
  EXPECT_TRUE(Parse(&kTree, {"--depth", "5", "leaf", "5"}).ok());

  // A flag of the command, one of the command above it, and an argument.
  EXPECT_THAT(Parse(&kTree, {"--depth", "6"}).status().message(),
              StartsWith("tree: \"--depth\": 6 is more than level 5."));
  EXPECT_THAT(Parse(&kTree, {"leaf", "--depth", "6", "5"}).status().message(),
              StartsWith("tree leaf: \"--depth\": 6 is more than level 5."));
  EXPECT_THAT(Parse(&kTree, {"leaf", "6"}).status().message(),
              StartsWith("tree leaf: <amount>: 6 is more than level 5."));
}

TEST_F(CliCommandLineRunTest, ValidatorsReportTheProgramsMistakes) {
  Settings settings = {.level = -1};
  context_.SetPtr<Settings>(&settings);

  // The error doesn't say how to get help, which is no use to the user.
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTree, {"--depth", "1"});
  EXPECT_TRUE(absl::IsFailedPrecondition(command_line.status()))
      << command_line.status();
  EXPECT_EQ(command_line.status().message(),
            "tree: \"--depth\": the level is negative");
  command_line = Parse(&kTree, {"leaf", "1"});
  EXPECT_TRUE(absl::IsFailedPrecondition(command_line.status()))
      << command_line.status();
  EXPECT_EQ(command_line.status().message(),
            "tree leaf: <amount>: the level is negative");
}

TEST_F(CliCommandLineRunTest, ValidatorsWithoutAValueTheyRead) {
  // The command's validators need Settings, though "--depth" isn't given.
  absl::StatusOr<CliCommandLine> command_line = Parse(&kTree, {});
  EXPECT_TRUE(absl::IsFailedPrecondition(command_line.status()))
      << command_line.status();
  EXPECT_EQ(command_line.status().message(),
            "tree: the context is missing a value that the validators of its "
            "flags or arguments read");
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr("Settings")));
}

// Reads the Settings in the context without declaring them.
absl::Status ValidateReadsUndeclared(const ValidatedContext& context,
                                     const int64_t&) {
  if (context.GetPtr<Settings>() == nullptr) {
    return absl::InvalidArgumentError("can't read the settings");
  }
  return absl::OkStatus();
}

const CliValidator<int64_t> kReadsUndeclared = {
    .validate = ValidateReadsUndeclared,
};

const CliFlag<int64_t> kBreadth = {.name = "breadth",
                                   .validators = {&kReadsUndeclared}};
const CliCommand kBush = {
    .name = "bush",
    .flags = {&kDepth, &kBreadth},
    .run = RunNothing,
};

TEST_F(CliCommandLineRunTest, ValidatorsOnlyReadWhatTheyDeclare) {
  Settings settings = {.level = 5};
  context_.SetPtr<Settings>(&settings);

  // "--depth"'s validator declares Settings, but "--breadth"'s doesn't.
  EXPECT_TRUE(Parse(&kBush, {"--depth", "5"}).ok());
  EXPECT_THAT(
      Parse(&kBush, {"--depth", "5", "--breadth", "1"}).status().message(),
      StartsWith("bush: \"--breadth\": can't read the settings."));
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr("Settings")));
}

//==============================================================================
// Value types
//==============================================================================

TEST(ParseCommandLineTest, Integers) {
  const std::vector<std::string_view> args = {"--count", "-12", "--limit",
                                              "+5"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kCount), Optional(-12));
  EXPECT_THAT(command_line->Get(kLimit), Optional(5));
}

TEST(ParseCommandLineTest, IntegerDefault) {
  const std::vector<std::string_view> args = {};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_EQ(command_line->Get(kCount), std::nullopt);
  EXPECT_THAT(command_line->Get(kLimit), Optional(10));
}

TEST(ParseCommandLineTest, IntegerLimits) {
  const std::vector<std::string_view> args = {"--count=9223372036854775807",
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
  const std::vector<std::string_view> args = {"--ratio", "0.25", "--scale",
                                              "-1e3"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kRatio), Optional(0.25));
  EXPECT_THAT(command_line->Get(kScale), Optional(-1000.0));
}

TEST(ParseCommandLineTest, FloatingPointDefault) {
  const std::vector<std::string_view> args = {};
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
  const std::vector<std::string_view> args = {"12", "2.5"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kMove, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kOffset), Optional(12));
  EXPECT_THAT(command_line->Get(kFactor), Optional(2.5));
}

TEST(ParseCommandLineTest, NegativeNumberArgs) {
  const std::vector<std::string_view> args = {"-r", "-5", "-.5"};
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

  const std::vector<std::string_view> args = {"-5"};
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kInput), Optional(std::string_view("-5")));
}

TEST(ParseCommandLineTest, Choices) {
  const std::vector<std::string_view> args = {"--shape", "square", "circle"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTypes, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_THAT(command_line->Get(kShape), Optional(std::string_view("square")));
  EXPECT_THAT(command_line->Get(kOutline),
              Optional(std::string_view("circle")));
}

TEST(ParseCommandLineTest, ChoiceDefault) {
  const std::vector<std::string_view> args = {};
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
  const std::vector<std::string_view> args = {};
  EXPECT_DEATH(ParseCommandLine(&kInvalid, args).status().IgnoreError(),
               "duplicate flag");
}

TEST(ParseCommandLineDeathTest, GetOutsideCommandPath) {
  const std::vector<std::string_view> args = {"status"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_DEATH(command_line->Get(kForce), "doesn't belong");
  EXPECT_DEATH(command_line->Get(kName), "doesn't belong");
}

TEST(ParseCommandLineDeathTest, GetForHelp) {
  const std::vector<std::string_view> args = {"-v", "--help"};
  absl::StatusOr<CliCommandLine> command_line = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(command_line.ok()) << command_line.status();
  EXPECT_DEATH(command_line->Get(kVerbose), "asks for help");
}

}  // namespace
}  // namespace gb
