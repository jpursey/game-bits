// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/pilot.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gb/image/image.h"
#include "gb/pilot/fake_pilot.h"
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

// Returns a command that records the values it is run with.
Pilot::Command RecordArgs(std::vector<std::string>* args) {
  return [args](absl::Span<const std::string> run_args) {
    args->assign(run_args.begin(), run_args.end());
    return absl::OkStatus();
  };
}

// Returns an item finder that finds `point` for each label in `labels`.
Pilot::ItemFinder FindLabels(std::vector<std::string_view> labels,
                             PilotPoint point) {
  return [labels, point](std::string_view label) {
    std::vector<PilotPoint> found;
    for (std::string_view item_label : labels) {
      if (item_label == label) {
        found.push_back(point);
      }
    }
    return found;
  };
}

TEST(PilotTest, RunsCommandWithValues) {
  FakePilot pilot;
  std::vector<std::string> args;
  PilotHandle handle = pilot.AddCommand("record", RecordArgs(&args));
  const std::vector<std::string> run_args = {"one", "2"};
  EXPECT_TRUE(pilot.RunCommand("record", run_args).ok());
  EXPECT_THAT(args, ElementsAre("one", "2"));
  EXPECT_TRUE(pilot.RunCommand("record", {}).ok());
  EXPECT_THAT(args, IsEmpty());
}

TEST(PilotTest, CommandReturnsItsError) {
  FakePilot pilot;
  PilotHandle handle =
      pilot.AddCommand("fail", [](absl::Span<const std::string>) {
        return absl::InternalError("Command failed");
      });
  const absl::Status status = pilot.RunCommand("fail", {});
  EXPECT_EQ(status.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(status.message(), "Command failed");
}

TEST(PilotTest, MissingCommandFails) {
  FakePilot pilot;
  std::vector<std::string> args;
  PilotHandle handle = pilot.AddCommand("record", RecordArgs(&args));
  const absl::Status status = pilot.RunCommand("missing", {});
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
  EXPECT_THAT(status.message(), HasSubstr("\"missing\""));
}

TEST(PilotTest, ChecksCondition) {
  FakePilot pilot;
  bool loaded = false;
  PilotHandle handle =
      pilot.AddCondition("loaded", [&loaded] { return loaded; });
  absl::StatusOr<bool> result = pilot.CheckCondition("loaded");
  ASSERT_TRUE(result.ok());
  EXPECT_FALSE(*result);
  loaded = true;
  result = pilot.CheckCondition("loaded");
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(*result);
}

TEST(PilotTest, MissingConditionFails) {
  FakePilot pilot;
  PilotHandle handle = pilot.AddCondition("loaded", [] { return true; });
  const absl::StatusOr<bool> result = pilot.CheckCondition("missing");
  EXPECT_EQ(result.status().code(), absl::StatusCode::kNotFound);
  EXPECT_THAT(result.status().message(), HasSubstr("\"missing\""));
}

TEST(PilotTest, CommandsAndConditionsHaveSeparateNames) {
  FakePilot pilot;
  std::vector<std::string> args;
  PilotHandle command = pilot.AddCommand("name", RecordArgs(&args));
  PilotHandle condition = pilot.AddCondition("name", [] { return true; });
  EXPECT_TRUE(pilot.RunCommand("name", {}).ok());
  EXPECT_TRUE(pilot.CheckCondition("name").ok());
}

TEST(PilotTest, DuplicateCommandLogsError) {
  LogErrorGuard log_error_guard;
  FakePilot pilot;
  std::vector<std::string> first_args;
  std::vector<std::string> second_args;
  PilotHandle first = pilot.AddCommand("record", RecordArgs(&first_args));
  PilotHandle second = pilot.AddCommand("record", RecordArgs(&second_args));
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("command \"record\" is already "
                                          "registered"))));

  // The second handle holds nothing, so destroying it leaves the first.
  second = {};
  const std::vector<std::string> run_args = {"value"};
  EXPECT_TRUE(pilot.RunCommand("record", run_args).ok());
  EXPECT_THAT(first_args, ElementsAre("value"));
  EXPECT_THAT(second_args, IsEmpty());
}

TEST(PilotTest, DuplicateConditionLogsError) {
  LogErrorGuard log_error_guard;
  FakePilot pilot;
  PilotHandle first = pilot.AddCondition("loaded", [] { return true; });
  PilotHandle second = pilot.AddCondition("loaded", [] { return false; });
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("condition \"loaded\" is already "
                                          "registered"))));
  second = {};
  const absl::StatusOr<bool> result = pilot.CheckCondition("loaded");
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(*result);
}

TEST(PilotTest, FindsItemsFromEachFinder) {
  FakePilot pilot;
  PilotHandle first =
      pilot.AddItemFinder(FindLabels({"Open", "Save"}, {10, 20}));
  PilotHandle second = pilot.AddItemFinder(FindLabels({"Close"}, {30, 40}));
  absl::StatusOr<PilotPoint> point = pilot.FindItem("Save");
  ASSERT_TRUE(point.ok()) << point.status();
  EXPECT_EQ(*point, (PilotPoint{10, 20}));
  point = pilot.FindItem("Close");
  ASSERT_TRUE(point.ok()) << point.status();
  EXPECT_EQ(*point, (PilotPoint{30, 40}));
}

TEST(PilotTest, MissingItemFails) {
  FakePilot pilot;
  PilotHandle handle = pilot.AddItemFinder(FindLabels({"Open"}, {10, 20}));
  const absl::StatusOr<PilotPoint> point = pilot.FindItem("Save");
  EXPECT_EQ(point.status().code(), absl::StatusCode::kNotFound);
  EXPECT_THAT(point.status().message(), HasSubstr("\"Save\""));
}

TEST(PilotTest, ItemWithoutFindersFails) {
  FakePilot pilot;
  EXPECT_EQ(pilot.FindItem("Save").status().code(),
            absl::StatusCode::kNotFound);
}

TEST(PilotTest, ItemTwoFindersFindIsAmbiguous) {
  FakePilot pilot;
  PilotHandle first = pilot.AddItemFinder(FindLabels({"Open"}, {10, 20}));
  PilotHandle second = pilot.AddItemFinder(FindLabels({"Open"}, {30, 40}));
  const absl::StatusOr<PilotPoint> point = pilot.FindItem("Open");
  EXPECT_EQ(point.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(point.status().message(),
              HasSubstr("2 items are labelled \"Open\""));
}

TEST(PilotTest, ItemOneFinderFindsTwiceIsAmbiguous) {
  FakePilot pilot;
  PilotHandle handle =
      pilot.AddItemFinder(FindLabels({"Open", "Open"}, {10, 20}));
  EXPECT_EQ(pilot.FindItem("Open").status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(PilotTest, DestroyingHandlesRemovesRegistrations) {
  FakePilot pilot;
  std::vector<std::string> args;
  PilotHandle command = pilot.AddCommand("record", RecordArgs(&args));
  PilotHandle condition = pilot.AddCondition("loaded", [] { return true; });
  PilotHandle finder = pilot.AddItemFinder(FindLabels({"Open"}, {10, 20}));
  int listener_calls = 0;
  PilotHandle frame_listener =
      pilot.AddFrameListener([&] { ++listener_calls; });
  PilotHandle other_command = pilot.AddCommand("other", RecordArgs(&args));

  command = {};
  EXPECT_EQ(pilot.RunCommand("record", {}).code(), absl::StatusCode::kNotFound);
  EXPECT_TRUE(pilot.CheckCondition("loaded").ok());
  EXPECT_TRUE(pilot.FindItem("Open").ok());

  condition = {};
  EXPECT_EQ(pilot.CheckCondition("loaded").status().code(),
            absl::StatusCode::kNotFound);
  EXPECT_TRUE(pilot.FindItem("Open").ok());

  finder = {};
  EXPECT_EQ(pilot.FindItem("Open").status().code(),
            absl::StatusCode::kNotFound);
  pilot.BeginFrame();
  pilot.EndFrame();
  EXPECT_EQ(listener_calls, 1);

  frame_listener = {};
  pilot.BeginFrame();
  pilot.EndFrame();
  EXPECT_EQ(listener_calls, 1);
  EXPECT_TRUE(pilot.RunCommand("other", {}).ok());
}

TEST(PilotTest, RemovedNameCanBeAddedAgain) {
  FakePilot pilot;
  std::vector<std::string> first_args;
  std::vector<std::string> second_args;
  PilotHandle handle = pilot.AddCommand("record", RecordArgs(&first_args));
  handle = {};
  handle = pilot.AddCommand("record", RecordArgs(&second_args));
  const std::vector<std::string> run_args = {"value"};
  EXPECT_TRUE(pilot.RunCommand("record", run_args).ok());
  EXPECT_THAT(first_args, IsEmpty());
  EXPECT_THAT(second_args, ElementsAre("value"));
}

TEST(PilotTest, MovingHandleKeepsRegistration) {
  FakePilot pilot;
  std::vector<std::string> args;
  PilotHandle moved_to;
  {
    PilotHandle handle = pilot.AddCommand("record", RecordArgs(&args));
    PilotHandle constructed(std::move(handle));
    moved_to = std::move(constructed);
  }
  EXPECT_TRUE(pilot.RunCommand("record", {}).ok());
  moved_to = {};
  EXPECT_EQ(pilot.RunCommand("record", {}).code(), absl::StatusCode::kNotFound);
}

TEST(PilotTest, MovingOntoHandleRemovesItsRegistration) {
  FakePilot pilot;
  std::vector<std::string> args;
  PilotHandle first = pilot.AddCommand("first", RecordArgs(&args));
  PilotHandle second = pilot.AddCommand("second", RecordArgs(&args));
  first = std::move(second);
  EXPECT_EQ(pilot.RunCommand("first", {}).code(), absl::StatusCode::kNotFound);
  EXPECT_TRUE(pilot.RunCommand("second", {}).ok());
}

TEST(PilotTest, CommandCanRemoveItself) {
  FakePilot pilot;
  auto state = std::make_shared<int>(0);
  PilotHandle handle;
  handle =
      pilot.AddCommand("once", [&handle, state](absl::Span<const std::string>) {
        handle = {};

        // The command is still alive while it runs.
        ++*state;
        return absl::OkStatus();
      });
  EXPECT_TRUE(pilot.RunCommand("once", {}).ok());
  EXPECT_EQ(*state, 1);
  EXPECT_EQ(state.use_count(), 1);
  EXPECT_EQ(pilot.RunCommand("once", {}).code(), absl::StatusCode::kNotFound);
}

TEST(PilotTest, CommandCanHoldHandles) {
  FakePilot pilot;
  std::vector<std::string> args;
  auto held = std::make_shared<PilotHandle>(
      pilot.AddCommand("held", RecordArgs(&args)));
  PilotHandle holder = pilot.AddCommand(
      "holder",
      [held](absl::Span<const std::string>) { return absl::OkStatus(); });
  held.reset();

  // Removing the command destroys the handle it holds, which removes its
  // command too.
  holder = {};
  EXPECT_EQ(pilot.RunCommand("holder", {}).code(), absl::StatusCode::kNotFound);
  EXPECT_EQ(pilot.RunCommand("held", {}).code(), absl::StatusCode::kNotFound);
}

TEST(PilotTest, ItemFinderCanAddAndRemoveFinders) {
  FakePilot pilot;
  PilotHandle changer;
  PilotHandle removed;
  PilotHandle added;
  changer = pilot.AddItemFinder([&](std::string_view) {
    changer = {};
    removed = {};
    added = pilot.AddItemFinder(FindLabels({"Open"}, {50, 60}));
    return std::vector<PilotPoint>();
  });
  removed = pilot.AddItemFinder(FindLabels({"Open"}, {30, 40}));

  // A finder removed while the item is looked for isn't asked, and one added
  // isn't asked until the next time.
  EXPECT_EQ(pilot.FindItem("Open").status().code(),
            absl::StatusCode::kNotFound);
  const absl::StatusOr<PilotPoint> point = pilot.FindItem("Open");
  ASSERT_TRUE(point.ok()) << point.status();
  EXPECT_EQ(*point, (PilotPoint{50, 60}));
}

TEST(PilotTest, BeginFrameCallsFrameListenersInOrderWithTheFrameSetUp) {
  FakePilot pilot;
  std::vector<std::string> calls;
  PilotHandle first = pilot.AddFrameListener([&] {
    calls.push_back(absl::StrCat("first ", pilot.GetInput().size()));
  });
  PilotHandle second = pilot.AddFrameListener([&] {
    calls.push_back(absl::StrCat("second ", pilot.GetInput().size()));
  });
  EXPECT_THAT(calls, IsEmpty());

  pilot.SetNextFrameInput({PilotTextEvent{.text = "a"}});
  pilot.BeginFrame();
  EXPECT_THAT(calls, ElementsAre("first 1", "second 1"));
  pilot.EndFrame();
  EXPECT_THAT(calls, ElementsAre("first 1", "second 1"));
}

TEST(PilotTest, FrameListenerCanAddAndRemoveListeners) {
  FakePilot pilot;
  int removed_calls = 0;
  int added_calls = 0;
  PilotHandle changer;
  PilotHandle removed;
  PilotHandle added;
  changer = pilot.AddFrameListener([&] {
    changer = {};
    removed = {};
    added = pilot.AddFrameListener([&] { ++added_calls; });
  });
  removed = pilot.AddFrameListener([&] { ++removed_calls; });

  // A listener removed during a frame isn't called, and one added isn't called
  // until the next frame.
  pilot.BeginFrame();
  pilot.EndFrame();
  EXPECT_EQ(removed_calls, 0);
  EXPECT_EQ(added_calls, 0);
  pilot.BeginFrame();
  pilot.EndFrame();
  EXPECT_EQ(added_calls, 1);
}

TEST(PilotTest, CountsFrames) {
  FakePilot pilot;
  EXPECT_EQ(pilot.GetFrame(), 0);
  pilot.BeginFrame();
  EXPECT_EQ(pilot.GetFrame(), 0);
  pilot.EndFrame();
  EXPECT_EQ(pilot.GetFrame(), 1);
  pilot.BeginFrame();
  EXPECT_EQ(pilot.GetFrame(), 1);
  pilot.EndFrame();
  EXPECT_EQ(pilot.GetFrame(), 2);
}

TEST(PilotTest, BeginFrameTwiceIsDebugFatal) {
  FakePilot pilot;
  pilot.BeginFrame();
  EXPECT_DEBUG_DEATH(pilot.BeginFrame(), "before EndFrame");
}

TEST(PilotTest, EndFrameWithoutBeginFrameIsDebugFatal) {
  FakePilot pilot;
  EXPECT_DEBUG_DEATH(pilot.EndFrame(), "without BeginFrame");
  pilot.BeginFrame();
  pilot.EndFrame();
  EXPECT_DEBUG_DEATH(pilot.EndFrame(), "without BeginFrame");
}

TEST(PilotTest, SaveFrameNotBeingSavedIsDebugFatal) {
  FakePilot pilot;
  Image image(1, 1);
  pilot.BeginFrame();
  EXPECT_DEBUG_DEATH(pilot.SaveFrame(image), "not being saved");
}

TEST(PilotTest, SaveFrameOutsideFrameIsDebugFatal) {
  FakePilot pilot;
  pilot.SetSavingFrames(true);
  Image image(1, 1);
  EXPECT_DEBUG_DEATH(pilot.SaveFrame(image), "outside a frame");
}

TEST(PilotTest, SaveFrameTwiceIsDebugFatal) {
  FakePilot pilot;
  pilot.SetSavingFrames(true);
  Image image(1, 1);
  pilot.BeginFrame();
  pilot.SaveFrame(image);
  EXPECT_DEBUG_DEATH(pilot.SaveFrame(image), "again in the same frame");
  pilot.EndFrame();

  // Each frame can be saved once.
  pilot.BeginFrame();
  pilot.SaveFrame(image);
  pilot.EndFrame();
}

#ifndef NDEBUG
TEST(PilotTest, HandleOutlivingPilotIsDebugFatal) {
  EXPECT_DEATH(
      {
        PilotHandle handle;
        {
          FakePilot pilot;
          handle = pilot.AddCondition("loaded", [] { return true; });
        }
      },
      "outlived its pilot");
}
#endif

}  // namespace
}  // namespace gb
