// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profiler.h"

#include <thread>

#include "absl/time/time.h"
#include "gb/profile/fake_ticks.h"
#include "gb/profile/profile_point.h"
#include "gb/profile/profile_timer.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Points are registered for the whole program, so each test uses names of its
// own. Every tick is a nanosecond.

class ProfilerTest : public ::testing::Test {
 protected:
  FakeTicks ticks_;
  Profiler profiler_{{.fake_ticks = &ticks_}};
};

TEST_F(ProfilerTest, OnePerThread) {
  EXPECT_DEATH(Profiler({.fake_ticks = &ticks_}),
               "Only one Profiler may exist per thread");
}

TEST_F(ProfilerTest, EachThreadRecordsItsOwn) {
  ProfileCount<"ProfilerTest/EachThread">(1);

  int64_t other_count = 0;
  std::thread thread([&other_count] {
    FakeTicks other_ticks;
    Profiler other_profiler({.fake_ticks = &other_ticks});
    ProfileCount<"ProfilerTest/EachThread">(2);
    other_count = other_profiler.GetCount("ProfilerTest/EachThread");
  });
  thread.join();

  EXPECT_EQ(profiler_.GetCount("ProfilerTest/EachThread"), 1);
  EXPECT_EQ(other_count, 2);
}

TEST_F(ProfilerTest, UnknownPointsAreZero) {
  EXPECT_EQ(profiler_.GetCount("ProfilerTest/Unknown"), 0);
  EXPECT_EQ(profiler_.GetSelfTime("ProfilerTest/Unknown"),
            absl::ZeroDuration());
  EXPECT_EQ(profiler_.GetValue("ProfilerTest/Unknown"), 0);
}

TEST_F(ProfilerTest, ReadingWrongKindDies) {
  ProfilePoint counter(ProfilePoint::Kind::kCounter, "ProfilerTest/Counter");
  ProfilePoint value(ProfilePoint::Kind::kValue, "ProfilerTest/Value");
  EXPECT_DEATH(profiler_.GetSelfTime("ProfilerTest/Counter"), "wrong kind");
  EXPECT_DEATH(profiler_.GetValue("ProfilerTest/Counter"), "wrong kind");
  EXPECT_DEATH(profiler_.GetCount("ProfilerTest/Value"), "wrong kind");
}

TEST_F(ProfilerTest, ResetClearsEverything) {
  {
    ProfileScope<"ProfilerTest/ResetScope"> scope;
    ticks_.Advance(10);
  }
  ProfileCount<"ProfilerTest/ResetCount">(1);
  ProfileSetValue<"ProfilerTest/ResetValue">(2);

  profiler_.Reset();
  EXPECT_EQ(profiler_.GetCount("ProfilerTest/ResetScope"), 0);
  EXPECT_EQ(profiler_.GetSelfTime("ProfilerTest/ResetScope"),
            absl::ZeroDuration());
  EXPECT_EQ(profiler_.GetCount("ProfilerTest/ResetCount"), 0);
  EXPECT_EQ(profiler_.GetValue("ProfilerTest/ResetValue"), 0);
}

TEST_F(ProfilerTest, ResetInTimerDies) {
  ProfileScope<"ProfilerTest/ResetInTimer"> scope;
  EXPECT_DEATH(profiler_.Reset(), "can't be reset in a timed point");
}

// Tests that set up their own Profiler.

TEST(ProfilerSetupTest, NewProfilerStartsEmpty) {
  FakeTicks ticks;
  {
    Profiler profiler({.fake_ticks = &ticks});
    ProfileCount<"ProfilerTest/StartsEmpty">(1);
  }
  Profiler profiler({.fake_ticks = &ticks});
  EXPECT_EQ(profiler.GetCount("ProfilerTest/StartsEmpty"), 0);
}

TEST(ProfilerSetupTest, OtherTicksPerSecond) {
  FakeTicks ticks(1'000);
  Profiler profiler({.fake_ticks = &ticks});
  {
    ProfileScope<"ProfilerTest/Milliseconds"> scope;
    ticks.Advance(3);
  }
  EXPECT_EQ(profiler.GetSelfTime("ProfilerTest/Milliseconds"),
            absl::Milliseconds(3));
}

// Only checks that the real timestamp counter is read. How long anything takes
// isn't checked, as no test depends on real time.
TEST(ProfilerSetupTest, RealTicks) {
  Profiler profiler;
  {
    ProfileScope<"ProfilerTest/RealTicks"> scope;
  }
  EXPECT_EQ(profiler.GetCount("ProfilerTest/RealTicks"), 1);
  EXPECT_GT(profiler.GetSelfTime("ProfilerTest/RealTicks"),
            absl::ZeroDuration());
}

}  // namespace
}  // namespace gb
