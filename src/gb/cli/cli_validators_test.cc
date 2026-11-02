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
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Calls the common validators with a context, which they don't read.
class CliValidatorsTest : public ::testing::Test {
 protected:
  template <typename T>
  absl::Status Validate(const CliValidator<T>& validator, const T& value) {
    return validator.validate(context_, value);
  }

  ValidatedContext context_;
};

//==============================================================================
// Number validators
//==============================================================================

// The template arguments' commas would split a test macro's arguments, so the
// validators in these tests are first given a name.

TEST_F(CliValidatorsTest, InRange) {
  const CliValidator<int64_t>& int_in_range = kCliInRange<int64_t, 1, 64>;
  const CliValidator<double>& double_in_range = kCliInRange<double, 0.5, 1.5>;
  EXPECT_TRUE(Validate(int_in_range, int64_t{1}).ok());
  EXPECT_TRUE(Validate(int_in_range, int64_t{64}).ok());
  EXPECT_EQ(Validate(int_in_range, int64_t{0}).message(),
            "0 is not from 1 to 64");
  EXPECT_EQ(Validate(int_in_range, int64_t{65}).message(),
            "65 is not from 1 to 64");
  EXPECT_TRUE(Validate(double_in_range, 1.5).ok());
  EXPECT_EQ(Validate(double_in_range, 0.25).message(),
            "0.25 is not from 0.5 to 1.5");
}

TEST_F(CliValidatorsTest, AtLeast) {
  const CliValidator<int64_t>& int_at_least = kCliAtLeast<int64_t, 0>;
  const CliValidator<double>& double_at_least = kCliAtLeast<double, 0.5>;
  EXPECT_TRUE(Validate(int_at_least, int64_t{0}).ok());
  EXPECT_EQ(Validate(int_at_least, int64_t{-1}).message(), "-1 is less than 0");
  EXPECT_TRUE(Validate(double_at_least, 0.5).ok());
  EXPECT_EQ(Validate(double_at_least, 0.25).message(), "0.25 is less than 0.5");
}

TEST_F(CliValidatorsTest, AtMost) {
  const CliValidator<int64_t>& int_at_most = kCliAtMost<int64_t, 10>;
  const CliValidator<double>& double_at_most = kCliAtMost<double, 2>;
  EXPECT_TRUE(Validate(int_at_most, int64_t{10}).ok());
  EXPECT_EQ(Validate(int_at_most, int64_t{11}).message(), "11 is more than 10");
  EXPECT_EQ(Validate(double_at_most, 3.5).message(), "3.5 is more than 2");
}

TEST_F(CliValidatorsTest, BoundsOfEitherKind) {
  const CliValidator<int64_t>& int_in_range = kCliInRange<int64_t, 0.5, 64.5>;
  const CliValidator<double>& double_in_range = kCliInRange<double, 1, 64>;
  EXPECT_FALSE(Validate(int_in_range, int64_t{65}).ok());
  EXPECT_FALSE(Validate(double_in_range, 64.5).ok());
}

//==============================================================================
// String validators
//==============================================================================

TEST_F(CliValidatorsTest, NotEmpty) {
  EXPECT_TRUE(Validate(kCliNotEmpty, std::string_view("a")).ok());
  EXPECT_EQ(Validate(kCliNotEmpty, std::string_view("")).message(), "is empty");
}

//==============================================================================
// Validators in definitions
//==============================================================================

const CliFlag<int64_t> kThreads = {
    .name = "threads",
    .validators = {&kCliInRange<int64_t, 1, 64>},
};
const CliFlag<double> kScale = {
    .name = "scale",
    .validators = {&kCliAtLeast<double, 0.5>, &kCliAtMost<double, 2>},
};
const CliArg<std::string_view> kName = {
    .name = "name",
    .validators = {&kCliNotEmpty},
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

TEST_F(CliValidatorsTest, InDefinitions) {
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
