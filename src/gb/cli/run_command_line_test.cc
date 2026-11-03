// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/run_command_line.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"
#include "gb/cli/cli_writer.h"
#include "gb/cli/fake_cli_writer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StartsWith;

const CliFlag<bool> kFail = {.name = "fail"};
const CliFlag<bool> kMistake = {.name = "mistake"};

bool Run(ValidatedContext& context, const CliCommandLine& command_line) {
  if (command_line.Get(kMistake)) {
    context.SetValue<std::string>(kKeyCliUsageError,
                                  "<input> and <output> are the same file");
    return false;
  }
  return !command_line.Get(kFail);
}

const CliCommand kProgram = {
    .name = "prog",
    .help = "Does nothing.",
    .flags = {&kFail, &kMistake},
    .run = Run,
};

class RunCommandLineTest : public ::testing::Test {
 protected:
  RunCommandLineTest() {
    context_.SetValue<CliWriter>(writer_.GetWriter());
    ValidatedContext::SetGlobalErrorCallback(
        [this](const std::string& message) {
          context_errors_.push_back(message);
        });
  }
  ~RunCommandLineTest() override {
    ValidatedContext::SetGlobalErrorCallback(nullptr);
  }

  int RunWith(const std::vector<std::string_view>& args) {
    return RunCommandLine(&context_, &kProgram, args);
  }

  // The writer is first, as the context's CliWriter refers to it.
  FakeCliWriter writer_;
  Context context_;

  // The errors ValidatedContext has reported.
  std::vector<std::string> context_errors_;
};

TEST_F(RunCommandLineTest, Succeeds) {
  EXPECT_EQ(RunWith({}), 0);
  EXPECT_EQ(writer_.TakeHelp(), "");
  EXPECT_EQ(writer_.TakeError(), "");
}

TEST_F(RunCommandLineTest, CommandFails) {
  EXPECT_EQ(RunWith({"--fail"}), 1);
  EXPECT_EQ(writer_.TakeHelp(), "");
  EXPECT_EQ(writer_.TakeError(), "");
}

TEST_F(RunCommandLineTest, CommandFindsAUsageError) {
  EXPECT_EQ(RunWith({"--mistake"}), 2);
  EXPECT_EQ(writer_.TakeHelp(), "");
  EXPECT_EQ(writer_.TakeError(),
            "prog: <input> and <output> are the same file. Run \"prog --help\" "
            "for help.");

  // The next run doesn't see this run's mistake.
  EXPECT_EQ(RunWith({"--fail"}), 1);
  EXPECT_EQ(writer_.TakeError(), "");
}

TEST_F(RunCommandLineTest, WritesHelp) {
  EXPECT_EQ(RunWith({"--help"}), 0);
  EXPECT_THAT(writer_.TakeHelp(), StartsWith("Usage: prog [<flags>]\n"));
  EXPECT_EQ(writer_.TakeError(), "");
}

TEST_F(RunCommandLineTest, WritesError) {
  EXPECT_EQ(RunWith({"--nope"}), 2);
  EXPECT_EQ(writer_.TakeHelp(), "");
  EXPECT_EQ(writer_.TakeError(),
            "prog: unknown flag \"--nope\". Run \"prog --help\" for help.");
}

GB_CONTEXT_CONSTRAINT_NAMED(kConstraintNeeded, kInRequired, int, "needed");

absl::Status ValidateAnything(const ValidatedContext&, const int64_t&) {
  return absl::OkStatus();
}

TEST_F(RunCommandLineTest, ContextMissingAValue) {
  static const CliValidator<int64_t> kNeedsValue = {
      .validate = ValidateAnything,
      .constraints = {&kConstraintNeeded},
  };
  static const CliFlag<int64_t> kNeeds = {.name = "needs",
                                          .validators = {&kNeedsValue}};
  static const CliCommand kNeedsProgram = {.name = "prog", .flags = {&kNeeds}};
  EXPECT_EQ(RunCommandLine(&context_, &kNeedsProgram, {}), 1);
  EXPECT_EQ(writer_.TakeHelp(), "");
  EXPECT_EQ(writer_.TakeError(),
            "prog: the context is missing a value that the validators of its "
            "flags or arguments read");
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr("needed")));
}

TEST_F(RunCommandLineTest, InvalidContext) {
  // A width that isn't an int fails kConstraintCliHelpWidth.
  context_.SetValue<int64_t>(kKeyCliHelpWidth, 40);
  EXPECT_EQ(RunWith({}), 1);
  EXPECT_EQ(writer_.TakeHelp(), "");
  EXPECT_EQ(writer_.TakeError(), "");
  EXPECT_THAT(context_errors_, ElementsAre(HasSubstr(kKeyCliHelpWidth)));
}

TEST(GetMainArgsTest, SkipsTheProgramName) {
  char program[] = "prog";
  char first[] = "first";
  char second[] = "second";
  char* argv[] = {program, first, second};
  EXPECT_THAT(GetMainArgs(3, argv), ElementsAre("first", "second"));
}

TEST(GetMainArgsTest, NoArguments) {
  char program[] = "prog";
  char* argv[] = {program};
  EXPECT_THAT(GetMainArgs(1, argv), IsEmpty());
  EXPECT_THAT(GetMainArgs(0, argv), IsEmpty());
}

}  // namespace
}  // namespace gb
