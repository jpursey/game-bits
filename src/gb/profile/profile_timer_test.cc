// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profile_timer.h"

#include <optional>
#include <string>
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
  EXPECT_DEATH(first.reset(), "must end on the fiber it started on");
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

//==============================================================================
// Timers across fiber switches
//==============================================================================

// A fiber that keeps timers open across switches. Each thread that runs it
// sets `ticks` to its own FakeProfilerCpu, which the fiber advances, and
// `return_fiber` to its own fiber, which the fiber switches back to.
struct SwitchingFiber {
  Fiber fiber = nullptr;
  Fiber return_fiber = nullptr;
  FakeProfilerCpu* ticks = nullptr;

  // The main Profiler that threads running the fiber join, if any.
  Profiler* main = nullptr;
};

// Runs `fiber` on the calling thread, with `ticks`, until it switches back.
void RunFiberHere(SwitchingFiber& fiber, FakeProfilerCpu& ticks) {
  fiber.ticks = &ticks;
  fiber.return_fiber = GetThisFiber();
  EXPECT_TRUE(SwitchToFiber(fiber.fiber));
}

// Switches from `fiber` back to the thread running it.
void SwitchBack(SwitchingFiber& fiber) {
  EXPECT_TRUE(SwitchToFiber(fiber.return_fiber));
}

// Runs `thread_main`, passed `fiber`, on a new thread's fiber, and waits for it
// to finish.
void RunFiberThread(SwitchingFiber& fiber, FiberMain thread_main) {
  auto threads = CreateFiberThreads(std::vector<PlacedThread>(1), {}, 0, &fiber,
                                    thread_main);
  ASSERT_EQ(threads.size(), 1);
  JoinThread(threads[0].thread);
  DeleteFiber(threads[0].fiber);
}

// A thread main that runs the fiber on a thread with no Profiler.
void RunWithoutProfiler(void* user_data) {
  auto& fiber = *static_cast<SwitchingFiber*>(user_data);
  FakeProfilerCpu ticks;
  RunFiberHere(fiber, ticks);
}

class ProfileTimerSwitchTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!SupportsFibers()) {
      GTEST_SKIP() << "Fibers are not supported";
    }
  }

  void TearDown() override {
    if (fiber_.fiber != nullptr) {
      DeleteFiber(fiber_.fiber);
    }
  }

  // Creates the fiber, which must end by switching back, as it is deleted
  // while suspended.
  void CreateSwitchingFiber(FiberMain fiber_main) {
    fiber_.fiber = CreateFiber({}, 0, &fiber_, fiber_main);
    ASSERT_NE(fiber_.fiber, nullptr);
  }

  SwitchingFiber fiber_;
};

TEST_F(ProfileTimerSwitchTest, ScopeLeavesOutOtherFiberOnSameThread) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/Other"> other;
      fiber.ticks->Advance(2);
    }
    fiber.ticks->Advance(8);
    SwitchBack(fiber);
  });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks});
        {
          ProfileScope<"SwitchTest/Spanning"> spanning;
          ticks.Advance(1);
          RunFiberHere(fiber, ticks);
          ticks.Advance(4);
        }
        EXPECT_EQ(profiler.GetCount("SwitchTest/Spanning"), 1);
        EXPECT_EQ(profiler.GetSelfTime("SwitchTest/Spanning"),
                  absl::Nanoseconds(5));
        EXPECT_EQ(profiler.GetCount("SwitchTest/Other"), 1);
        EXPECT_EQ(profiler.GetSelfTime("SwitchTest/Other"),
                  absl::Nanoseconds(2));
      });
}

// The second thread's ticks are far ahead of the first's, so a timer that kept
// its first thread's start would record a long time.
TEST_F(ProfileTimerSwitchTest, ScopesRecordRunTimeInProfilerTheyEndIn) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/MovedOuter"> outer;
      fiber.ticks->Advance(1);
      {
        ProfileScope<"SwitchTest/MovedInner"> inner;
        fiber.ticks->Advance(2);
        SwitchBack(fiber);
        fiber.ticks->Advance(4);
      }
      fiber.ticks->Advance(8);
    }
    SwitchBack(fiber);
  });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks});
        RunFiberHere(fiber, ticks);
        EXPECT_EQ(profiler.GetCount("SwitchTest/MovedOuter"), 0);
        EXPECT_EQ(profiler.GetCount("SwitchTest/MovedInner"), 0);
      });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        FakeProfilerCpu ticks;
        ticks.Advance(1000);
        Profiler profiler({.fake_cpu = &ticks});
        RunFiberHere(fiber, ticks);
        EXPECT_EQ(profiler.GetCount("SwitchTest/MovedOuter"), 1);
        EXPECT_EQ(profiler.GetSelfTime("SwitchTest/MovedOuter"),
                  absl::Nanoseconds(9));
        EXPECT_EQ(profiler.GetCount("SwitchTest/MovedInner"), 1);
        EXPECT_EQ(profiler.GetSelfTime("SwitchTest/MovedInner"),
                  absl::Nanoseconds(6));
      });
}

// The scope runs on two joined threads during one frame, the first on a class 1
// core, and each thread's busy time is its own part.
TEST_F(ProfileTimerSwitchTest, BusyTimeGoesToEachThreadScopeRanOn) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/Busy"> scope;
      fiber.ticks->Advance(100);
      SwitchBack(fiber);
      fiber.ticks->Advance(300);
    }
    SwitchBack(fiber);
  });
  FakeProfilerCpu ticks;
  Profiler main({.fake_cpu = &ticks});
  fiber_.main = &main;
  {
    ProfileFrame<"SwitchTest/BusyFrame"> frame;
    ticks.Advance(1000);
    RunFiberThread(
        fiber_, +[](void* user_data) {
          auto& fiber = *static_cast<SwitchingFiber*>(user_data);
          FakeProfilerCpu ticks;
          ticks.SetCoreClass(1);
          Profiler profiler(
              {.fake_cpu = &ticks, .main = fiber.main, .name = "first"});
          RunFiberHere(fiber, ticks);
        });
    RunFiberThread(
        fiber_, +[](void* user_data) {
          auto& fiber = *static_cast<SwitchingFiber*>(user_data);
          FakeProfilerCpu ticks;
          Profiler profiler(
              {.fake_cpu = &ticks, .main = fiber.main, .name = "second"});
          RunFiberHere(fiber, ticks);
        });
  }
  EXPECT_EQ(main.GetSelfTime("SwitchTest/Busy"), absl::Nanoseconds(400));
  const std::string report = main.GetReport();
  EXPECT_NE(report.find(R"(
Thread  Busy/frame     Max  Share  class 0  class 1
first        100ns   100ns  10.0%        -     100%
main        1.00us  1.00us   100%     100%        -
second       300ns   300ns  30.0%     100%        -
)"),
            std::string::npos)
      << report;
}

// The fiber starts a scope on a thread with a Profiler, resumes on one with
// none, and ends the scope after resuming on a third thread with a Profiler.
TEST_F(ProfileTimerSwitchTest, ScopeResumedWithoutProfilerRecordsNothing) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/Unprofiled"> scope;
      fiber.ticks->Advance(1);
      SwitchBack(fiber);
      fiber.ticks->Advance(2);
      SwitchBack(fiber);
      fiber.ticks->Advance(4);
    }
    SwitchBack(fiber);
  });
  constexpr FiberMain kRunWithProfiler = +[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    FakeProfilerCpu ticks;
    Profiler profiler({.fake_cpu = &ticks});
    RunFiberHere(fiber, ticks);
    EXPECT_EQ(profiler.GetCount("SwitchTest/Unprofiled"), 0);
  };
  RunFiberThread(fiber_, kRunWithProfiler);
  RunFiberThread(fiber_, RunWithoutProfiler);
  RunFiberThread(fiber_, kRunWithProfiler);
}

TEST_F(ProfileTimerSwitchTest, ScopeStartedWithoutProfilerStaysIgnored) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/StartedWithout"> scope;
      SwitchBack(fiber);
      fiber.ticks->Advance(1);
    }
    SwitchBack(fiber);
  });
  RunFiberThread(fiber_, RunWithoutProfiler);
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks});
        RunFiberHere(fiber, ticks);
        EXPECT_EQ(profiler.GetCount("SwitchTest/StartedWithout"), 0);
      });
}

void RunFiberInFrame(void* user_data) {
  auto& fiber = *static_cast<SwitchingFiber*>(user_data);
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks});
  ProfileFrame<"SwitchTest/Frame"> frame;
  RunFiberHere(fiber, ticks);
}

TEST_F(ProfileTimerSwitchTest, SwitchingInFrameDies) {
  CreateSwitchingFiber(+[](void* user_data) {
    SwitchBack(*static_cast<SwitchingFiber*>(user_data));
  });
  EXPECT_DEATH(RunFiberThread(fiber_, RunFiberInFrame),
               "can't span a fiber switch");
}

// A timer the thread's own fiber starts, which the switching fiber ends.
std::optional<ProfileScope<"SwitchTest/OtherFiber">> g_other_fiber_scope;

void RunFiberWithOpenScope(void* user_data) {
  auto& fiber = *static_cast<SwitchingFiber*>(user_data);
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks});
  g_other_fiber_scope.emplace();
  RunFiberHere(fiber, ticks);
}

TEST_F(ProfileTimerSwitchTest, EndingTimerOnAnotherFiberDies) {
  CreateSwitchingFiber(+[](void* user_data) {
    g_other_fiber_scope.reset();
    SwitchBack(*static_cast<SwitchingFiber*>(user_data));
  });
  EXPECT_DEATH(RunFiberThread(fiber_, RunFiberWithOpenScope),
               "must end on the fiber it started on");
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
