// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profile_timer.h"

#include <optional>
#include <thread>

#include "absl/time/time.h"
#include "gb/profile/fake_ticks.h"
#include "gb/profile/profile_point.h"
#include "gb/profile/profiler.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Points are registered for the whole program, so each test uses names of its
// own. Every tick is a nanosecond.

class ProfileTimerTest : public ::testing::Test {
 protected:
  FakeTicks ticks_;
  Profiler profiler_{{.fake_ticks = &ticks_}};
};

TEST_F(ProfileTimerTest, ScopeRecordsCallsAndTime) {
  for (int i = 0; i < 2; ++i) {
    ProfileScope<"TimerTest/Scope"> scope;
    ticks_.Advance(10);
  }
  EXPECT_EQ(profiler_.GetCount("TimerTest/Scope"), 2);
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/Scope"), absl::Nanoseconds(20));
}

TEST_F(ProfileTimerTest, TimerForRegisteredPoint) {
  ProfilePoint point(ProfilePoint::Kind::kScope, "TimerTest/Registered");
  {
    ProfileTimer timer(point);
    ticks_.Advance(10);
  }
  {
    ProfileScope<"TimerTest/Registered"> scope;
    ticks_.Advance(5);
  }
  EXPECT_EQ(profiler_.GetCount("TimerTest/Registered"), 2);
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/Registered"),
            absl::Nanoseconds(15));
}

TEST_F(ProfileTimerTest, NestedTimersRecordSelfTime) {
  {
    ProfileScope<"TimerTest/Outer"> outer;
    ticks_.Advance(1);
    {
      ProfileScope<"TimerTest/Inner"> inner;
      ticks_.Advance(2);
    }
    ticks_.Advance(4);
    {
      ProfileScope<"TimerTest/Inner"> inner;
      ticks_.Advance(8);
    }
    ticks_.Advance(16);
  }
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/Outer"), absl::Nanoseconds(21));
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/Inner"), absl::Nanoseconds(10));
  EXPECT_EQ(profiler_.GetCount("TimerTest/Inner"), 2);
}

TEST_F(ProfileTimerTest, CallbackIsSubtractedFromCall) {
  {
    ProfileScope<"TimerTest/Program"> program;
    ticks_.Advance(1);
    {
      ProfileCall<"TimerTest/System"> system;
      ticks_.Advance(2);
      {
        ProfileScope<"TimerTest/Callback"> callback;
        ticks_.Advance(4);
      }
      ticks_.Advance(8);
    }
    ticks_.Advance(16);
  }
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/Program"), absl::Nanoseconds(17));
  EXPECT_EQ(profiler_.GetCount("TimerTest/System"), 1);
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/System"), absl::Nanoseconds(10));
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/Callback"), absl::Nanoseconds(4));
}

TEST_F(ProfileTimerTest, OtherThreadIsIgnored) {
  std::thread thread([] {
    ProfileScope<"TimerTest/OtherThread"> scope;
    ProfileCount<"TimerTest/OtherThreadCount">(1);
  });
  thread.join();
  EXPECT_EQ(profiler_.GetCount("TimerTest/OtherThread"), 0);
  EXPECT_EQ(profiler_.GetCount("TimerTest/OtherThreadCount"), 0);
}

TEST_F(ProfileTimerTest, EndingOuterTimerFirstDies) {
  std::optional<ProfileScope<"TimerTest/First">> first;
  first.emplace();
  ProfileScope<"TimerTest/Second"> second;
  EXPECT_DEATH(first.reset(), "must end on the thread and fiber");
}

TEST(ProfileTimerNoProfilerTest, IgnoredWithoutProfiler) {
  {
    ProfileScope<"TimerTest/NoProfiler"> scope;
  }
  FakeTicks ticks;
  Profiler profiler({.fake_ticks = &ticks});
  EXPECT_EQ(profiler.GetCount("TimerTest/NoProfiler"), 0);
}

TEST(ProfileTimerNoProfilerTest, TimerStartedWithoutProfilerIsIgnored) {
  ProfileScope<"TimerTest/StartedWithout"> scope;
  FakeTicks ticks;
  Profiler profiler({.fake_ticks = &ticks});
  EXPECT_EQ(profiler.GetCount("TimerTest/StartedWithout"), 0);
}

}  // namespace
}  // namespace gb
