// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli_validators.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/strip.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

//==============================================================================
// Number validators
//==============================================================================

// The bounds' comma would split a test macro's arguments, so the validators
// in these tests are first given a name.

TEST(CliValidatorsTest, InRange) {
  const CliValidator<int64_t> int_in_range = CliInRange<1, 64>;
  const CliValidator<double> double_in_range = CliInRange<0.5, 1.5>;
  EXPECT_TRUE(int_in_range(1).ok());
  EXPECT_TRUE(int_in_range(64).ok());
  EXPECT_EQ(int_in_range(0).message(), "0 is not from 1 to 64");
  EXPECT_EQ(int_in_range(65).message(), "65 is not from 1 to 64");
  EXPECT_TRUE(double_in_range(1.5).ok());
  EXPECT_EQ(double_in_range(0.25).message(), "0.25 is not from 0.5 to 1.5");
}

TEST(CliValidatorsTest, AtLeast) {
  EXPECT_TRUE(CliAtLeast<0>(int64_t{0}).ok());
  EXPECT_EQ(CliAtLeast<0>(int64_t{-1}).message(), "-1 is less than 0");
  EXPECT_TRUE(CliAtLeast<0.5>(0.5).ok());
  EXPECT_EQ(CliAtLeast<0.5>(0.25).message(), "0.25 is less than 0.5");
}

TEST(CliValidatorsTest, AtMost) {
  EXPECT_TRUE(CliAtMost<10>(int64_t{10}).ok());
  EXPECT_EQ(CliAtMost<10>(int64_t{11}).message(), "11 is more than 10");
  EXPECT_EQ(CliAtMost<2>(3.5).message(), "3.5 is more than 2");
}

TEST(CliValidatorsTest, NumberValidatorsConvertToEachType) {
  const CliValidator<int64_t> int_in_range = &CliInRange<1, 64>;
  const CliValidator<double> double_in_range = &CliInRange<1, 64>;
  const CliValidator<double> double_at_least = CliAtLeast<1>;
  const CliValidator<int64_t> int_at_most = CliAtMost<64>;
  EXPECT_FALSE(int_in_range(65).ok());
  EXPECT_FALSE(double_in_range(64.5).ok());
  EXPECT_FALSE(double_at_least(0.5).ok());
  EXPECT_FALSE(int_at_most(65).ok());
}

//==============================================================================
// String validators
//==============================================================================

TEST(CliValidatorsTest, NotEmpty) {
  EXPECT_TRUE(CliNotEmpty("a").ok());
  EXPECT_EQ(CliNotEmpty("").message(), "is empty");
}

//==============================================================================
// Validators in definitions
//==============================================================================

const CliFlag<int64_t> kThreads = {
    .name = "threads",
    .validators = {CliInRange<1, 64>},
};
const CliFlag<double> kScale = {
    .name = "scale",
    .validators = {CliAtLeast<0.5>, CliAtMost<2>},
};
const CliArg<std::string_view> kName = {
    .name = "name",
    .validators = {CliNotEmpty},
};
const CliCommand kProgram = {
    .name = "prog",
    .flags = {&kThreads, &kScale},
    .args = {&kName},
};

// Returns the error message from parsing `args`, without the hint for help
// that ends it, or an empty string if they parse.
std::string ParseError(const std::vector<std::string_view>& args) {
  absl::StatusOr<CliCommandLine> command_line =
      ParseCommandLine(&kProgram, args);
  if (command_line.ok()) {
    return "";
  }
  std::string_view message = command_line.status().message();
  EXPECT_TRUE(absl::ConsumeSuffix(&message, ". Run \"prog --help\" for help."))
      << message;
  return std::string(message);
}

TEST(CliValidatorsTest, InDefinitions) {
  EXPECT_EQ(ParseError({"--threads=8", "--scale=2", "a"}), "");
  EXPECT_EQ(ParseError({"--threads=100", "a"}),
            "prog: \"--threads\": 100 is not from 1 to 64");
  EXPECT_EQ(ParseError({"--scale=0.25", "a"}),
            "prog: \"--scale\": 0.25 is less than 0.5");
  EXPECT_EQ(ParseError({"--scale=3", "a"}),
            "prog: \"--scale\": 3 is more than 2");
  EXPECT_EQ(ParseError({""}), "prog: <name>: is empty");
}

}  // namespace
}  // namespace gb
