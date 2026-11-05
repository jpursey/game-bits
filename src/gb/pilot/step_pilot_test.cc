// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/step_pilot.h"

#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gb/image/image.h"
#include "gb/image/pixel.h"
#include "gb/test/log_error_guard.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;

// A run fails if no frame is drawn for a save within this many frames.
constexpr int kMaxSaveFrames = 600;

PilotInput KeyDown(std::string_view key) {
  return PilotKeyEvent{.key = std::string(key), .down = true};
}

PilotInput KeyUp(std::string_view key) {
  return PilotKeyEvent{.key = std::string(key), .down = false};
}

PilotInput Text(std::string_view text) {
  return PilotTextEvent{.text = std::string(text)};
}

PilotInput Move(int x, int y) {
  return PilotMouseMoveEvent{.position = {x, y}};
}

PilotInput Button(PilotMouseButton button, bool down, int x, int y) {
  return PilotMouseButtonEvent{
      .button = button, .down = down, .position = {x, y}};
}

PilotInput Wheel(int amount) { return PilotMouseWheelEvent{.amount = amount}; }

// Runs a frame as a program would, drawing `drawn` if it isn't null, and
// returns the frame's input.
std::vector<PilotInput> RunFrame(Pilot* pilot, const Image* drawn = nullptr) {
  pilot->BeginFrame();
  absl::Span<const PilotInput> input = pilot->GetInput();
  std::vector<PilotInput> frame_input(input.begin(), input.end());
  if (drawn != nullptr && pilot->IsSavingFrame()) {
    pilot->SaveFrame(*drawn);
  }
  pilot->EndFrame();
  return frame_input;
}

// Runs frames until the pilot is done, drawing none, and returns each frame's
// input.
std::vector<std::vector<PilotInput>> RunUntilDone(Pilot* pilot) {
  std::vector<std::vector<PilotInput>> frames;
  while (!pilot->IsDone() && static_cast<int>(frames.size()) < 1000) {
    frames.push_back(RunFrame(pilot));
  }
  return frames;
}

// Returns an item finder that finds `point` for `label`.
Pilot::ItemFinder FindLabel(std::string_view label, PilotPoint point) {
  return [label = std::string(label), point](std::string_view find_label) {
    return find_label == label ? std::vector<PilotPoint>{point}
                               : std::vector<PilotPoint>{};
  };
}

auto LogText(std::string_view text) {
  return Field(&LogRecorder::Message::text, std::string(text));
}

auto LogTextHas(std::string_view text) {
  return Field(&LogRecorder::Message::text, HasSubstr(std::string(text)));
}

class StepPilotTest : public ::testing::Test {
 protected:
  LogErrorGuard log_error_guard_;
};

//------------------------------------------------------------------------------
// Steps as text
//------------------------------------------------------------------------------

TEST(PilotStepTest, StepsAreWrittenAsScriptLines) {
  EXPECT_EQ(absl::StrCat(PilotWaitStep{.frames = 3}), "wait 3");
  EXPECT_EQ(absl::StrCat(PilotWaitForStep{.condition = "loaded"}),
            "wait for loaded 600");
  EXPECT_EQ(absl::StrCat(PilotKeyStep{.key = "f1"}), "key f1");
  EXPECT_EQ(absl::StrCat(PilotKeyStep{.key = "Left Shift", .frames = 20}),
            "key \"Left Shift\" for 20");
  EXPECT_EQ(absl::StrCat(PilotKeyStep{.key = "a", .frames = 0}), "key a for 0");
  EXPECT_EQ(absl::StrCat(PilotTextStep{.text = "say \"hi\"\n"}),
            "text \"say \\\"hi\\\"\\n\"");
  EXPECT_EQ(absl::StrCat(PilotMouseMoveStep{.position = {640, 400}}),
            "move 640 400");
  EXPECT_EQ(absl::StrCat(PilotMouseClickStep{}), "click");
  EXPECT_EQ(
      absl::StrCat(PilotMouseClickStep{.button = PilotMouseButton::kMiddle,
                                       .target = PilotPoint{7, 8}}),
      "click middle 7 8");
  EXPECT_EQ(absl::StrCat(PilotMouseClickStep{.button = PilotMouseButton::kRight,
                                             .target = "OK"}),
            "click right \"OK\"");
  EXPECT_EQ(absl::StrCat(PilotMouseClickStep{.frames = 5}), "click for 5");
  EXPECT_EQ(absl::StrCat(
                PilotMouseClickStep{.target = PilotPoint{7, 8}, .frames = 30}),
            "click 7 8 for 30");
  EXPECT_EQ(absl::StrCat(PilotMouseWheelStep{.amount = -2}), "wheel -2");
  EXPECT_EQ(absl::StrCat(PilotRunStep{.command = "reset"}), "run reset");
  EXPECT_EQ(absl::StrCat(PilotRunStep{.command = "set",
                                      .args = {"speed", "-2", "two words"}}),
            "run set speed -2 \"two words\"");
  EXPECT_EQ(absl::StrCat(PilotExpectStep{.condition = "loaded"}),
            "expect loaded");
  EXPECT_EQ(absl::StrCat(PilotExpectErrorStep{.text = "can't \"edit\""}),
            "expect error \"can't \\\"edit\\\"\" 600");
  EXPECT_EQ(absl::StrCat(PilotSaveStep{.name = "start"}), "save start");
}

TEST(PilotStepTest, PilotStepIsWrittenAsTheStepItHolds) {
  const PilotStep step = PilotKeyStep{.key = "a"};
  EXPECT_EQ(absl::StrCat(step), "key a");
}

//------------------------------------------------------------------------------
// Options and done
//------------------------------------------------------------------------------

TEST_F(StepPilotTest, HasOptions) {
  StepPilot pilot({.delta_time = absl::Milliseconds(10), .hide_window = true});
  EXPECT_EQ(pilot.GetDeltaTime(), absl::Milliseconds(10));
  EXPECT_TRUE(pilot.IsWindowHidden());

  StepPilot default_pilot;
  EXPECT_EQ(default_pilot.GetDeltaTime(), absl::Seconds(1) / 60);
  EXPECT_FALSE(default_pilot.IsWindowHidden());
}

TEST_F(StepPilotTest, WithNoStepsIsDoneAfterOneFrame) {
  StepPilot pilot;
  EXPECT_FALSE(pilot.IsDone());
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(IsEmpty()));
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, StepsThatTakeNoTimeRunInOneFrame) {
  StepPilot pilot;
  pilot.AddSteps({PilotTextStep{.text = "hi"},
                  PilotMouseMoveStep{.position = {10, 20}},
                  PilotMouseWheelStep{.amount = -2}});
  pilot.BeginFrame();
  EXPECT_THAT(pilot.GetInput(),
              ElementsAre(Text("hi"), Move(10, 20), Wheel(-2)));
  EXPECT_EQ(pilot.GetMouse(), (PilotMouse{.position = {10, 20}}));
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
}

//------------------------------------------------------------------------------
// Waits and keys
//------------------------------------------------------------------------------

TEST_F(StepPilotTest, WaitTakesFrames) {
  StepPilot pilot;
  pilot.AddSteps({PilotWaitStep{.frames = 3}, PilotTextStep{.text = "a"}});
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(IsEmpty(), IsEmpty(), IsEmpty(),
                                                ElementsAre(Text("a"))));
}

TEST_F(StepPilotTest, WaitOfNoFramesTakesNoTime) {
  StepPilot pilot;
  pilot.AddSteps({PilotWaitStep{.frames = 0}, PilotTextStep{.text = "a"}});
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(ElementsAre(Text("a"))));
}

TEST_F(StepPilotTest, LastWaitRunsOnlyItsFrames) {
  StepPilot pilot;
  pilot.AddSteps({PilotWaitStep{.frames = 2}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(2));
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, KeyIsReleasedInAFrameOfItsOwn) {
  StepPilot pilot;
  pilot.AddSteps({PilotKeyStep{.key = "a"}, PilotKeyStep{.key = "a"},
                  PilotTextStep{.text = "b"}});
  EXPECT_THAT(RunUntilDone(&pilot),
              ElementsAre(ElementsAre(KeyDown("a")), ElementsAre(KeyUp("a")),
                          ElementsAre(KeyDown("a")), ElementsAre(KeyUp("a")),
                          ElementsAre(Text("b"))));
}

TEST_F(StepPilotTest, KeyIsHeldForItsFrames) {
  StepPilot pilot;
  pilot.AddSteps({PilotKeyStep{.key = "Left Shift", .frames = 3}});
  EXPECT_THAT(RunUntilDone(&pilot),
              ElementsAre(ElementsAre(KeyDown("Left Shift")), IsEmpty(),
                          IsEmpty(), ElementsAre(KeyUp("Left Shift"))));
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, KeyIsHeldForAtLeastOneFrame) {
  StepPilot pilot;
  pilot.AddSteps({PilotKeyStep{.key = "a", .frames = 0},
                  PilotKeyStep{.key = "b", .frames = -1}});
  EXPECT_THAT(RunUntilDone(&pilot),
              ElementsAre(ElementsAre(KeyDown("a")), ElementsAre(KeyUp("a")),
                          ElementsAre(KeyDown("b")), ElementsAre(KeyUp("b"))));
}

TEST_F(StepPilotTest, HasKeyNamesOfEveryStepAdded) {
  StepPilot pilot;
  pilot.AddSteps({PilotKeyStep{.key = "a"},
                  PilotKeyStep{.key = "Left Shift", .frames = 2},
                  PilotKeyStep{.key = "a"}});
  EXPECT_THAT(pilot.GetKeyNames(), ElementsAre("a", "Left Shift"));
  pilot.AddSteps({PilotKeyStep{.key = "b"}});
  EXPECT_THAT(pilot.GetKeyNames(), ElementsAre("a", "Left Shift", "b"));
}

//------------------------------------------------------------------------------
// Mouse clicks
//------------------------------------------------------------------------------

TEST_F(StepPilotTest, ClicksWhereTheMouseIs) {
  StepPilot pilot;
  pilot.AddSteps({PilotMouseMoveStep{.position = {5, 6}},
                  PilotMouseClickStep{.button = PilotMouseButton::kRight}});

  pilot.BeginFrame();
  EXPECT_THAT(
      pilot.GetInput(),
      ElementsAre(Move(5, 6), Button(PilotMouseButton::kRight, true, 5, 6)));
  EXPECT_EQ(pilot.GetMouse(), (PilotMouse{.position = {5, 6}, .right = true}));
  pilot.EndFrame();

  pilot.BeginFrame();
  EXPECT_THAT(pilot.GetInput(),
              ElementsAre(Button(PilotMouseButton::kRight, false, 5, 6)));
  EXPECT_EQ(pilot.GetMouse(), (PilotMouse{.position = {5, 6}}));
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
}

TEST_F(StepPilotTest, ClicksAtAPoint) {
  StepPilot pilot;
  pilot.AddSteps({PilotMouseClickStep{.button = PilotMouseButton::kMiddle,
                                      .target = PilotPoint{7, 8}}});

  pilot.BeginFrame();
  EXPECT_THAT(
      pilot.GetInput(),
      ElementsAre(Move(7, 8), Button(PilotMouseButton::kMiddle, true, 7, 8)));
  EXPECT_EQ(pilot.GetMouse(), (PilotMouse{.position = {7, 8}, .middle = true}));
  pilot.EndFrame();

  EXPECT_THAT(
      RunUntilDone(&pilot),
      ElementsAre(ElementsAre(Button(PilotMouseButton::kMiddle, false, 7, 8))));
}

TEST_F(StepPilotTest, ClicksAtAnItem) {
  StepPilot pilot;
  PilotHandle other = pilot.AddItemFinder(FindLabel("Cancel", {1, 2}));
  PilotHandle finder = pilot.AddItemFinder(FindLabel("OK", {30, 40}));
  pilot.AddSteps({PilotMouseClickStep{.target = "OK"}});
  EXPECT_THAT(
      RunUntilDone(&pilot),
      ElementsAre(ElementsAre(Move(30, 40),
                              Button(PilotMouseButton::kLeft, true, 30, 40)),
                  ElementsAre(Button(PilotMouseButton::kLeft, false, 30, 40))));
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, ClickIsReleasedInAFrameOfItsOwn) {
  StepPilot pilot;
  pilot.AddSteps({PilotMouseClickStep{.target = PilotPoint{1, 2}},
                  PilotMouseClickStep{.target = PilotPoint{3, 4}},
                  PilotMouseMoveStep{.position = {5, 6}}});
  EXPECT_THAT(
      RunUntilDone(&pilot),
      ElementsAre(
          ElementsAre(Move(1, 2), Button(PilotMouseButton::kLeft, true, 1, 2)),
          ElementsAre(Button(PilotMouseButton::kLeft, false, 1, 2)),
          ElementsAre(Move(3, 4), Button(PilotMouseButton::kLeft, true, 3, 4)),
          ElementsAre(Button(PilotMouseButton::kLeft, false, 3, 4)),
          ElementsAre(Move(5, 6))));
}

TEST_F(StepPilotTest, ClickIsHeldForItsFrames) {
  StepPilot pilot;
  pilot.AddSteps({PilotMouseClickStep{.button = PilotMouseButton::kRight,
                                      .target = PilotPoint{1, 2},
                                      .frames = 3},
                  PilotMouseClickStep{.frames = 0}});
  EXPECT_THAT(
      RunUntilDone(&pilot),
      ElementsAre(
          ElementsAre(Move(1, 2), Button(PilotMouseButton::kRight, true, 1, 2)),
          IsEmpty(), IsEmpty(),
          ElementsAre(Button(PilotMouseButton::kRight, false, 1, 2)),
          ElementsAre(Button(PilotMouseButton::kLeft, true, 1, 2)),
          ElementsAre(Button(PilotMouseButton::kLeft, false, 1, 2))));
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, ClickAtAMissingLabelFails) {
  StepPilot pilot;
  pilot.AddSteps(
      {PilotMouseClickStep{.target = "OK"}, PilotTextStep{.text = "after"}});
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(IsEmpty()));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(
      log_error_guard_.TakeMessages(),
      ElementsAre(LogText(
          "Pilot frame 0: click \"OK\" failed: No item is labelled \"OK\"")));
}

TEST_F(StepPilotTest, ClickAtAnAmbiguousLabelFails) {
  StepPilot pilot;
  PilotHandle finder_1 = pilot.AddItemFinder(FindLabel("OK", {1, 2}));
  PilotHandle finder_2 = pilot.AddItemFinder(FindLabel("OK", {3, 4}));
  pilot.AddSteps({PilotMouseClickStep{.target = "OK"}});
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(IsEmpty()));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("2 items are labelled \"OK\"")));
}

//------------------------------------------------------------------------------
// Commands and conditions
//------------------------------------------------------------------------------

TEST_F(StepPilotTest, RunsCommandsWithValues) {
  StepPilot pilot;
  std::vector<std::string> args;
  PilotHandle command = pilot.AddCommand(
      "record", [&args](absl::Span<const std::string> run_args) {
        args.assign(run_args.begin(), run_args.end());
        return absl::OkStatus();
      });
  pilot.AddSteps({PilotRunStep{.command = "record", .args = {"one", "2"}}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(1));
  EXPECT_THAT(args, ElementsAre("one", "2"));
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, MissingCommandFails) {
  StepPilot pilot;
  pilot.AddSteps({PilotRunStep{.command = "missing", .args = {"two words"}}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(1));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: run missing \"two words\" "
                                  "failed: No command named \"missing\"")));
}

TEST_F(StepPilotTest, FailingCommandFailsAndEndsTheRun) {
  StepPilot pilot;
  PilotHandle command =
      pilot.AddCommand("broken", [](absl::Span<const std::string>) {
        return absl::InternalError("Broken");
      });
  pilot.AddSteps({PilotWaitStep{.frames = 1}, PilotRunStep{.command = "broken"},
                  PilotTextStep{.text = "after"}});
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(IsEmpty(), IsEmpty()));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 1: run broken failed: Broken")));
}

TEST_F(StepPilotTest, WaitForFinishesAfterTheFrameTheConditionHolds) {
  StepPilot pilot;
  bool loaded = false;
  PilotHandle condition =
      pilot.AddCondition("loaded", [&loaded] { return loaded; });
  pilot.AddSteps(
      {PilotWaitForStep{.condition = "loaded"}, PilotTextStep{.text = "a"}});

  EXPECT_THAT(RunFrame(&pilot), IsEmpty());
  pilot.BeginFrame();
  loaded = true;
  pilot.EndFrame();
  EXPECT_THAT(RunFrame(&pilot), ElementsAre(Text("a")));
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, WaitForTakesNoTimeIfTheConditionHolds) {
  StepPilot pilot;
  PilotHandle condition = pilot.AddCondition("loaded", [] { return true; });
  pilot.AddSteps(
      {PilotWaitForStep{.condition = "loaded"}, PilotTextStep{.text = "a"}});
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(ElementsAre(Text("a"))));
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, WaitForTimesOut) {
  StepPilot pilot;
  PilotHandle condition = pilot.AddCondition("loaded", [] { return false; });
  pilot.AddSteps({PilotWaitForStep{.condition = "loaded", .max_frames = 3},
                  PilotTextStep{.text = "a"}});
  EXPECT_THAT(RunUntilDone(&pilot),
              ElementsAre(IsEmpty(), IsEmpty(), IsEmpty()));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 2: wait for loaded 3 failed: "
                                  "The condition doesn't hold after 3 "
                                  "frames")));
}

TEST_F(StepPilotTest, WaitForNoFramesFailsAtOnce) {
  StepPilot pilot;
  PilotHandle condition = pilot.AddCondition("loaded", [] { return false; });
  pilot.AddSteps({PilotWaitForStep{.condition = "loaded", .max_frames = 0}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(1));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: wait for loaded 0 failed: "
                                  "The condition doesn't hold after 0 "
                                  "frames")));
}

TEST_F(StepPilotTest, ExpectPassesIfTheConditionHolds) {
  StepPilot pilot;
  PilotHandle condition = pilot.AddCondition("loaded", [] { return true; });
  pilot.AddSteps(
      {PilotExpectStep{.condition = "loaded"}, PilotTextStep{.text = "a"}});
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(ElementsAre(Text("a"))));
  EXPECT_FALSE(pilot.HasFailed());
}

TEST_F(StepPilotTest, ExpectFailsIfTheConditionDoesntHold) {
  StepPilot pilot;
  PilotHandle condition = pilot.AddCondition("loaded", [] { return false; });
  pilot.AddSteps({PilotExpectStep{.condition = "loaded"}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(1));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: expect loaded failed: The "
                                  "condition doesn't hold")));
}

TEST_F(StepPilotTest, MissingConditionFails) {
  StepPilot pilot;
  pilot.AddSteps({PilotWaitForStep{.condition = "missing"}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(1));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("No condition named \"missing\"")));
}

//------------------------------------------------------------------------------
// Saving frames
//------------------------------------------------------------------------------

// A frame saver that keeps the name and first pixel of each frame.
class FrameRecorder {
 public:
  struct Frame {
    bool operator==(const Frame&) const = default;

    std::string name;
    Pixel pixel;
  };

  StepPilot::FrameSaver GetSaver(bool result = true) {
    return [this, result](std::string_view name, const Image& image) {
      frames_.push_back({std::string(name), image.GetPixels().front()});
      return result;
    };
  }

  const std::vector<Frame>& GetFrames() const { return frames_; }

 private:
  std::vector<Frame> frames_;
};

TEST_F(StepPilotTest, SavesTheFrameTheStepStartsIn) {
  FrameRecorder recorder;
  StepPilot pilot({.save_frame = recorder.GetSaver()});
  pilot.AddSteps({PilotSaveStep{.name = "first"}, PilotTextStep{.text = "a"}});
  const Image image(2, 1, Pixel(1, 2, 3, 4));
  EXPECT_THAT(RunFrame(&pilot, &image), IsEmpty());
  EXPECT_THAT(RunFrame(&pilot, &image), ElementsAre(Text("a")));
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(recorder.GetFrames(),
              ElementsAre(FrameRecorder::Frame{"first", Pixel(1, 2, 3, 4)}));
}

TEST_F(StepPilotTest, SavesTheNextFrameDrawn) {
  FrameRecorder recorder;
  StepPilot pilot({.save_frame = recorder.GetSaver()});
  pilot.AddSteps({PilotSaveStep{.name = "first"}});
  const Image image(2, 1, Pixel(1, 2, 3, 4));
  RunFrame(&pilot);
  EXPECT_FALSE(pilot.IsDone());
  RunFrame(&pilot, &image);
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(recorder.GetFrames(),
              ElementsAre(FrameRecorder::Frame{"first", Pixel(1, 2, 3, 4)}));
}

TEST_F(StepPilotTest, SaveFailsWithoutAFrameSaver) {
  StepPilot pilot;
  pilot.AddSteps({PilotSaveStep{.name = "first"}});
  pilot.BeginFrame();
  EXPECT_FALSE(pilot.IsSavingFrame());
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: save first failed: No frame "
                                  "saver was given")));
}

TEST_F(StepPilotTest, SaveFailsWhenTheFrameSaverFails) {
  FrameRecorder recorder;
  StepPilot pilot({.save_frame = recorder.GetSaver(false)});
  pilot.AddSteps({PilotSaveStep{.name = "first"}});
  const Image image(2, 1, Pixel(1, 2, 3, 4));
  RunFrame(&pilot, &image);
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(recorder.GetFrames(), SizeIs(1));
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("save first failed")));
}

TEST_F(StepPilotTest, SaveFailsWhenNoFrameIsDrawn) {
  FrameRecorder recorder;
  StepPilot pilot({.save_frame = recorder.GetSaver()});
  pilot.AddSteps({PilotSaveStep{.name = "first"}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(kMaxSaveFrames));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(recorder.GetFrames(), IsEmpty());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("No frame was drawn in 600 frames")));
}

//------------------------------------------------------------------------------
// Adding steps
//------------------------------------------------------------------------------

TEST_F(StepPilotTest, RunsStepsAddedBetweenFrames) {
  StepPilot pilot;
  pilot.AddSteps({PilotKeyStep{.key = "a"}});
  EXPECT_THAT(RunFrame(&pilot), ElementsAre(KeyDown("a")));
  pilot.AddSteps({PilotTextStep{.text = "b"}});
  EXPECT_THAT(RunUntilDone(&pilot),
              ElementsAre(ElementsAre(KeyUp("a")), ElementsAre(Text("b"))));
}

TEST_F(StepPilotTest, RunsStepsAddedByACommand) {
  StepPilot pilot;
  PilotHandle command =
      pilot.AddCommand("more", [&pilot](absl::Span<const std::string>) {
        pilot.AddSteps({PilotTextStep{.text = "b"}});
        return absl::OkStatus();
      });
  pilot.AddSteps({PilotRunStep{.command = "more"}});
  EXPECT_THAT(RunUntilDone(&pilot), ElementsAre(ElementsAre(Text("b"))));
}

TEST_F(StepPilotTest, IgnoresStepsAddedOnceDone) {
  StepPilot pilot;
  RunUntilDone(&pilot);
  pilot.AddSteps({PilotTextStep{.text = "a"}});
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("added after the run was done")));
}

//------------------------------------------------------------------------------
// Expected errors
//------------------------------------------------------------------------------

TEST_F(StepPilotTest, ExpectedErrorInTheStepsFramePasses) {
  LogRecorder log_recorder;
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{.text = "edited", .max_frames = 1},
                  PilotWaitStep{.frames = 1}});
  pilot.BeginFrame();
  LOG(ERROR) << "Texture cannot be edited";
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Texture cannot be edited")));
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: expect error \"edited\" 1"),
                          LogText("Pilot frame 0: wait 1"),
                          LogText("Texture cannot be edited"),
                          LogText("Pilot frame 0: The error was expected, by "
                                  "expect error \"edited\" 1"),
                          LogTextHas("Pilot run passed")));
}

TEST_F(StepPilotTest, ExpectedErrorsInLaterFramesPass) {
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{.text = "edited", .max_frames = 3},
                  PilotWaitStep{.frames = 3}});
  RunFrame(&pilot);
  pilot.BeginFrame();
  LOG(ERROR) << "Texture cannot be edited";
  pilot.EndFrame();
  LOG(ERROR) << "Texture cannot be edited, again";
  RunFrame(&pilot);
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(), SizeIs(2));
}

TEST_F(StepPilotTest, ExpectedErrorOnAnotherThreadPasses) {
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{.text = "edited", .max_frames = 2},
                  PilotWaitStep{.frames = 2}});
  RunFrame(&pilot);
  std::thread([] { LOG(ERROR) << "Texture cannot be edited"; }).join();
  RunFrame(&pilot);
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(), SizeIs(1));
}

TEST_F(StepPilotTest, ExpectedErrorWithNoFramesLastsItsFrame) {
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{.text = "edited", .max_frames = 0},
                  PilotWaitStep{.frames = 1}});
  pilot.BeginFrame();
  LOG(ERROR) << "Texture cannot be edited";
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(), SizeIs(1));
}

TEST_F(StepPilotTest, ErrorBeforeTheExpectErrorStepFails) {
  StepPilot pilot;
  PilotHandle command =
      pilot.AddCommand("edit", [](absl::Span<const std::string>) {
        LOG(ERROR) << "Texture cannot be edited";
        return absl::OkStatus();
      });
  pilot.AddSteps({PilotRunStep{.command = "edit"},
                  PilotExpectErrorStep{.text = "edited"},
                  PilotWaitStep{.frames = 1}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(1));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Texture cannot be edited"),
                          LogText("Pilot frame 0: An error was logged: "
                                  "Texture cannot be edited")));
}

TEST_F(StepPilotTest, ErrorAfterTheExpectationsFramesFails) {
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{.text = "edited", .max_frames = 1},
                  PilotWaitStep{.frames = 3}});
  pilot.BeginFrame();
  LOG(ERROR) << "Texture cannot be edited";
  pilot.EndFrame();
  pilot.BeginFrame();
  LOG(ERROR) << "Texture cannot be edited";
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Texture cannot be edited"),
                          LogText("Texture cannot be edited"),
                          LogText("Pilot frame 1: An error was logged: "
                                  "Texture cannot be edited")));
}

TEST_F(StepPilotTest, ErrorWithOtherTextFails) {
  StepPilot pilot;
  pilot.AddSteps(
      {PilotExpectErrorStep{.text = "edited"}, PilotWaitStep{.frames = 2}});
  pilot.BeginFrame();
  LOG(ERROR) << "Broken";
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(
      log_error_guard_.TakeMessages(),
      ElementsAre(LogText("Broken"),
                  LogText("Pilot frame 0: An error was logged: Broken")));
}

TEST_F(StepPilotTest, ExpectedErrorNotLoggedInItsFramesFails) {
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{.text = "edited", .max_frames = 2},
                  PilotWaitStep{.frames = 5}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(2));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 1: expect error \"edited\" 2 "
                                  "failed: No error with its text was logged "
                                  "in 2 frames")));
}

TEST_F(StepPilotTest, ExpectedErrorNotLoggedBeforeTheRunEndsFails) {
  StepPilot pilot;
  pilot.AddSteps(
      {PilotExpectErrorStep{.text = "edited"}, PilotWaitStep{.frames = 1}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(1));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: expect error \"edited\" 600 "
                                  "failed: No error with its text was logged "
                                  "before the run ended")));
}

TEST_F(StepPilotTest, ExpectedErrorWithNoTextFails) {
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{}, PilotWaitStep{.frames = 1}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(1));
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: expect error \"\" 600 "
                                  "failed: No text to expect")));
}

TEST_F(StepPilotTest, ExpectationsCanOverlap) {
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{.text = "edited", .max_frames = 2},
                  PilotWaitStep{.frames = 1},
                  PilotExpectErrorStep{.text = "vertices", .max_frames = 2},
                  PilotWaitStep{.frames = 2}});
  RunFrame(&pilot);
  pilot.BeginFrame();
  LOG(ERROR) << "Texture cannot be edited";
  LOG(ERROR) << "GUI has too many vertices";
  pilot.EndFrame();
  RunFrame(&pilot);
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(), SizeIs(2));
}

TEST_F(StepPilotTest, OneErrorMeetsEveryExpectationItMatches) {
  StepPilot pilot;
  pilot.AddSteps({PilotExpectErrorStep{.text = "cannot", .max_frames = 1},
                  PilotExpectErrorStep{.text = "edited", .max_frames = 1},
                  PilotWaitStep{.frames = 1}});
  pilot.BeginFrame();
  LOG(ERROR) << "Texture cannot be edited";
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(), SizeIs(1));
}

//------------------------------------------------------------------------------
// Logging
//------------------------------------------------------------------------------

TEST_F(StepPilotTest, ErrorLoggedOnAnotherThreadFailsTheRun) {
  StepPilot pilot;
  pilot.AddSteps({PilotWaitStep{.frames = 3}});
  RunFrame(&pilot);
  std::thread([] { LOG(ERROR) << "Broken"; }).join();
  RunFrame(&pilot);
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogText("Broken"),
                          LogText("Pilot frame 1: An error was logged: "
                                  "Broken")));
}

TEST_F(StepPilotTest, ErrorLoggedInTheLastFrameFailsTheRun) {
  StepPilot pilot;
  pilot.AddSteps({PilotKeyStep{.key = "a"}});
  RunFrame(&pilot);
  pilot.BeginFrame();
  EXPECT_FALSE(pilot.IsDone());
  LOG(ERROR) << "Broken";
  pilot.EndFrame();
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_THAT(log_error_guard_.TakeMessages(), SizeIs(2));
}

TEST_F(StepPilotTest, LogsEachStepAndASummary) {
  LogRecorder log_recorder;
  StepPilot pilot;
  pilot.AddSteps({PilotKeyStep{.key = "a"}, PilotTextStep{.text = "hi there"},
                  PilotKeyStep{.key = "Left Shift", .frames = 2},
                  PilotMouseMoveStep{.position = {1, 2}},
                  PilotMouseClickStep{.button = PilotMouseButton::kRight,
                                      .target = PilotPoint{3, 4}},
                  PilotMouseWheelStep{.amount = -1},
                  PilotWaitStep{.frames = 1}});
  EXPECT_THAT(RunUntilDone(&pilot), SizeIs(8));
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: key a"),
                          LogText("Pilot frame 2: text \"hi there\""),
                          LogText("Pilot frame 2: key \"Left Shift\" for 2"),
                          LogText("Pilot frame 5: move 1 2"),
                          LogText("Pilot frame 5: click right 3 4"),
                          LogText("Pilot frame 7: wheel -1"),
                          LogText("Pilot frame 7: wait 1"),
                          LogText("Pilot run passed. Frames run: 8. Frames "
                                  "saved: 0.")));
}

TEST_F(StepPilotTest, LogsASummaryOfAFailedRun) {
  FrameRecorder recorder;
  StepPilot pilot({.save_frame = recorder.GetSaver()});
  pilot.AddSteps({PilotSaveStep{.name = "first"},
                  PilotExpectStep{.condition = "missing"}});
  const Image image(2, 1, Pixel(1, 2, 3, 4));
  LogRecorder log_recorder(absl::LogSeverityAtLeast::kInfo);
  RunFrame(&pilot, &image);
  RunFrame(&pilot, &image);
  EXPECT_TRUE(pilot.HasFailed());
  log_error_guard_.TakeMessages();
  EXPECT_THAT(log_recorder.TakeMessages(),
              ElementsAre(LogText("Pilot frame 0: save first"),
                          LogText("Pilot frame 1: expect missing"),
                          LogTextHas("expect missing failed"),
                          LogText("Pilot run failed. Frames run: 2. Frames "
                                  "saved: 1.")));
}

}  // namespace
}  // namespace gb
