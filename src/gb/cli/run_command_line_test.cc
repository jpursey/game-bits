// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/run_command_line.h"

#include <string_view>
#include <vector>

#include "gb/base/context.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli_writer.h"
#include "gb/cli/fake_cli_writer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::StartsWith;

const CliFlag<bool> kFail = {.name = "fail"};

bool Run(ValidatedContext&, const CliCommandLine& command_line) {
  return !command_line.Get(kFail);
}

const CliCommand kProgram = {
    .name = "prog",
    .help = "Does nothing.",
    .flags = {&kFail},
    .run = Run,
};

class RunCommandLineTest : public ::testing::Test {
 protected:
  RunCommandLineTest() { context_.SetValue<CliWriter>(writer_.GetWriter()); }

  int RunWith(const std::vector<std::string_view>& args) {
    return RunCommandLine(&context_, &kProgram, args);
  }

  // The writer is first, as the context's CliWriter refers to it.
  FakeCliWriter writer_;
  Context context_;
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
