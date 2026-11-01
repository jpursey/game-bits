// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_values.h"

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Optional;

bool RunNothing(const CliValues&) { return true; }

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
    .commands = {&kRemote, &kStatus},
    .run = RunNothing,
};

// Returns the error message from parsing `args`, or an empty string if they
// parse.
std::string ParseError(const CliCommand* program,
                       const std::vector<std::string>& args) {
  absl::StatusOr<CliValues> values = ParseCommandLine(program, args);
  if (values.ok()) {
    return "";
  }
  EXPECT_EQ(values.status().code(), absl::StatusCode::kInvalidArgument);
  return std::string(values.status().message());
}

//==============================================================================
// Flags
//==============================================================================

TEST(ParseCommandLineTest, LongFlagWithValue) {
  const std::vector<std::string> args = {"--output", "out.txt", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kOutput), Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, LongFlagWithAttachedValue) {
  const std::vector<std::string> args = {"--output=out.txt", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kOutput), Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, AttachedValueWithEquals) {
  const std::vector<std::string> args = {"--output=a=b", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kOutput), Optional(std::string_view("a=b")));
}

TEST(ParseCommandLineTest, AttachedEmptyValue) {
  const std::vector<std::string> args = {"--output=", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kOutput), Optional(std::string_view("")));
}

TEST(ParseCommandLineTest, ShortFlagWithValue) {
  const std::vector<std::string> args = {"-o", "out.txt", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kOutput), Optional(std::string_view("out.txt")));
}

TEST(ParseCommandLineTest, ValueStartingWithDash) {
  const std::vector<std::string> args = {"--output", "-v", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kOutput), Optional(std::string_view("-v")));
  EXPECT_FALSE(values->Get(kVerbose));
}

TEST(ParseCommandLineTest, FlagNotGiven) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_EQ(values->Get(kOutput), std::nullopt);
}

TEST(ParseCommandLineTest, FlagDefault) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kMode), Optional(std::string_view("fast")));
}

TEST(ParseCommandLineTest, DefaultIsNotValidated) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kTitle), Optional(std::string_view("")));
  EXPECT_EQ(ParseError(&kConvert, {"--title=", "in.txt"}),
            "convert: \"--title\": is empty");
}

TEST(ParseCommandLineTest, FlagOverridesDefault) {
  const std::vector<std::string> args = {"--mode", "slow", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kMode), Optional(std::string_view("slow")));
}

TEST(ParseCommandLineTest, BoolFlags) {
  const std::vector<std::string> args = {"--verbose", "-q", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();

  // A bool flag reads as a plain bool, not a std::optional<bool>, which would
  // be true whenever it holds a value.
  static_assert(std::is_same_v<decltype(values->Get(kVerbose)), bool>);
  EXPECT_TRUE(values->Get(kVerbose));
  EXPECT_TRUE(values->Get(kQuiet));
}

TEST(ParseCommandLineTest, BoolFlagNotGiven) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_FALSE(values->Get(kVerbose));
}

TEST(ParseCommandLineTest, FlagsBetweenArgs) {
  const std::vector<std::string> args = {"-v", "in.txt", "-o", "out.txt",
                                         "more.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_TRUE(values->Get(kVerbose));
  EXPECT_THAT(values->Get(kOutput), Optional(std::string_view("out.txt")));
  EXPECT_THAT(values->Get(kInput), Optional(std::string_view("in.txt")));
  EXPECT_THAT(values->Get(kExtra), Optional(std::string_view("more.txt")));
}

TEST(ParseCommandLineTest, ValidatorsPass) {
  const std::vector<std::string> args = {"--label", "name", "in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kLabel), Optional(std::string_view("name")));
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
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kInput), Optional(std::string_view("in.txt")));
}

TEST(ParseCommandLineTest, OptionalArgs) {
  const std::vector<std::string> args = {"in.txt", "more.txt", "last.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kExtra), Optional(std::string_view("more.txt")));
  EXPECT_THAT(values->Get(kLast), Optional(std::string_view("last.txt")));
}

TEST(ParseCommandLineTest, OptionalArgsNotGiven) {
  const std::vector<std::string> args = {"in.txt"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kExtra), Optional(std::string_view("extra.txt")));
  EXPECT_EQ(values->Get(kLast), std::nullopt);
}

TEST(ParseCommandLineTest, DoubleDashEndsFlags) {
  const std::vector<std::string> args = {"-v", "--", "--quiet", "-o"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_TRUE(values->Get(kVerbose));
  EXPECT_FALSE(values->Get(kQuiet));
  EXPECT_THAT(values->Get(kInput), Optional(std::string_view("--quiet")));
  EXPECT_THAT(values->Get(kExtra), Optional(std::string_view("-o")));
}

TEST(ParseCommandLineTest, SingleDashIsArg) {
  const std::vector<std::string> args = {"-"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kConvert, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_THAT(values->Get(kInput), Optional(std::string_view("-")));
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
  absl::StatusOr<CliValues> values = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_EQ(values->GetProgram(), &kTool);
  EXPECT_EQ(values->GetCommand(), &kTool);
  EXPECT_THAT(values->GetCommandPath(), ElementsAre(&kTool));
}

TEST(ParseCommandLineTest, Subcommand) {
  const std::vector<std::string> args = {"remote", "add", "origin"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_EQ(values->GetProgram(), &kTool);
  EXPECT_EQ(values->GetCommand(), &kAdd);
  EXPECT_THAT(values->GetCommandPath(), ElementsAre(&kTool, &kRemote, &kAdd));
  EXPECT_THAT(values->Get(kName), Optional(std::string_view("origin")));
}

TEST(ParseCommandLineTest, ParentFlagBeforeSubcommand) {
  const std::vector<std::string> args = {"--verbose", "remote", "add",
                                         "origin"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_TRUE(values->Get(kVerbose));
}

TEST(ParseCommandLineTest, ParentFlagAfterSubcommand) {
  const std::vector<std::string> args = {"remote", "add", "origin", "-v", "-f"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_TRUE(values->Get(kVerbose));
  EXPECT_TRUE(values->Get(kForce));
}

TEST(ParseCommandLineTest, SameFlagInSiblingCommands) {
  const std::vector<std::string> args = {"remote", "remove", "-f", "origin"};
  absl::StatusOr<CliValues> values = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_EQ(values->GetCommand(), &kRemove);
  EXPECT_TRUE(values->Get(kForce));
}

TEST(ParseCommandLineTest, SubcommandFlagBeforeSubcommand) {
  EXPECT_EQ(ParseError(&kTool, {"remote", "--force", "add", "origin"}),
            "tool remote: unknown flag \"--force\"");
}

TEST(ParseCommandLineTest, UnknownCommand) {
  EXPECT_EQ(ParseError(&kTool, {"nope"}), "tool: unknown command \"nope\"");
}

TEST(ParseCommandLineTest, MissingCommand) {
  EXPECT_EQ(ParseError(&kTool, {"remote"}), "tool remote: missing command");
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
  absl::StatusOr<CliValues> values = ParseCommandLine(&kTool, args);
  ASSERT_TRUE(values.ok()) << values.status();
  EXPECT_DEATH(values->Get(kForce), "doesn't belong");
  EXPECT_DEATH(values->Get(kName), "doesn't belong");
}

}  // namespace
}  // namespace gb
