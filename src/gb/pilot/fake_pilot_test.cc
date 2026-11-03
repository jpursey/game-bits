// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/fake_pilot.h"

#include <memory>
#include <vector>

#include "absl/time/time.h"
#include "gb/image/image.h"
#include "gb/image/pixel.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(FakePilotTest, Defaults) {
  FakePilot pilot;
  EXPECT_FALSE(pilot.IsDone());
  EXPECT_FALSE(pilot.HasFailed());
  EXPECT_EQ(pilot.GetDeltaTime(), absl::Seconds(1) / 60);
  EXPECT_FALSE(pilot.IsWindowHidden());
  EXPECT_THAT(pilot.GetKeyNames(), IsEmpty());
  EXPECT_THAT(pilot.GetInput(), IsEmpty());
  EXPECT_EQ(pilot.GetMouse(), PilotMouse());
  EXPECT_FALSE(pilot.IsSavingFrame());
}

TEST(FakePilotTest, ReportsState) {
  FakePilot pilot;
  pilot.SetDone(true);
  pilot.SetFailed(true);
  pilot.SetDeltaTime(absl::Milliseconds(10));
  pilot.SetWindowHidden(true);
  pilot.SetKeyNames({"a", "Left Shift"});
  pilot.SetMouse({.position = {1, 2}, .right = true});
  pilot.SetSavingFrames(true);
  EXPECT_TRUE(pilot.IsDone());
  EXPECT_TRUE(pilot.HasFailed());
  EXPECT_EQ(pilot.GetDeltaTime(), absl::Milliseconds(10));
  EXPECT_TRUE(pilot.IsWindowHidden());
  EXPECT_THAT(pilot.GetKeyNames(), ElementsAre("a", "Left Shift"));
  EXPECT_EQ(pilot.GetMouse(), (PilotMouse{.position = {1, 2}, .right = true}));
  EXPECT_TRUE(pilot.IsSavingFrame());
}

TEST(FakePilotTest, InputIsForOneFrame) {
  FakePilot pilot;
  const PilotInput key_down = PilotKeyEvent{.key = "a", .down = true};
  const PilotInput text = PilotTextEvent{.text = "hello"};
  pilot.SetNextFrameInput({key_down, text});
  EXPECT_THAT(pilot.GetInput(), IsEmpty());
  pilot.BeginFrame();
  EXPECT_THAT(pilot.GetInput(), ElementsAre(key_down, text));
  pilot.EndFrame();
  EXPECT_THAT(pilot.GetInput(), IsEmpty());
  pilot.BeginFrame();
  EXPECT_THAT(pilot.GetInput(), IsEmpty());
  pilot.EndFrame();
}

TEST(FakePilotTest, KeepsCopiesOfSavedFrames) {
  FakePilot pilot;
  pilot.SetSavingFrames(true);
  for (const Pixel pixel : {Pixel(1, 2, 3, 4), Pixel(5, 6, 7, 8)}) {
    pilot.BeginFrame();
    Image image(2, 1, pixel);
    pilot.SaveFrame(image);
    pilot.EndFrame();
  }

  std::vector<std::unique_ptr<Image>> frames = pilot.TakeSavedFrames();
  ASSERT_EQ(frames.size(), 2);
  EXPECT_EQ(frames[0]->GetWidth(), 2);
  EXPECT_EQ(frames[0]->GetHeight(), 1);
  EXPECT_THAT(frames[0]->GetPixels(),
              ElementsAre(Pixel(1, 2, 3, 4), Pixel(1, 2, 3, 4)));
  EXPECT_THAT(frames[1]->GetPixels(),
              ElementsAre(Pixel(5, 6, 7, 8), Pixel(5, 6, 7, 8)));
  EXPECT_THAT(pilot.TakeSavedFrames(), IsEmpty());
}

}  // namespace
}  // namespace gb
