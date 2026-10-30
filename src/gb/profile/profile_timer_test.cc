// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profile_timer.h"

#include <optional>
#include <thread>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "gb/profile/fake_profiler_cpu.h"
#include "gb/profile/profile_point.h"
#include "gb/profile/profiler.h"
#include "gb/thread/fiber.h"
#include "gb/thread/thread.h"
#include "gb/thread/thread_placement.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Points are registered for the whole program, so each test uses names of its
// own. Every tick is a nanosecond.

class ProfileTimerTest : public ::testing::Test {
 protected:
  FakeProfilerCpu ticks_;
  Profiler profiler_{{.fake_cpu = &ticks_}};
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

// A fiber times a scope and counts on one thread, switches away, and does the
// same after resuming on a second thread, in the same function. Each thread has
// its own Profiler, and the first thread keeps its Profiler, and checks it,
// after the second visit is done, so a Profiler found before the switch and
// reused after it would record the second visit.
struct MovingFiberState {
  Fiber moving_fiber = nullptr;
  Fiber return_fiber = nullptr;
  absl::Notification first_visit_done;
  absl::Notification second_visit_done;
};

void RunMovingFiber(void* user_data) {
  auto& state = *static_cast<MovingFiberState*>(user_data);
  {
    ProfileScope<"FiberTimerTest/FirstScope"> scope;
    ProfileCount<"FiberTimerTest/FirstCount">(1);
  }
  SwitchToFiber(state.return_fiber);
  {
    ProfileScope<"FiberTimerTest/SecondScope"> scope;
    ProfileCount<"FiberTimerTest/SecondCount">(1);
  }
  SwitchToFiber(state.return_fiber);
}

TEST(ProfileTimerFiberTest, TimesInProfilerOfThreadFiberMovedTo) {
  if (!SupportsFibers()) {
    GTEST_SKIP() << "Fibers are not supported";
  }
  MovingFiberState state;
  state.moving_fiber = CreateFiber({}, 0, &state, RunMovingFiber);
  ASSERT_NE(state.moving_fiber, nullptr);
  absl::Cleanup delete_moving_fiber = [&state] {
    DeleteFiber(state.moving_fiber);
  };
  auto fiber_1 = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<MovingFiberState*>(user_data);
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks});
        state.return_fiber = GetThisFiber();
        EXPECT_TRUE(SwitchToFiber(state.moving_fiber));
        state.first_visit_done.Notify();
        state.second_visit_done.WaitForNotification();
        EXPECT_EQ(profiler.GetCount("FiberTimerTest/FirstScope"), 1);
        EXPECT_EQ(profiler.GetCount("FiberTimerTest/FirstCount"), 1);
        EXPECT_EQ(profiler.GetCount("FiberTimerTest/SecondScope"), 0);
        EXPECT_EQ(profiler.GetCount("FiberTimerTest/SecondCount"), 0);
      });
  ASSERT_EQ(fiber_1.size(), 1);

  // The first thread waits for the second to finish, so if the second can't be
  // created, this releases the first before it is joined.
  absl::Cleanup end_fiber_1 = [&state, &fiber_1] {
    if (!state.second_visit_done.HasBeenNotified()) {
      state.second_visit_done.Notify();
    }
    JoinThread(fiber_1[0].thread);
    DeleteFiber(fiber_1[0].fiber);
  };
  auto fiber_2 = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &state, +[](void* user_data) {
        auto& state = *static_cast<MovingFiberState*>(user_data);
        state.first_visit_done.WaitForNotification();
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks});
        state.return_fiber = GetThisFiber();
        EXPECT_TRUE(SwitchToFiber(state.moving_fiber));
        EXPECT_EQ(profiler.GetCount("FiberTimerTest/FirstScope"), 0);
        EXPECT_EQ(profiler.GetCount("FiberTimerTest/FirstCount"), 0);
        EXPECT_EQ(profiler.GetCount("FiberTimerTest/SecondScope"), 1);
        EXPECT_EQ(profiler.GetCount("FiberTimerTest/SecondCount"), 1);
        state.second_visit_done.Notify();
      });
  ASSERT_EQ(fiber_2.size(), 1);
  JoinThread(fiber_2[0].thread);
  DeleteFiber(fiber_2[0].fiber);
}

TEST(ProfileTimerNoProfilerTest, IgnoredWithoutProfiler) {
  {
    ProfileScope<"TimerTest/NoProfiler"> scope;
  }
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks});
  EXPECT_EQ(profiler.GetCount("TimerTest/NoProfiler"), 0);
}

TEST(ProfileTimerNoProfilerTest, TimerStartedWithoutProfilerIsIgnored) {
  ProfileScope<"TimerTest/StartedWithout"> scope;
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks});
  EXPECT_EQ(profiler.GetCount("TimerTest/StartedWithout"), 0);
}

}  // namespace
}  // namespace gb
