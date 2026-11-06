// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/script/pilot_script.h"

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::StartsWith;

// Returns the steps read from `text`, as the lines they convert to.
std::vector<std::string> ReadLines(std::string text) {
  absl::StatusOr<PilotScript> script = ReadPilotScript(std::move(text));
  if (!script.ok()) {
    ADD_FAILURE() << script.status();
    return {};
  }
  std::vector<std::string> lines;
  for (const PilotStep& step : script->steps) {
    lines.push_back(absl::StrCat(step));
  }
  return lines;
}

// Returns the error message from reading `text`.
std::string ReadError(std::string text, std::string_view filename = {}) {
  absl::StatusOr<PilotScript> script =
      ReadPilotScript(std::move(text), filename);
  if (script.ok()) {
    ADD_FAILURE() << "The script was read without an error.";
    return {};
  }
  EXPECT_EQ(script.status().code(), absl::StatusCode::kInvalidArgument);
  return std::string(script.status().message());
}

// Returns the one step read from `text`.
PilotStep ReadStep(std::string text) {
  absl::StatusOr<PilotScript> script = ReadPilotScript(std::move(text));
  if (!script.ok() || script->steps.size() != 1) {
    ADD_FAILURE() << "Expected one step: " << script.status();
    return {};
  }
  return script->steps.front();
}

TEST(PilotScriptTest, ReadsEveryStep) {
  EXPECT_THAT(
      ReadLines("wait 30\n"
                "wait for loaded\n"
                "wait for loaded 20\n"
                "key f1\n"
                "key w for 20\n"
                "press key \"Left Shift\"\n"
                "release key \"Left Shift\"\n"
                "text \"hello\"\n"
                "move 640 -400\n"
                "move by -10 5\n"
                "click\n"
                "press\n"
                "release\n"
                "wheel -2\n"
                "run reset_peaks\n"
                "expect loaded\n"
                "expect error \"cannot be edited\"\n"
                "expect error \"cannot be edited\" 10\n"
                "save get_layer_1\n"),
      ElementsAre("wait 30", "wait for loaded 600", "wait for loaded 20",
                  "key f1", "key w for 20", "press key \"Left Shift\"",
                  "release key \"Left Shift\"", "text \"hello\"",
                  "move 640 -400", "move by -10 5", "click", "press", "release",
                  "wheel -2", "run reset_peaks", "expect loaded",
                  "expect error \"cannot be edited\" 600",
                  "expect error \"cannot be edited\" 10", "save get_layer_1"));
}

TEST(PilotScriptTest, ReadsHeldFrames) {
  EXPECT_THAT(ReadLines("key w for 0\n"
                        "key \"Left Shift\" for 1\n"
                        "click for 5\n"
                        "click right for 2\n"
                        "click 700 120 for 3\n"
                        "click \"OK\" for 30\n"),
              ElementsAre("key w for 0", "key \"Left Shift\"", "click for 5",
                          "click right for 2", "click 700 120 for 3",
                          "click \"OK\" for 30"));
}

TEST(PilotScriptTest, ExpectErrorIsBesideAConditionNamedError) {
  EXPECT_THAT(ReadLines("expect error\n"), ElementsAre("expect error"));
  EXPECT_EQ(ReadError("expect error edited"),
            "(1:14): Parse error at \"edited\": Expected end of line");
}

TEST(PilotScriptTest, ReadsClickForms) {
  EXPECT_THAT(ReadLines("click\n"
                        "click left\n"
                        "click middle\n"
                        "click right 3 4\n"
                        "click 700 120\n"
                        "click \"Get layer 1\"\n"
                        "click right \"Render Check/Get layer 1\"\n"),
              ElementsAre("click", "click", "click middle", "click right 3 4",
                          "click 700 120", "click \"Get layer 1\"",
                          "click right \"Render Check/Get layer 1\""));
}

TEST(PilotScriptTest, ReadsPressAndReleaseForms) {
  EXPECT_THAT(ReadLines("press key a\n"
                        "press key 1\n"
                        "release key a\n"
                        "press left\n"
                        "press middle 3 4\n"
                        "press right \"Extra rectangles\"\n"
                        "press 700 120\n"
                        "release left\n"
                        "release middle\n"
                        "release right\n"),
              ElementsAre("press key a", "press key 1", "release key a",
                          "press", "press middle 3 4",
                          "press right \"Extra rectangles\"", "press 700 120",
                          "release", "release middle", "release right"));
}

TEST(PilotScriptTest, ReadsRunValues) {
  PilotStep step = ReadStep("run set speed -2 \"two words\" 007");
  const auto* run = std::get_if<PilotRunStep>(&step);
  ASSERT_NE(run, nullptr);
  EXPECT_EQ(run->command, "set");
  EXPECT_THAT(run->args, ElementsAre("speed", "-2", "two words", "007"));
}

TEST(PilotScriptTest, NamesCanBeQuotedOrIntegers) {
  EXPECT_THAT(
      ReadLines("key \"Left Shift\"\n"
                "key 1\n"
                "key \"Keypad 1\" for 2\n"
                "wait for \"is loaded\" 5\n"
                "expect \"is loaded\"\n"
                "run \"do it\" now\n"
                "save \"frame 1\"\n"),
      ElementsAre("key \"Left Shift\"", "key 1", "key \"Keypad 1\" for 2",
                  "wait for \"is loaded\" 5", "expect \"is loaded\"",
                  "run \"do it\" now", "save \"frame 1\""));
}

TEST(PilotScriptTest, ReadsStringEscapes) {
  PilotStep step = ReadStep(R"(text "a\"b\\c\n\t\x41é")");
  ASSERT_TRUE(std::holds_alternative<PilotTextStep>(step));
  EXPECT_EQ(std::get<PilotTextStep>(step).text, "a\"b\\c\n\tAé");
}

TEST(PilotScriptTest, StepsReadBackFromTheirText) {
  const PilotStep steps[] = {
      PilotKeyStep{.key = "1"},
      PilotKeyStep{.key = "Left Shift"},
      PilotKeyStep{.key = "_x9", .frames = 0},
      PilotTextStep{.text = "say \"hi\"\\\r\n\t\x01\x7f é"},
      PilotMouseClickStep{.button = PilotMouseButton::kRight, .target = "a\"b"},
      PilotMouseClickStep{.button = PilotMouseButton::kMiddle, .frames = 7},
      PilotMouseMoveByStep{.offset = {-1, 0}},
      PilotKeyPressStep{.key = "Left Shift"},
      PilotKeyReleaseStep{.key = "key"},
      PilotMousePressStep{.button = PilotMouseButton::kRight,
                          .target = PilotPoint{-1, 2}},
      PilotMousePressStep{.target = "a\"b"},
      PilotMouseReleaseStep{.button = PilotMouseButton::kMiddle},
      PilotRunStep{.command = "set",
                   .args = {"-2", "007", "-0", "a-b", "", "99999999999"}},
      PilotExpectErrorStep{.text = "can't \"edit\"", .max_frames = 0},
      PilotSaveStep{.name = "2x"},
      PilotSaveStep{.name = "_"},
      PilotSaveStep{.name = "_1"},
  };
  for (const PilotStep& step : steps) {
    const std::string line = absl::StrCat(step);
    EXPECT_THAT(ReadLines(line), ElementsAre(line));
  }
}

TEST(PilotScriptTest, SkipsCommentsAndBlankLines) {
  EXPECT_THAT(ReadLines("# Start\n"
                        "\n"
                        "wait 1  # One\n"
                        "   \n"
                        "# End\n"),
              ElementsAre("wait 1"));
  EXPECT_THAT(ReadLines("wait 1\r\n\r\nwait 2\r\n"),
              ElementsAre("wait 1", "wait 2"));
  EXPECT_THAT(ReadLines("wait 1\nwait 2"), ElementsAre("wait 1", "wait 2"));
  EXPECT_THAT(ReadLines(""), IsEmpty());
  EXPECT_THAT(ReadLines("# Nothing"), IsEmpty());
}

TEST(PilotScriptTest, HidesWindowOnlyBeforeOtherSteps) {
  absl::StatusOr<PilotScript> script = ReadPilotScript("wait 1");
  ASSERT_TRUE(script.ok()) << script.status();
  EXPECT_FALSE(script->hide_window);

  script = ReadPilotScript("# Hidden\n\nhide window\nwait 1\n");
  ASSERT_TRUE(script.ok()) << script.status();
  EXPECT_TRUE(script->hide_window);
  EXPECT_EQ(script->steps.size(), 1);

  script = ReadPilotScript("hide window");
  ASSERT_TRUE(script.ok()) << script.status();
  EXPECT_TRUE(script->hide_window);
  EXPECT_THAT(script->steps, IsEmpty());

  EXPECT_EQ(ReadError("wait 1\nhide window\n"),
            "(2:1): Parse error at \"hide\": \"hide window\" must come before "
            "every other step");
}

TEST(PilotScriptTest, UnknownStepFails) {
  EXPECT_EQ(ReadError("wait 1\nwiat 3\nwait 1\n"),
            "(2:1): Parse error at \"wiat\": Unknown step");
  EXPECT_EQ(ReadError("wait 1\n  3"),
            "(2:3): Parse error at \"3\": Unknown step");
}

TEST(PilotScriptTest, MissingValuesFail) {
  EXPECT_EQ(ReadError("wait\n"),
            "(1:5): Parse error at \"\": Expected integer value");
  EXPECT_EQ(ReadError("key w for"),
            "(1:10): Parse error at \"\": Expected integer value");
  EXPECT_EQ(ReadError("click \"OK\" for\n"),
            "(1:15): Parse error at \"\": Expected integer value");
  EXPECT_EQ(ReadError("click 700\n"),
            "(1:10): Parse error at \"\": Expected integer value");
  EXPECT_EQ(ReadError("text hello"),
            "(1:6): Parse error at \"hello\": Expected string value");
  EXPECT_EQ(ReadError("hide"),
            "(1:5): Parse error at \"\": Expected identifier window");
  EXPECT_EQ(ReadError("press key\n"),
            "(1:10): Parse error at \"\": Expected string value");
  EXPECT_EQ(ReadError("press 700\n"),
            "(1:10): Parse error at \"\": Expected integer value");
  EXPECT_EQ(ReadError("move by 3\n"),
            "(1:10): Parse error at \"\": Expected integer value");
}

TEST(PilotScriptTest, LeftOverValuesFail) {
  EXPECT_EQ(ReadError("wait 1 2\n"),
            "(1:8): Parse error at \"2\": Expected end of line");
  EXPECT_EQ(ReadError("wait 1\nkey a b"),
            "(2:7): Parse error at \"b\": Expected end of line");
  EXPECT_EQ(ReadError("wait 1 wait 2\n"),
            "(1:8): Parse error at \"wait\": Expected end of line");
  EXPECT_EQ(ReadError("click \"OK\" 1 2\n"),
            "(1:12): Parse error at \"1\": Expected end of line");
  EXPECT_EQ(ReadError("press \"OK\" for 2\n"),
            "(1:12): Parse error at \"for\": Expected end of line");
  EXPECT_EQ(ReadError("release 1 2\n"),
            "(1:9): Parse error at \"1\": Expected end of line");
  EXPECT_EQ(ReadError("release key a b\n"),
            "(1:15): Parse error at \"b\": Expected end of line");
}

TEST(PilotScriptTest, NegativeFramesFail) {
  EXPECT_EQ(ReadError("wait -1"),
            "(1:6): Parse error at \"-1\": Frames can't be negative");
  EXPECT_EQ(ReadError("wait for loaded -1"),
            "(1:17): Parse error at \"-1\": Frames can't be negative");
  EXPECT_EQ(ReadError("key w for -1"),
            "(1:11): Parse error at \"-1\": Frames can't be negative");
  EXPECT_EQ(ReadError("click 1 2 for -1"),
            "(1:15): Parse error at \"-1\": Frames can't be negative");
  EXPECT_EQ(ReadError("expect error \"edited\" -1"),
            "(1:23): Parse error at \"-1\": Frames can't be negative");
}

TEST(PilotScriptTest, BadTokensFail) {
  EXPECT_THAT(ReadError("text \"hello"), StartsWith("(1:6): "));
  EXPECT_THAT(ReadError("wait 99999999999"), StartsWith("(1:6): "));
}

TEST(PilotScriptTest, FailsAtTheFirstProblem) {
  EXPECT_EQ(ReadError("wait -1\nwiat 3\n"),
            "(1:6): Parse error at \"-1\": Frames can't be negative");
  EXPECT_EQ(ReadError("wait 1\nwiat 3\nwait -1\n"),
            "(2:1): Parse error at \"wiat\": Unknown step");
}

TEST(PilotScriptTest, ErrorsStartWithTheFilename) {
  EXPECT_EQ(ReadError("wiat 3", "run.txt"),
            "run.txt(1:1): Parse error at \"wiat\": Unknown step");
}

}  // namespace
}  // namespace gb
