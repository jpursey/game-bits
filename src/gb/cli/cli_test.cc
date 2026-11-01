// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/cli/cli.h"

#include <string_view>

#include "gtest/gtest.h"

namespace gb {
namespace {

const CliFlag<bool> kAlpha = {.name = "alpha", .short_name = 'a'};
const CliFlag<std::string_view> kValue = {.name = "value", .short_name = 'v'};
const CliArg<std::string_view> kRequired = {.name = "required"};
const CliArg<std::string_view> kOptional = {.name = "optional",
                                            .optional = true};

const CliCommand kSub = {.name = "sub", .flags = {&kValue}};
const CliCommand kOtherSub = {.name = "other", .flags = {&kValue}};
const CliCommand kValid = {
    .name = "prog",
    .flags = {&kAlpha},
    .commands = {&kSub, &kOtherSub},
};

TEST(CliCommandTest, ListsHoldTheirElements) {
  ASSERT_EQ(kValid.flags.size(), 1);
  EXPECT_EQ(kValid.flags.begin()[0].GetDefinition(), &kAlpha);
  ASSERT_EQ(kValid.commands.size(), 2);
  EXPECT_EQ(kValid.commands.begin()[0], &kSub);
  EXPECT_EQ(kValid.commands.begin()[1], &kOtherSub);
}

TEST(ValidateCliCommandTest, Valid) {
  EXPECT_TRUE(ValidateCliCommand(&kValid).ok());
}

TEST(ValidateCliCommandTest, ValidWithArgs) {
  static const CliCommand kProgram = {
      .name = "prog",
      .flags = {&kAlpha, &kValue},
      .args = {&kRequired, &kOptional},
  };
  EXPECT_TRUE(ValidateCliCommand(&kProgram).ok());
}

TEST(ValidateCliCommandTest, NullCommand) {
  static const CliCommand kProgram = {.name = "prog", .commands = {nullptr}};
  const absl::Status status = ValidateCliCommand(&kProgram);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status.message(), "prog: null command");
}

TEST(ValidateCliCommandTest, NoName) {
  static const CliCommand kNoName = {};
  static const CliCommand kProgram = {.name = "prog", .commands = {&kNoName}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog : command has no name");
}

TEST(ValidateCliCommandTest, NameStartsWithDash) {
  static const CliCommand kDash = {.name = "-sub"};
  static const CliCommand kProgram = {.name = "prog", .commands = {&kDash}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog -sub: command name starts with \"-\"");
}

const CliCommand kContainsItself = {
    .name = "prog",
    .commands = {&kContainsItself},
};

TEST(ValidateCliCommandTest, ContainsItself) {
  EXPECT_EQ(ValidateCliCommand(&kContainsItself).message(),
            "prog: command \"prog\" contains itself");
}

TEST(ValidateCliCommandTest, DuplicateCommand) {
  static const CliCommand kSubAgain = {.name = "sub"};
  static const CliCommand kProgram = {.name = "prog",
                                      .commands = {&kSub, &kSubAgain}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: duplicate command \"sub\"");
}

TEST(ValidateCliCommandTest, ArgsAndCommands) {
  static const CliCommand kProgram = {
      .name = "prog",
      .args = {&kRequired},
      .commands = {&kSub},
  };
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: command has both arguments and commands");
}

TEST(ValidateCliCommandTest, FlagNoName) {
  static const CliFlag<bool> kNoName = {.short_name = 'n'};
  static const CliCommand kProgram = {.name = "prog", .flags = {&kNoName}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(), "prog: flag has no name");
}

TEST(ValidateCliCommandTest, FlagNameStartsWithDash) {
  static const CliFlag<bool> kDash = {.name = "-dash"};
  static const CliCommand kProgram = {.name = "prog", .flags = {&kDash}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: flag name \"-dash\" starts with \"-\" or contains \"=\"");
}

TEST(ValidateCliCommandTest, FlagNameContainsEquals) {
  static const CliFlag<std::string_view> kEquals = {.name = "a=b"};
  static const CliCommand kProgram = {.name = "prog", .flags = {&kEquals}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: flag name \"a=b\" starts with \"-\" or contains \"=\"");
}

TEST(ValidateCliCommandTest, InvalidShortName) {
  static const CliFlag<bool> kDash = {.name = "dash", .short_name = '-'};
  static const CliFlag<bool> kSpace = {.name = "space", .short_name = ' '};
  static const CliCommand kDashProgram = {.name = "prog", .flags = {&kDash}};
  static const CliCommand kSpaceProgram = {.name = "prog", .flags = {&kSpace}};
  EXPECT_EQ(ValidateCliCommand(&kDashProgram).message(),
            "prog: flag \"--dash\" has an invalid short name");
  EXPECT_EQ(ValidateCliCommand(&kSpaceProgram).message(),
            "prog: flag \"--space\" has an invalid short name");
}

TEST(ValidateCliCommandTest, ShortNameThatStartsANumber) {
  static const CliFlag<bool> kDigit = {.name = "digit", .short_name = '5'};
  static const CliFlag<bool> kDot = {.name = "dot", .short_name = '.'};
  static const CliCommand kDigitProgram = {.name = "prog", .flags = {&kDigit}};
  static const CliCommand kDotProgram = {.name = "prog", .flags = {&kDot}};
  EXPECT_EQ(ValidateCliCommand(&kDigitProgram).message(),
            "prog: flag \"--digit\" has an invalid short name");
  EXPECT_EQ(ValidateCliCommand(&kDotProgram).message(),
            "prog: flag \"--dot\" has an invalid short name");
}

absl::Status ValidateAnything(const bool&) { return absl::OkStatus(); }

TEST(ValidateCliCommandTest, BoolFlagWithValueFields) {
  static const CliFlag<bool> kValueName = {.name = "a", .value_name = "x"};
  static const CliFlag<bool> kDefault = {.name = "b", .default_value = true};
  static const CliFlag<bool> kValidators = {.name = "c",
                                            .validators = {ValidateAnything}};
  static const CliCommand kValueNameProgram = {.name = "prog",
                                               .flags = {&kValueName}};
  static const CliCommand kDefaultProgram = {.name = "prog",
                                             .flags = {&kDefault}};
  static const CliCommand kValidatorsProgram = {.name = "prog",
                                                .flags = {&kValidators}};
  EXPECT_EQ(ValidateCliCommand(&kValueNameProgram).message(),
            "prog: flag \"--a\" is a bool, so can't have a value name");
  EXPECT_EQ(ValidateCliCommand(&kDefaultProgram).message(),
            "prog: flag \"--b\" is a bool, so can't have a default value");
  EXPECT_EQ(ValidateCliCommand(&kValidatorsProgram).message(),
            "prog: flag \"--c\" is a bool, so can't have validators");
}

enum class Color { kRed, kGreen };

TEST(ValidateCliCommandTest, ValidEnumeration) {
  static const CliFlag<Color> kFlag = {
      .name = "color",
      .names = {{.name = "red", .value = Color::kRed},
                {.name = "green", .value = Color::kGreen}},
  };
  static const CliArg<Color> kArg = {
      .name = "color",
      .names = {{.name = "red", .value = Color::kRed}},
  };
  static const CliCommand kProgram = {
      .name = "prog",
      .flags = {&kFlag},
      .args = {&kArg},
  };
  EXPECT_TRUE(ValidateCliCommand(&kProgram).ok());
}

TEST(ValidateCliCommandTest, EnumerationWithoutNames) {
  static const CliFlag<Color> kFlag = {.name = "color"};
  static const CliArg<Color> kArg = {.name = "color"};
  static const CliCommand kFlagProgram = {.name = "prog", .flags = {&kFlag}};
  static const CliCommand kArgProgram = {.name = "prog", .args = {&kArg}};
  EXPECT_EQ(ValidateCliCommand(&kFlagProgram).message(),
            "prog: flag \"--color\" is an enumeration with no names");
  EXPECT_EQ(ValidateCliCommand(&kArgProgram).message(),
            "prog: argument <color> is an enumeration with no names");
}

TEST(ValidateCliCommandTest, EmptyEnumerationName) {
  static const CliFlag<Color> kFlag = {
      .name = "color",
      .names = {{.name = "red", .value = Color::kRed},
                {.name = "", .value = Color::kGreen}},
  };
  static const CliCommand kProgram = {.name = "prog", .flags = {&kFlag}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: flag \"--color\" has an empty enumeration name");
}

TEST(ValidateCliCommandTest, EnumerationNameStartsWithDash) {
  static const CliArg<Color> kArg = {
      .name = "color",
      .names = {{.name = "-red", .value = Color::kRed}},
  };
  static const CliCommand kProgram = {.name = "prog", .args = {&kArg}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: argument <color> has enumeration name \"-red\", which "
            "starts with \"-\"");
}

TEST(ValidateCliCommandTest, DuplicateEnumerationName) {
  static const CliArg<Color> kArg = {
      .name = "color",
      .names = {{.name = "red", .value = Color::kRed},
                {.name = "red", .value = Color::kGreen}},
  };
  static const CliCommand kProgram = {.name = "prog", .args = {&kArg}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: argument <color> has duplicate enumeration name \"red\"");
}

TEST(ValidateCliCommandTest, DuplicateFlag) {
  static const CliFlag<std::string_view> kAlphaValue = {.name = "alpha"};
  static const CliCommand kProgram = {.name = "prog",
                                      .flags = {&kAlpha, &kAlphaValue}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: duplicate flag \"--alpha\"");
}

TEST(ValidateCliCommandTest, SameFlagTwice) {
  static const CliCommand kProgram = {.name = "prog",
                                      .flags = {&kAlpha, &kAlpha}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: duplicate flag \"--alpha\"");
}

TEST(ValidateCliCommandTest, DuplicateShortName) {
  static const CliFlag<bool> kOther = {.name = "other", .short_name = 'a'};
  static const CliCommand kProgram = {.name = "prog",
                                      .flags = {&kAlpha, &kOther}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: duplicate flag \"-a\"");
}

TEST(ValidateCliCommandTest, DuplicateFlagFromParent) {
  static const CliCommand kAlphaSub = {.name = "sub", .flags = {&kAlpha}};
  static const CliCommand kProgram = {
      .name = "prog",
      .flags = {&kAlpha},
      .commands = {&kAlphaSub},
  };
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog sub: duplicate flag \"--alpha\"");
}

TEST(ValidateCliCommandTest, DuplicateShortNameFromParent) {
  static const CliFlag<bool> kOther = {.name = "other", .short_name = 'a'};
  static const CliCommand kOtherSub = {.name = "sub", .flags = {&kOther}};
  static const CliCommand kProgram = {
      .name = "prog",
      .flags = {&kAlpha},
      .commands = {&kOtherSub},
  };
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog sub: duplicate flag \"-a\"");
}

TEST(ValidateCliCommandTest, ArgNoName) {
  static const CliArg<std::string_view> kNoName = {};
  static const CliCommand kProgram = {.name = "prog", .args = {&kNoName}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: argument has no name");
}

TEST(ValidateCliCommandTest, DuplicateArg) {
  static const CliArg<std::string_view> kRequiredAgain = {.name = "required"};
  static const CliCommand kProgram = {.name = "prog",
                                      .args = {&kRequired, &kRequiredAgain}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: duplicate argument <required>");
}

TEST(ValidateCliCommandTest, RequiredAfterOptional) {
  static const CliCommand kProgram = {.name = "prog",
                                      .args = {&kOptional, &kRequired}};
  EXPECT_EQ(ValidateCliCommand(&kProgram).message(),
            "prog: required argument <required> follows an optional one");
}

}  // namespace
}  // namespace gb
