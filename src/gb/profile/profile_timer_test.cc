// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profile_timer.h"

#include <optional>
#include <string>
#include <string_view>
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

TEST_F(ProfileTimerTest, JobTimerRecordsSelfTimeAndRunTime) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "TimerTest/Job");
  for (int i = 0; i < 2; ++i) {
    ProfileJobTimer timer(job, profiler_.ReadTicks());
    ticks_.Advance(1);
    {
      ProfileScope<"TimerTest/InJob"> scope;
      ticks_.Advance(2);
    }
  }
  EXPECT_EQ(profiler_.GetCount("TimerTest/Job"), 2);
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/Job"), absl::Nanoseconds(2));
  EXPECT_EQ(profiler_.GetSelfTime("TimerTest/InJob"), absl::Nanoseconds(4));

  // A run includes the points inside it.
  const Profiler::JobSummary summary = profiler_.GetJobSummary("TimerTest/Job");
  EXPECT_EQ(summary.runs, 2);
  EXPECT_EQ(summary.total, absl::Nanoseconds(6));
  EXPECT_EQ(summary.max, absl::Nanoseconds(3));
}

TEST_F(ProfileTimerTest, JobTimerRecordsStartDelay) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "TimerTest/StartDelay");
  for (int64_t delay_ticks : {10, 30}) {
    const int64_t queued_ticks = profiler_.ReadTicks();
    ticks_.Advance(delay_ticks);
    ProfileJobTimer timer(job, queued_ticks);
    ticks_.Advance(1);
  }
  const Profiler::JobSummary summary =
      profiler_.GetJobSummary("TimerTest/StartDelay");
  EXPECT_EQ(summary.start_delay, absl::Nanoseconds(20));
  EXPECT_EQ(summary.max_start_delay, absl::Nanoseconds(30));
  EXPECT_EQ(summary.total, absl::Nanoseconds(2));
}

TEST_F(ProfileTimerTest, StartDelayIsNeverNegative) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "TimerTest/NegativeDelay");
  {
    ProfileJobTimer timer(job, profiler_.ReadTicks() + 100);
  }
  EXPECT_EQ(profiler_.GetJobSummary("TimerTest/NegativeDelay").max_start_delay,
            absl::ZeroDuration());
}

TEST_F(ProfileTimerTest, ResumeDelayGoesToInnermostJob) {
  ProfilePoint outer(ProfilePoint::Kind::kJob, "TimerTest/OuterJob");
  ProfilePoint inner(ProfilePoint::Kind::kJob, "TimerTest/InnerJob");
  {
    ProfileJobTimer outer_timer(outer, profiler_.ReadTicks());
    ProfileJobTimer inner_timer(inner, profiler_.ReadTicks());
    ProfileScope<"TimerTest/InInnerJob"> scope;
    for (int64_t delay_ticks : {4, 8}) {
      const int64_t ready_ticks = profiler_.ReadTicks();
      ticks_.Advance(delay_ticks);
      Profiler::AddResumeDelay(ready_ticks);
    }
  }
  const Profiler::JobSummary summary =
      profiler_.GetJobSummary("TimerTest/InnerJob");
  EXPECT_EQ(summary.resumes, 2);
  EXPECT_EQ(summary.resume_delay, absl::Nanoseconds(6));
  EXPECT_EQ(summary.max_resume_delay, absl::Nanoseconds(8));
  EXPECT_EQ(profiler_.GetJobSummary("TimerTest/OuterJob").resumes, 0);
}

TEST_F(ProfileTimerTest, ResumeDelayOutsideJobIsIgnored) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "TimerTest/NoResume");
  {
    ProfileJobTimer timer(job, profiler_.ReadTicks());
  }
  {
    ProfileScope<"TimerTest/NotAJob"> scope;
    Profiler::AddResumeDelay(profiler_.ReadTicks());
  }
  Profiler::AddResumeDelay(profiler_.ReadTicks());
  EXPECT_EQ(profiler_.GetJobSummary("TimerTest/NoResume").resumes, 0);
}

TEST_F(ProfileTimerTest, JobsInReport) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "TimerTest/ReportJob");
  {
    ProfileFrame<"TimerTest/ReportFrame"> frame;
    ticks_.Advance(1);
    ProfileJobTimer timer(job, profiler_.ReadTicks());
    ticks_.Advance(4);
  }
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find("\nTimerTest/ReportJob    job "), std::string::npos)
      << report;

  // The breakdown has a row for jobs, as the frame ran one.
  EXPECT_NE(report.find("\njob    4.00ns\n"), std::string::npos) << report;
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

  // The FakeProfilerCpu every thread shares, if any, with `main`. Threads that
  // join `main` then use it, rather than their own, so their ticks agree.
  FakeProfilerCpu* shared_ticks = nullptr;

  // For a job on the fiber, when what it waits on was ready.
  int64_t ready_ticks = 0;
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

TEST_F(ProfileTimerSwitchTest, JobRunLeavesOutSuspensionAndHasResumeDelay) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    static const ProfilePoint s_job(ProfilePoint::Kind::kJob,
                                    "SwitchTest/WaitingJob");
    {
      ProfileJobTimer timer(s_job, 0);
      fiber.ticks->Advance(1);
      SwitchBack(fiber);
      Profiler::AddResumeDelay(fiber.ready_ticks);
      fiber.ticks->Advance(2);
    }
    SwitchBack(fiber);
  });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks});
        RunFiberHere(fiber, ticks);
        ticks.Advance(10);
        fiber.ready_ticks = profiler.ReadTicks();
        ticks.Advance(5);
        RunFiberHere(fiber, ticks);
        const Profiler::JobSummary summary =
            profiler.GetJobSummary("SwitchTest/WaitingJob");
        EXPECT_EQ(summary.runs, 1);
        EXPECT_EQ(summary.total, absl::Nanoseconds(3));
        EXPECT_EQ(summary.resumes, 1);
        EXPECT_EQ(summary.resume_delay, absl::Nanoseconds(5));
        EXPECT_EQ(profiler.GetWaitTime("SwitchTest/WaitingJob"),
                  absl::Nanoseconds(15));
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

TEST_F(ProfileTimerSwitchTest, InnermostScopeWaitsWhileOtherFiberRuns) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    fiber.ticks->Advance(10);
    SwitchBack(fiber);
  });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks});
        {
          ProfileScope<"SwitchTest/WaitOuter"> outer;
          ticks.Advance(1);
          {
            ProfileScope<"SwitchTest/WaitInner"> inner;
            ticks.Advance(2);
            RunFiberHere(fiber, ticks);
            ticks.Advance(4);
          }
        }
        EXPECT_EQ(profiler.GetWaitTime("SwitchTest/WaitInner"),
                  absl::Nanoseconds(10));
        EXPECT_EQ(profiler.GetSelfTime("SwitchTest/WaitInner"),
                  absl::Nanoseconds(6));
        EXPECT_EQ(profiler.GetWaitTime("SwitchTest/WaitOuter"),
                  absl::ZeroDuration());
      });
}

// The second thread's ticks are 20 ahead of where the first's were when the
// fiber suspended, so the two threads' ticks are in step.
TEST_F(ProfileTimerSwitchTest, WaitIsRecordedWhereFiberResumes) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/MovedWait"> scope;
      fiber.ticks->Advance(1);
      SwitchBack(fiber);
      fiber.ticks->Advance(2);
    }
    SwitchBack(fiber);
  });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks});
        RunFiberHere(fiber, ticks);
        EXPECT_EQ(profiler.GetWaitTime("SwitchTest/MovedWait"),
                  absl::ZeroDuration());
      });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        FakeProfilerCpu ticks;
        ticks.Advance(21);
        Profiler profiler({.fake_cpu = &ticks});
        RunFiberHere(fiber, ticks);
        EXPECT_EQ(profiler.GetWaitTime("SwitchTest/MovedWait"),
                  absl::Nanoseconds(20));
        EXPECT_EQ(profiler.GetSelfTime("SwitchTest/MovedWait"),
                  absl::Nanoseconds(3));
      });
}

// The scope waits from the first joined thread to the second, which is
// destroyed during the frame with the scope still open, so the scope has
// waited but has no calls.
TEST_F(ProfileTimerSwitchTest, JoinedWaitInMainReport) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    ProfileScope<"SwitchTest/JoinedWait"> scope;
    SwitchBack(fiber);
    fiber.ticks->Advance(1);
    SwitchBack(fiber);
  });
  FakeProfilerCpu ticks;
  Profiler main({.fake_cpu = &ticks});
  fiber_.main = &main;
  {
    ProfileFrame<"SwitchTest/WaitFrame"> frame;
    ticks.Advance(100);
    RunFiberThread(
        fiber_, +[](void* user_data) {
          auto& fiber = *static_cast<SwitchingFiber*>(user_data);
          FakeProfilerCpu ticks;
          Profiler profiler({.fake_cpu = &ticks, .main = fiber.main});
          RunFiberHere(fiber, ticks);
        });
    RunFiberThread(
        fiber_, +[](void* user_data) {
          auto& fiber = *static_cast<SwitchingFiber*>(user_data);
          FakeProfilerCpu ticks;
          ticks.Advance(30);
          Profiler profiler({.fake_cpu = &ticks, .main = fiber.main});
          RunFiberHere(fiber, ticks);
        });
  }
  EXPECT_EQ(main.GetWaitTime("SwitchTest/JoinedWait"), absl::Nanoseconds(30));
  const std::string report = main.GetReport();
  EXPECT_NE(report.find(R"(
Point                  Kind   Count  Count/frame   Self  Self/call  Self/frame  Share    Wait  Wait/call
SwitchTest/WaitFrame   frame      1         1.00  100ns      100ns       100ns   100%       0          0
SwitchTest/JoinedWait  scope      0         0.00      0          -           0  0.00%  30.0ns          -
)"),
            std::string::npos)
      << report;
  EXPECT_NE(report.find(R"(
Point                  Kind   Count   Self    Wait
SwitchTest/WaitFrame   frame      1  100ns       0
SwitchTest/JoinedWait  scope      0      0  30.0ns
)"),
            std::string::npos)
      << report;
}

// A joined thread that runs `fiber` twice, so the fiber's scope waits, and then
// stays alive until `reset` is notified.
struct LiveJoinedThread {
  SwitchingFiber* fiber = nullptr;
  absl::Notification waited;
  absl::Notification reset;
};

TEST_F(ProfileTimerSwitchTest, ResetClearsLiveJoinedWait) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/LiveWait"> scope;
      SwitchBack(fiber);
    }
    SwitchBack(fiber);
  });
  FakeProfilerCpu ticks;
  Profiler main({.fake_cpu = &ticks});
  fiber_.main = &main;
  LiveJoinedThread live = {.fiber = &fiber_};
  auto threads = CreateFiberThreads(
      std::vector<PlacedThread>(1), {}, 0, &live, +[](void* user_data) {
        auto& live = *static_cast<LiveJoinedThread*>(user_data);
        FakeProfilerCpu ticks;
        Profiler profiler({.fake_cpu = &ticks, .main = live.fiber->main});
        RunFiberHere(*live.fiber, ticks);
        ticks.Advance(30);
        RunFiberHere(*live.fiber, ticks);
        live.waited.Notify();
        live.reset.WaitForNotification();
      });
  ASSERT_EQ(threads.size(), 1);
  live.waited.WaitForNotification();
  EXPECT_EQ(main.GetWaitTime("SwitchTest/LiveWait"), absl::Nanoseconds(30));
  main.Reset();
  EXPECT_EQ(main.GetWaitTime("SwitchTest/LiveWait"), absl::ZeroDuration());
  live.reset.Notify();
  JoinThread(threads[0].thread);
  DeleteFiber(threads[0].fiber);
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

// A thread main that runs the fiber in a frame, on a Profiler joined to
// `fiber.main` if it is set.
void RunFiberInFrame(void* user_data) {
  auto& fiber = *static_cast<SwitchingFiber*>(user_data);
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks, .main = fiber.main});
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

// A thread main that runs the fiber on a Profiler named "worker", joined to
// `fiber.main`, with `fiber.shared_ticks`.
void RunFiberJoined(void* user_data) {
  auto& fiber = *static_cast<SwitchingFiber*>(user_data);
  Profiler profiler({.main = fiber.main, .name = "worker"});
  RunFiberHere(fiber, *fiber.shared_ticks);
}

// A threadless main Profiler, whose joined Profilers share its ticks, and
// which keeps every slow frame's report.
class ProfileTimerThreadlessSwitchTest : public ProfileTimerSwitchTest {
 protected:
  ProfileTimerThreadlessSwitchTest() {
    fiber_.main = &main_;
    fiber_.shared_ticks = &ticks_;
  }

  FakeProfilerCpu ticks_;
  std::vector<std::string> reports_;
  Profiler main_{{.fake_cpu = &ticks_,
                  .threadless = true,
                  .slow_frame = absl::ZeroDuration(),
                  .on_slow_frame = [this](std::string_view report) {
                    reports_.emplace_back(report);
                  }}};
};

// The frame's scope waits while another fiber runs a scope alongside the frame
// on the same thread, and the frame's own times add up to its time with the
// wait.
TEST_F(ProfileTimerThreadlessSwitchTest, FrameSpansSwitch) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/Alongside"> scope;
      fiber.ticks->Advance(20);
    }
    SwitchBack(fiber);
  });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        Profiler profiler({.main = fiber.main});
        ProfileFrame<"SwitchTest/SpanningFrame"> frame;
        fiber.shared_ticks->Advance(10);
        ProfileScope<"SwitchTest/InFrame"> scope;
        fiber.shared_ticks->Advance(5);
        RunFiberHere(fiber, *fiber.shared_ticks);
        fiber.shared_ticks->Advance(1);
      });
  EXPECT_EQ(main_.GetFrameSummary().total, absl::Nanoseconds(36));
  ASSERT_EQ(reports_.size(), 1);
  EXPECT_EQ(reports_[0], R"(Slow frame: 36.0ns, core class 0, profiler 0
Kind   In frame  Alongside
frame    10.0ns          -
scope    6.00ns     20.0ns
call          0          0
wait     20.0ns          0

Point                     Kind   Count    Self    Wait
SwitchTest/Alongside      scope      1  20.0ns       0
SwitchTest/SpanningFrame  frame      1  10.0ns       0
SwitchTest/InFrame        scope      1  6.00ns  20.0ns
)");
}

// The frame starts on one joined thread, waits, and ends on another, so its
// time includes the wait, and its thread's busy time is its run on each.
TEST_F(ProfileTimerThreadlessSwitchTest, FrameEndsOnAnotherThread) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileFrame<"SwitchTest/MovingFrame"> frame;
      fiber.ticks->Advance(100);
      SwitchBack(fiber);
      fiber.ticks->Advance(50);
    }
    SwitchBack(fiber);
  });
  RunFiberThread(fiber_, RunFiberJoined);
  ticks_.Advance(300);
  RunFiberThread(fiber_, RunFiberJoined);

  const Profiler::FrameSummary summary = main_.GetFrameSummary();
  EXPECT_EQ(summary.frames, 1);
  EXPECT_EQ(summary.total, absl::Nanoseconds(450));
  EXPECT_EQ(main_.GetSelfTime("SwitchTest/MovingFrame"),
            absl::Nanoseconds(150));
  EXPECT_EQ(main_.GetWaitTime("SwitchTest/MovingFrame"),
            absl::Nanoseconds(300));
  EXPECT_EQ(main_.GetSlowestFrameSelfTime("SwitchTest/MovingFrame"),
            absl::Nanoseconds(150));

  // The second thread carries on from the first, of the same name.
  const std::string report = main_.GetReport();
  EXPECT_NE(report.find(R"(
Thread  Busy/frame    Max  Share  class 0
worker       150ns  150ns  33.3%     100%
)"),
            std::string::npos)
      << report;
}

TEST_F(ProfileTimerThreadlessSwitchTest, FrameResumedWithoutProfilerDies) {
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileFrame<"SwitchTest/UnprofiledFrame"> frame;
      SwitchBack(fiber);
    }
    SwitchBack(fiber);
  });
  RunFiberThread(fiber_, RunFiberJoined);
  EXPECT_DEATH(RunFiberThread(fiber_, RunWithoutProfiler),
               "must resume on a thread joined to the frame's main Profiler");
}

// The profiler's cost per frame counts the frame's own points, and not those
// of another fiber that ran alongside it. The ticks advance as they are read,
// so a point has a cost.
TEST_F(ProfileTimerSwitchTest, ThreadlessFrameCostCountsOnlyItsOwnPoints) {
  FakeProfilerCpu ticks;
  ticks.SetAutoAdvance(1);
  Profiler main({.fake_cpu = &ticks, .threadless = true});
  fiber_.main = &main;
  fiber_.shared_ticks = &ticks;
  CreateSwitchingFiber(+[](void* user_data) {
    auto& fiber = *static_cast<SwitchingFiber*>(user_data);
    {
      ProfileScope<"SwitchTest/AlongsideCost"> scope;
    }
    SwitchBack(fiber);
  });
  RunFiberThread(
      fiber_, +[](void* user_data) {
        auto& fiber = *static_cast<SwitchingFiber*>(user_data);
        Profiler profiler({.main = fiber.main});
        ProfileFrame<"SwitchTest/CostFrame"> frame;
        ProfileScope<"SwitchTest/InFrameCost"> scope;
        RunFiberHere(fiber, *fiber.shared_ticks);
      });
  ASSERT_GT(main.GetPointCost(), absl::ZeroDuration());
  EXPECT_EQ(main.GetFrameSummary().profiler_cost, 2 * main.GetPointCost());
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

TEST(ProfileTimerNoProfilerTest, JobIgnoredWithoutProfiler) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "TimerTest/NoProfilerJob");
  {
    ProfileJobTimer timer(job, 0);
    Profiler::AddResumeDelay(0);
  }
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks});
  EXPECT_EQ(profiler.GetJobSummary("TimerTest/NoProfilerJob").runs, 0);
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
