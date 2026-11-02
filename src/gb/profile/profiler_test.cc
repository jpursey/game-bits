// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profiler.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "gb/profile/fake_profiler_cpu.h"
#include "gb/profile/profile_point.h"
#include "gb/profile/profile_timer.h"
#include "gb/thread/thread.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Points are registered for the whole program, so each test uses names of its
// own. Every tick is a nanosecond.

class ProfilerTest : public ::testing::Test {
 protected:
  // Records a frame of each of `frame_ticks`, with nothing inside it.
  void RecordFrames(const std::vector<int64_t>& frame_ticks) {
    for (int64_t ticks : frame_ticks) {
      ProfileFrame<"ProfilerTest/Frames"> frame;
      ticks_.Advance(ticks);
    }
  }

  FakeProfilerCpu ticks_;
  Profiler profiler_{{.fake_cpu = &ticks_}};
};

TEST_F(ProfilerTest, OnePerThread) {
  EXPECT_DEATH(Profiler({.fake_cpu = &ticks_}),
               "Only one Profiler may exist per thread");
}

TEST_F(ProfilerTest, EachThreadRecordsItsOwn) {
  ProfileCount<"ProfilerTest/EachThread">(1);

  int64_t other_count = 0;
  std::thread thread([&other_count] {
    FakeProfilerCpu other_ticks;
    Profiler other_profiler({.fake_cpu = &other_ticks});
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

//------------------------------------------------------------------------------
// Joined Profilers
//------------------------------------------------------------------------------

// A thread with a Profiler joined to a main Profiler, which runs tasks
// for the test. Each thread has its own FakeProfilerCpu, with the default tick
// rate.
class ProfiledThread {
 public:
  using Task =
      absl::AnyInvocable<void(Profiler& profiler, FakeProfilerCpu& ticks)>;

  // Starts the thread, and waits until its Profiler, named `name`, has joined
  // `main`, which must outlive this.
  explicit ProfiledThread(Profiler* main, std::string_view name = {})
      : thread_(&ProfiledThread::RunThread, this, main, std::string(name)) {
    Run([](Profiler& profiler, FakeProfilerCpu& ticks) {});
  }
  ProfiledThread(const ProfiledThread&) = delete;
  ProfiledThread& operator=(const ProfiledThread&) = delete;

  // Destroys the thread's Profiler, and waits for the thread to end.
  ~ProfiledThread() {
    {
      absl::MutexLock lock(&mutex_);
      stop_ = true;
    }
    thread_.join();
  }

  // Runs `task` on the thread, and waits for it to finish.
  void Run(Task task) {
    absl::MutexLock lock(&mutex_);
    task_ = std::move(task);
    mutex_.Await(absl::Condition(this, &ProfiledThread::IsIdle));
  }

 private:
  bool IsIdle() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return task_ == nullptr;
  }
  bool HasWork() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return task_ != nullptr || stop_;
  }

  void RunThread(Profiler* main, std::string name) {
    FakeProfilerCpu ticks;
    Profiler profiler({.fake_cpu = &ticks, .main = main, .name = name});
    absl::MutexLock lock(&mutex_);
    while (true) {
      mutex_.Await(absl::Condition(this, &ProfiledThread::HasWork));
      if (task_ == nullptr) {
        break;
      }
      task_(profiler, ticks);
      task_ = nullptr;
    }
  }

  absl::Mutex mutex_;
  Task task_ ABSL_GUARDED_BY(mutex_);
  bool stop_ ABSL_GUARDED_BY(mutex_) = false;

  // Last, so the thread starts after the rest is initialized.
  std::thread thread_;
};

TEST_F(ProfilerTest, ThreadPointsAddToMain) {
  {
    ProfileScope<"ThreadTest/Scope"> scope;
    ticks_.Advance(5);
  }
  ProfileCount<"ThreadTest/Count">(1);

  ProfiledThread thread(&profiler_);
  thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    for (int i = 0; i < 2; ++i) {
      ProfileScope<"ThreadTest/Scope"> scope;
      ticks.Advance(10);
    }
    ProfileCount<"ThreadTest/Count">(2);

    // A joined Profiler reads only its own thread.
    EXPECT_EQ(profiler.GetCount("ThreadTest/Scope"), 2);
    EXPECT_EQ(profiler.GetSelfTime("ThreadTest/Scope"), absl::Nanoseconds(20));
    EXPECT_EQ(profiler.GetCount("ThreadTest/Count"), 2);
  });

  EXPECT_EQ(profiler_.GetCount("ThreadTest/Scope"), 3);
  EXPECT_EQ(profiler_.GetSelfTime("ThreadTest/Scope"), absl::Nanoseconds(25));
  EXPECT_EQ(profiler_.GetCount("ThreadTest/Count"), 3);
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find("ThreadTest/Scope  scope        3"), std::string::npos)
      << report;
}

TEST_F(ProfilerTest, JoinedProfilerWithoutFakeUsesMains) {
  std::thread thread([this] {
    Profiler profiler({.main = &profiler_});
    ProfileScope<"ThreadTest/MainsFake"> scope;
    ticks_.Advance(7);
  });
  thread.join();
  EXPECT_EQ(profiler_.GetSelfTime("ThreadTest/MainsFake"),
            absl::Nanoseconds(7));
}

TEST_F(ProfilerTest, PointsOfEveryThreadAdd) {
  ProfiledThread thread_a(&profiler_);
  ProfiledThread thread_b(&profiler_);
  thread_a.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/EveryThread">(1);
  });
  thread_b.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/EveryThread">(2);
  });
  EXPECT_EQ(profiler_.GetCount("ThreadTest/EveryThread"), 3);
}

TEST_F(ProfilerTest, DestroyedThreadStaysInTotals) {
  std::optional<ProfiledThread> thread(std::in_place, &profiler_);
  thread->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileScope<"ThreadTest/Destroyed"> scope;
    ticks.Advance(10);
  });
  thread.reset();
  EXPECT_EQ(profiler_.GetCount("ThreadTest/Destroyed"), 1);
  EXPECT_EQ(profiler_.GetSelfTime("ThreadTest/Destroyed"),
            absl::Nanoseconds(10));
}

TEST_F(ProfilerTest, ValueIsLatestSetOnAnyThread) {
  // Each thread's ticks are its own, so the test sets when each value is set.
  ticks_.Advance(10);
  ProfileSetValue<"ThreadTest/Value">(1);
  ProfiledThread thread(&profiler_);
  thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ticks.Advance(20);
    ProfileSetValue<"ThreadTest/Value">(2);
  });
  EXPECT_EQ(profiler_.GetValue("ThreadTest/Value"), 2);

  ticks_.Advance(20);
  ProfileSetValue<"ThreadTest/Value">(3);
  EXPECT_EQ(profiler_.GetValue("ThreadTest/Value"), 3);

  std::optional<ProfiledThread> destroyed(std::in_place, &profiler_);
  destroyed->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ticks.Advance(40);
    ProfileSetValue<"ThreadTest/Value">(4);
  });
  destroyed.reset();
  EXPECT_EQ(profiler_.GetValue("ThreadTest/Value"), 4);
}

TEST_F(ProfilerTest, ResetClearsEveryThread) {
  ProfileCount<"ThreadTest/Reset">(1);
  ProfiledThread thread(&profiler_);
  thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/Reset">(2);
    ProfileSetValue<"ThreadTest/ResetValue">(5);
  });
  std::optional<ProfiledThread> destroyed(std::in_place, &profiler_);
  destroyed->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/Reset">(4);
  });
  destroyed.reset();
  ASSERT_EQ(profiler_.GetCount("ThreadTest/Reset"), 7);

  profiler_.Reset();
  EXPECT_EQ(profiler_.GetCount("ThreadTest/Reset"), 0);
  EXPECT_EQ(profiler_.GetValue("ThreadTest/ResetValue"), 0);

  // What a thread records after the reset counts.
  thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/Reset">(8);
  });
  EXPECT_EQ(profiler_.GetCount("ThreadTest/Reset"), 8);
}

TEST_F(ProfilerTest, ThreadCantTimeFrames) {
  EXPECT_DEATH(
      {
        ProfiledThread thread(&profiler_);
        thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
          ProfileFrame<"ThreadTest/Frame"> frame;
        });
      },
      "A joined Profiler can't time frames");
}

TEST_F(ProfilerTest, ThreadCantReset) {
  EXPECT_DEATH(
      {
        ProfiledThread thread(&profiler_);
        thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
          profiler.Reset();
        });
      },
      "A joined Profiler can't be reset");
}

TEST_F(ProfilerTest, ThreadNeedsMainTickRate) {
  EXPECT_DEATH(
      {
        std::thread thread([this] {
          FakeProfilerCpu ticks(2e9);
          Profiler profiler({.fake_cpu = &ticks, .main = &profiler_});
        });
        thread.join();
      },
      "must have its main Profiler's tick rate");
}

TEST_F(ProfilerTest, MainCantBeJoined) {
  EXPECT_DEATH(
      {
        ProfiledThread thread(&profiler_);
        thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
          ProfiledThread inner(&profiler);
        });
      },
      "main Profiler can't itself be joined");
}

TEST_F(ProfilerTest, ThreadWithNameOfDestroyedOneCarriesOn) {
  std::optional<ProfiledThread> first(std::in_place, &profiler_, "loader");
  first->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/SameName">(1);
  });
  first.reset();

  // The second takes over the first's totals, and a third that joins while
  // the second exists is kept apart from them.
  std::optional<ProfiledThread> second(std::in_place, &profiler_, "loader");
  second->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/SameName">(2);
  });
  ProfiledThread third(&profiler_, "loader");
  third.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/SameName">(4);
  });
  EXPECT_EQ(profiler_.GetCount("ThreadTest/SameName"), 7);
  second.reset();
  EXPECT_EQ(profiler_.GetCount("ThreadTest/SameName"), 7);

  profiler_.Reset();
  std::optional<ProfiledThread> fourth(std::in_place, &profiler_, "loader");
  fourth->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadTest/SameName">(8);
  });
  EXPECT_EQ(profiler_.GetCount("ThreadTest/SameName"), 8);
}

// Joined Profilers are numbered with the lowest number no other existing one
// has, and only those that still exist when the main Profiler is destroyed are
// an error.
TEST(ProfilerThreadSetupTest, MainMustOutliveThreads) {
  EXPECT_DEATH(
      {
        FakeProfilerCpu ticks;
        std::optional<Profiler> main(std::in_place,
                                     Profiler::Options{.fake_cpu = &ticks});
        std::optional<ProfiledThread> first(std::in_place, &*main);
        std::optional<ProfiledThread> second(std::in_place, &*main);
        second.reset();
        ProfiledThread third(&*main);
        first.reset();
        main.reset();
      },
      "must outlive its joined Profilers, but \"thread 2\" still exists");
}

TEST(ProfilerThreadSetupTest, MainMustOutliveNamedThread) {
  EXPECT_DEATH(
      {
        FakeProfilerCpu ticks;
        std::optional<Profiler> main(std::in_place,
                                     Profiler::Options{.fake_cpu = &ticks});
        ProfiledThread thread(&*main, "loader");
        main.reset();
      },
      "\"loader\" still exists");
}

// A joined Profiler on a named thread that CreateThread() made.
struct NamedThreadJoiner {
  Profiler* main;
  absl::Notification joined;
  absl::Notification done;
};

TEST(ProfilerThreadSetupTest, NameDefaultsToThreadName) {
  EXPECT_DEATH(
      {
        FakeProfilerCpu ticks;
        std::optional<Profiler> main(std::in_place,
                                     Profiler::Options{.fake_cpu = &ticks});
        NamedThreadJoiner joiner = {.main = &*main};
        Thread thread = CreateThread(0, 0, &joiner, [](void* user_data) {
          NamedThreadJoiner* joiner =
              static_cast<NamedThreadJoiner*>(user_data);
          SetThreadName(GetThisThread(), "loader thread");
          FakeProfilerCpu ticks;
          Profiler profiler({.fake_cpu = &ticks, .main = joiner->main});
          joiner->joined.Notify();
          joiner->done.WaitForNotification();
        });
        joiner.joined.WaitForNotification();
        main.reset();
        joiner.done.Notify();
        JoinThread(thread);
      },
      "\"loader thread\" still exists");
}

//------------------------------------------------------------------------------
// Frames
//------------------------------------------------------------------------------

// Percentiles are within 1/16th of the true value.
void ExpectPercentile(absl::Duration actual, absl::Duration expected) {
  EXPECT_LE(absl::AbsDuration(actual - expected), expected / 16)
      << "actual " << actual << ", expected " << expected;
}

TEST_F(ProfilerTest, FrameRecordsCountAndSelfTime) {
  for (int i = 0; i < 2; ++i) {
    ProfileFrame<"ProfilerTest/Frame"> frame;
    ticks_.Advance(10);
    ProfileScope<"ProfilerTest/InFrame"> scope;
    ticks_.Advance(5);
  }
  EXPECT_EQ(profiler_.GetCount("ProfilerTest/Frame"), 2);
  EXPECT_EQ(profiler_.GetSelfTime("ProfilerTest/Frame"), absl::Nanoseconds(20));
  EXPECT_EQ(profiler_.GetSelfTime("ProfilerTest/InFrame"),
            absl::Nanoseconds(10));
}

TEST_F(ProfilerTest, FrameInsideScope) {
  {
    ProfileScope<"ProfilerTest/AroundFrame"> scope;
    ticks_.Advance(1);
    ProfileFrame<"ProfilerTest/InsideFrame"> frame;
    ticks_.Advance(10);
  }
  EXPECT_EQ(profiler_.GetSelfTime("ProfilerTest/AroundFrame"),
            absl::Nanoseconds(1));
  EXPECT_EQ(profiler_.GetFrameSummary().total, absl::Nanoseconds(10));
}

TEST_F(ProfilerTest, NestedFramesDie) {
  ProfileFrame<"ProfilerTest/Outer"> frame;
  EXPECT_DEATH({ ProfileFrame<"ProfilerTest/Inner"> inner; }, "can't nest");
}

TEST_F(ProfilerTest, FrameSummaryWithNoFrames) {
  const Profiler::FrameSummary summary = profiler_.GetFrameSummary();
  EXPECT_EQ(summary.frames, 0);
  EXPECT_EQ(summary.total, absl::ZeroDuration());
  EXPECT_EQ(summary.average, absl::ZeroDuration());
  EXPECT_EQ(summary.p50, absl::ZeroDuration());
  EXPECT_EQ(summary.max, absl::ZeroDuration());
}

TEST_F(ProfilerTest, FrameSummary) {
  RecordFrames({10, 20, 30});
  const Profiler::FrameSummary summary = profiler_.GetFrameSummary();
  EXPECT_EQ(summary.frames, 3);
  EXPECT_EQ(summary.total, absl::Nanoseconds(60));
  EXPECT_EQ(summary.average, absl::Nanoseconds(20));
  EXPECT_EQ(summary.max, absl::Nanoseconds(30));
  ExpectPercentile(summary.p50, absl::Nanoseconds(20));
}

TEST_F(ProfilerTest, FramePercentiles) {
  // Frames of 1us to 1000us, in a shuffled order.
  std::vector<int64_t> frame_ticks;
  for (int i = 0; i < 1000; ++i) {
    frame_ticks.push_back(((i * 7919) % 1000 + 1) * 1000);
  }
  RecordFrames(frame_ticks);
  const Profiler::FrameSummary summary = profiler_.GetFrameSummary();
  ExpectPercentile(summary.p50, absl::Microseconds(500));
  ExpectPercentile(summary.p90, absl::Microseconds(900));
  ExpectPercentile(summary.p99, absl::Microseconds(990));
  EXPECT_EQ(summary.max, absl::Microseconds(1000));
}

TEST_F(ProfilerTest, SmallFramePercentilesAreExact) {
  RecordFrames({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
  EXPECT_EQ(profiler_.GetFrameSummary().p50, absl::Nanoseconds(8));
}

TEST_F(ProfilerTest, LongFramePercentiles) {
  constexpr int64_t kTicks = (int64_t{1} << 40) + (int64_t{1} << 40) / 3;
  RecordFrames({kTicks});
  const Profiler::FrameSummary summary = profiler_.GetFrameSummary();
  ExpectPercentile(summary.p50, absl::Nanoseconds(kTicks));
  EXPECT_EQ(summary.max, absl::Nanoseconds(kTicks));
}

TEST_F(ProfilerTest, SlowestFrameBreakdown) {
  {
    ProfileFrame<"ProfilerTest/Slowest"> frame;
    ticks_.Advance(10);
    ProfileCount<"ProfilerTest/SlowestCount">(2);
    ProfileScope<"ProfilerTest/SlowestA"> scope;
    ticks_.Advance(20);
  }
  {
    // Faster, so it doesn't replace the slowest frame.
    ProfileFrame<"ProfilerTest/Slowest"> frame;
    ProfileScope<"ProfilerTest/SlowestB"> scope;
    ticks_.Advance(20);
  }
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/Slowest"), 1);
  EXPECT_EQ(profiler_.GetSlowestFrameSelfTime("ProfilerTest/Slowest"),
            absl::Nanoseconds(10));
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/SlowestA"), 1);
  EXPECT_EQ(profiler_.GetSlowestFrameSelfTime("ProfilerTest/SlowestA"),
            absl::Nanoseconds(20));
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/SlowestCount"), 2);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/SlowestB"), 0);

  {
    // Slower, so it replaces the slowest frame.
    ProfileFrame<"ProfilerTest/Slowest"> frame;
    ProfileScope<"ProfilerTest/SlowestB"> scope;
    ticks_.Advance(40);
  }
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/SlowestA"), 0);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/SlowestCount"), 0);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/SlowestB"), 1);
  EXPECT_EQ(profiler_.GetSlowestFrameSelfTime("ProfilerTest/SlowestB"),
            absl::Nanoseconds(40));
  EXPECT_EQ(profiler_.GetCount("ProfilerTest/SlowestB"), 2);
}

TEST_F(ProfilerTest, PointsOutsideFramesAreOnlyInTotals) {
  {
    ProfileScope<"ProfilerTest/Outside"> scope;
    ticks_.Advance(100);
  }
  {
    ProfileFrame<"ProfilerTest/OutsideFrame"> frame;
    ticks_.Advance(10);
  }
  {
    ProfileScope<"ProfilerTest/Outside"> scope;
    ProfileCount<"ProfilerTest/OutsideCount">(1);
    ticks_.Advance(100);
  }
  {
    // The slowest so far, which copies the breakdown of this frame only.
    ProfileFrame<"ProfilerTest/OutsideFrame"> frame;
    ticks_.Advance(20);
  }
  EXPECT_EQ(profiler_.GetCount("ProfilerTest/Outside"), 2);
  EXPECT_EQ(profiler_.GetCount("ProfilerTest/OutsideCount"), 1);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/Outside"), 0);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/OutsideCount"), 0);
  EXPECT_EQ(profiler_.GetFrameSummary().total, absl::Nanoseconds(30));
}

TEST_F(ProfilerTest, ResetClearsFrames) {
  {
    ProfileFrame<"ProfilerTest/ResetFrame"> frame;
    ProfileScope<"ProfilerTest/ResetInFrame"> scope;
    ticks_.Advance(10);
  }
  profiler_.Reset();
  EXPECT_EQ(profiler_.GetFrameSummary().frames, 0);
  EXPECT_EQ(profiler_.GetFrameSummary().max, absl::ZeroDuration());
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/ResetInFrame"), 0);

  {
    ProfileFrame<"ProfilerTest/ResetFrame"> frame;
    ticks_.Advance(5);
  }
  EXPECT_EQ(profiler_.GetFrameSummary().frames, 1);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ProfilerTest/ResetFrame"), 1);
}

TEST_F(ProfilerTest, ThreadPointsInFrameBreakdown) {
  ProfiledThread thread(&profiler_);
  {
    ProfileFrame<"ThreadFrameTest/Frame"> frame;
    ticks_.Advance(10);
    thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileScope<"ThreadFrameTest/Scope"> scope;
      ticks.Advance(30);
      ProfileCount<"ThreadFrameTest/Count">(2);
    });
  }
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/Scope"), 1);
  EXPECT_EQ(profiler_.GetSlowestFrameSelfTime("ThreadFrameTest/Scope"),
            absl::Nanoseconds(30));
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/Count"), 2);

  // Points that end between frames are in no frame's breakdown.
  thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileScope<"ThreadFrameTest/Scope"> scope;
    ticks.Advance(5);
    ProfileCount<"ThreadFrameTest/Count">(1);
  });
  {
    // Slower, so it replaces the slowest frame.
    ProfileFrame<"ThreadFrameTest/Frame"> frame;
    ticks_.Advance(20);
  }
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/Frame"), 1);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/Scope"), 0);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/Count"), 0);
  EXPECT_EQ(profiler_.GetCount("ThreadFrameTest/Scope"), 2);
}

TEST_F(ProfilerTest, PointsOfEveryThreadAddInFrame) {
  ProfiledThread thread_a(&profiler_);
  ProfiledThread thread_b(&profiler_);
  {
    ProfileFrame<"ThreadFrameTest/EveryFrame"> frame;
    ProfileScope<"ThreadFrameTest/Every"> scope;
    ticks_.Advance(10);
    thread_a.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileScope<"ThreadFrameTest/Every"> scope;
      ticks.Advance(20);
    });
    thread_b.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileScope<"ThreadFrameTest/Every"> scope;
      ticks.Advance(40);
    });
  }
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/Every"), 3);
  EXPECT_EQ(profiler_.GetSlowestFrameSelfTime("ThreadFrameTest/Every"),
            absl::Nanoseconds(70));
}

TEST_F(ProfilerTest, DestroyedThreadStaysInFrame) {
  // Destroyed before the frame, so it has no part in it.
  std::optional<ProfiledThread> before(std::in_place, &profiler_);
  before->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileCount<"ThreadFrameTest/Destroyed">(1);
  });
  {
    ProfileFrame<"ThreadFrameTest/DestroyedFrame"> frame;
    ticks_.Advance(10);
    before->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileCount<"ThreadFrameTest/Destroyed">(2);
    });
    before.reset();

    // Two that are destroyed during the frame both count.
    for (int64_t count : {4, 8}) {
      std::optional<ProfiledThread> during(std::in_place, &profiler_);
      during->Run([count](Profiler& profiler, FakeProfilerCpu& ticks) {
        ProfileCount<"ThreadFrameTest/Destroyed">(count);
        ProfileScope<"ThreadFrameTest/DestroyedScope"> scope;
        ticks.Advance(count);
      });
    }
  }
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/Destroyed"), 14);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/DestroyedScope"),
            2);
  EXPECT_EQ(profiler_.GetSlowestFrameSelfTime("ThreadFrameTest/DestroyedScope"),
            absl::Nanoseconds(12));

  // The destroyed threads' parts are only in the frame they were destroyed in.
  {
    ProfileFrame<"ThreadFrameTest/DestroyedFrame"> frame;
    ticks_.Advance(20);
  }
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/Destroyed"), 0);
  EXPECT_EQ(profiler_.GetCount("ThreadFrameTest/Destroyed"), 15);
}

TEST_F(ProfilerTest, ThreadPointsBeforeResetAreInNoFrame) {
  ProfiledThread thread(&profiler_);
  {
    ProfileFrame<"ThreadFrameTest/ResetFrame"> frame;
    thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileCount<"ThreadFrameTest/BeforeReset">(1);
    });
  }
  profiler_.Reset();

  // The first frame after the reset isn't taken for the one before it, which
  // the thread's slots still hold a part in.
  {
    ProfileFrame<"ThreadFrameTest/ResetFrame"> frame;
    ticks_.Advance(10);
  }
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/ResetFrame"), 1);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("ThreadFrameTest/BeforeReset"), 0);
}

TEST_F(ProfilerTest, ThreadsTable) {
  ProfiledThread loader(&profiler_, "loader");
  ProfiledThread idle(&profiler_);
  {
    ProfileFrame<"ThreadBusyTest/Frame"> frame;
    ticks_.Advance(1000);

    // Only the outermost point counts, on the class of core it starts on.
    loader.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ticks.SetCoreClass(1);
      ProfileScope<"ThreadBusyTest/Outer"> outer;
      ticks.Advance(100);
      {
        ProfileScope<"ThreadBusyTest/Inner"> inner;
        ticks.Advance(100);
      }
      ticks.Advance(100);
      ticks.SetCoreClass(0);
    });
  }

  // Busy time between frames isn't counted.
  loader.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileScope<"ThreadBusyTest/Outer"> outer;
    ticks.Advance(5000);
  });
  {
    ProfileFrame<"ThreadBusyTest/Frame"> frame;
    ticks_.Advance(1000);
    loader.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileScope<"ThreadBusyTest/Outer"> outer;
      ticks.Advance(100);
    });
  }

  // Sorted by name, with the main thread's busy time its frames' time.
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Thread    Busy/frame     Max  Share  class 0  class 1
loader         200ns   300ns  20.0%    25.0%    75.0%
main          1.00us  1.00us   100%     100%        -
thread 1           0       0  0.00%        -        -
)"),
            std::string::npos)
      << report;
}

// A point still running as frames end counts in each frame, up to its end,
// and not before or between frames.
TEST_F(ProfilerTest, LongPointIsBusyInEachFrame) {
  absl::Notification started;
  absl::Notification end;
  std::thread thread([this, &started, &end] {
    Profiler profiler({.main = &profiler_, .name = "loader"});
    ProfileScope<"ThreadBusyTest/Long"> scope;
    started.Notify();
    end.WaitForNotification();
  });
  started.WaitForNotification();
  ticks_.Advance(500);
  RecordFrames({1000});
  ticks_.Advance(200);
  RecordFrames({400});
  end.Notify();
  thread.join();
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Thread  Busy/frame     Max  Share  class 0
loader       700ns  1.00us   100%     100%
main         700ns  1.00us   100%     100%
)"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, NoThreadsTableUntilJoined) {
  RecordFrames({1000});
  EXPECT_EQ(profiler_.GetReport().find("Thread"), std::string::npos);
}

TEST_F(ProfilerTest, MainThreadName) {
  ticks_.SetCoreClass(1);
  RecordFrames({1000});
  ticks_.SetCoreClass(0);
  {
    // A frame that changes class of core is mixed.
    ProfileFrame<"ProfilerTest/Frames"> frame;
    ticks_.Advance(3000);
    ticks_.SetCoreClass(1);
  }
  std::thread thread([this] {
    FakeProfilerCpu ticks;
    Profiler profiler({.fake_cpu = &ticks, .main = &profiler_});
  });
  thread.join();
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Thread    Busy/frame     Max  Share  class 1  mixed
main          2.00us  3.00us   100%    25.0%  75.0%
thread 1           0       0  0.00%        -      -
)"),
            std::string::npos)
      << report;
}

TEST(ProfilerThreadSetupTest, MainThreadHasItsName) {
  FakeProfilerCpu ticks;
  Profiler main({.fake_cpu = &ticks, .name = "render"});
  std::thread thread([&main] {
    FakeProfilerCpu thread_ticks;
    Profiler profiler({.fake_cpu = &thread_ticks, .main = &main});
  });
  thread.join();
  const std::string report = main.GetReport();
  EXPECT_NE(report.find("\nrender  "), std::string::npos) << report;
}

TEST_F(ProfilerTest, DestroyedThreadStaysBusy) {
  std::optional<ProfiledThread> thread(std::in_place, &profiler_, "loader");
  {
    ProfileFrame<"ThreadBusyTest/Frame"> frame;
    ticks_.Advance(1000);
    thread->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileScope<"ThreadBusyTest/Destroyed"> scope;
      ticks.Advance(400);
    });
    thread.reset();
  }
  std::string report = profiler_.GetReport();
  EXPECT_NE(report.find("\nloader       400ns   400ns  40.0%     100%\n"),
            std::string::npos)
      << report;

  // A reset clears it, and a thread of the same name carries on after.
  profiler_.Reset();
  thread.emplace(&profiler_, "loader");
  {
    ProfileFrame<"ThreadBusyTest/Frame"> frame;
    ticks_.Advance(1000);
    thread->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileScope<"ThreadBusyTest/Destroyed"> scope;
      ticks.Advance(100);
    });
  }
  report = profiler_.GetReport();
  EXPECT_NE(report.find("\nloader       100ns   100ns  10.0%     100%\n"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, ResetClearsLiveThreadsBusyTime) {
  ProfiledThread thread(&profiler_, "loader");
  {
    ProfileFrame<"ThreadBusyTest/Frame"> frame;
    ticks_.Advance(1000);
    thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ticks.SetCoreClass(1);
      ProfileScope<"ThreadBusyTest/Live"> scope;
      ticks.Advance(400);
    });
  }
  profiler_.Reset();
  {
    ProfileFrame<"ThreadBusyTest/Frame"> frame;
    ticks_.Advance(1000);
    thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ticks.SetCoreClass(0);
      ProfileScope<"ThreadBusyTest/Live"> scope;
      ticks.Advance(100);
    });
  }
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Thread  Busy/frame     Max  Share  class 0
loader       100ns   100ns  10.0%     100%
main        1.00us  1.00us   100%     100%
)"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, ThreadsTableHasIdleTime) {
  ProfiledThread loader(&profiler_, "loader");
  ProfiledThread worker(&profiler_, "worker");

  // Idle time between frames isn't counted.
  loader.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileIdleTimer idle;
    ticks.Advance(5000);
  });
  {
    ProfileFrame<"ThreadIdleTest/Frame"> frame;
    ticks_.Advance(1000);
    loader.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      {
        ProfileIdleTimer idle;
        ticks.Advance(300);
      }
      ProfileScope<"ThreadIdleTest/Work"> work;
      ticks.Advance(100);
    });
  }

  // The main thread has no idle time, and a joined thread that wasn't idle has
  // none.
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Thread  Busy/frame     Max  Share  class 0  Idle/frame  Idle share
loader       100ns   100ns  10.0%     100%       300ns       30.0%
main        1.00us  1.00us   100%     100%           -           -
worker           0       0  0.00%        -           0       0.00%
)"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, NoIdleColumnsUntilIdleInFrame) {
  ProfiledThread thread(&profiler_, "loader");
  thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileIdleTimer idle;
    ticks.Advance(100);
  });

  // The main thread's idle time isn't reported.
  {
    ProfileIdleTimer idle;
    ticks_.Advance(100);
  }
  RecordFrames({1000});
  const std::string report = profiler_.GetReport();
  EXPECT_EQ(report.find("Idle/frame"), std::string::npos) << report;
}

// A wait still in progress as frames end counts in each frame, up to its end,
// and not before or between frames.
TEST_F(ProfilerTest, LongWaitIsIdleInEachFrame) {
  absl::Notification started;
  absl::Notification end;
  std::thread thread([this, &started, &end] {
    Profiler profiler({.main = &profiler_, .name = "loader"});
    ProfileIdleTimer idle;
    started.Notify();
    end.WaitForNotification();
  });
  started.WaitForNotification();
  ticks_.Advance(500);
  RecordFrames({1000});
  ticks_.Advance(200);
  RecordFrames({400});
  end.Notify();
  thread.join();
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Thread  Busy/frame     Max  Share  class 0  Idle/frame  Idle share
loader           0       0  0.00%        -       700ns        100%
main         700ns  1.00us   100%     100%           -           -
)"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, DestroyedThreadStaysIdle) {
  std::optional<ProfiledThread> thread(std::in_place, &profiler_, "loader");
  {
    ProfileFrame<"ThreadIdleTest/Frame"> frame;
    ticks_.Advance(1000);
    thread->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileIdleTimer idle;
      ticks.Advance(400);
    });
    thread.reset();
  }
  std::string report = profiler_.GetReport();
  EXPECT_NE(report.find("\nloader           0       0  0.00%        -       "
                        "400ns       40.0%\n"),
            std::string::npos)
      << report;

  // A reset clears it, and a thread of the same name carries on after.
  profiler_.Reset();
  thread.emplace(&profiler_, "loader");
  {
    ProfileFrame<"ThreadIdleTest/Frame"> frame;
    ticks_.Advance(1000);
    thread->Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileIdleTimer idle;
      ticks.Advance(100);
    });
  }
  report = profiler_.GetReport();
  EXPECT_NE(report.find("\nloader           0       0  0.00%        -       "
                        "100ns       10.0%\n"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, IdleTimerInTimedPointDies) {
  EXPECT_DEATH(
      {
        ProfiledThread thread(&profiler_);
        thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
          ProfileScope<"ThreadIdleTest/Scope"> scope;
          ProfileIdleTimer idle;
        });
      },
      "outside every timed point");

  // The same holds on the main thread, whose frames are timed points.
  EXPECT_DEATH(
      {
        ProfileFrame<"ThreadIdleTest/Frame"> frame;
        ProfileIdleTimer idle;
      },
      "outside every timed point");
}

TEST_F(ProfilerTest, TimedPointInIdleTimerDies) {
  EXPECT_DEATH(
      {
        ProfiledThread thread(&profiler_);
        thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
          ProfileIdleTimer idle;
          ProfileScope<"ThreadIdleTest/Scope"> scope;
        });
      },
      "can't be inside an idle timer");
}

TEST_F(ProfilerTest, NestedIdleTimersDie) {
  EXPECT_DEATH(
      {
        ProfiledThread thread(&profiler_);
        thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
          ProfileIdleTimer outer;
          ProfileIdleTimer inner;
        });
      },
      "Idle timers can't nest");
}

TEST_F(ProfilerTest, DestroyingProfilerInIdleTimerDies) {
  EXPECT_DEATH(
      {
        std::thread thread([this] {
          std::optional<Profiler> profiler(
              std::in_place, Profiler::Options{.main = &profiler_});
          ProfileIdleTimer idle;
          profiler.reset();
        });
        thread.join();
      },
      "destroyed in an idle timer");
}

TEST_F(ProfilerTest, ReadingValueAsSlowestFrameDies) {
  ProfilePoint value(ProfilePoint::Kind::kValue, "ProfilerTest/SlowestValue");
  EXPECT_DEATH(profiler_.GetSlowestFrameCount("ProfilerTest/SlowestValue"),
               "wrong kind");
}

//------------------------------------------------------------------------------
// Jobs
//------------------------------------------------------------------------------

// Runs a job of `job` on the calling thread, whose Profiler is `profiler`,
// queued `delay_ticks` before it starts, and running for `run_ticks`.
void RunJob(Profiler& profiler, FakeProfilerCpu& ticks, const ProfilePoint& job,
            int64_t delay_ticks, int64_t run_ticks) {
  const int64_t queued_ticks = profiler.ReadTicks();
  ticks.Advance(delay_ticks);
  ProfileJobTimer timer(job, queued_ticks);
  ticks.Advance(run_ticks);
}

TEST_F(ProfilerTest, JobSummaryWithNoRuns) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "JobTest/NoRuns");
  const Profiler::JobSummary summary =
      profiler_.GetJobSummary("JobTest/NoRuns");
  EXPECT_EQ(summary.runs, 0);
  EXPECT_EQ(summary.total, absl::ZeroDuration());
  EXPECT_EQ(summary.p50, absl::ZeroDuration());
  EXPECT_EQ(summary.max, absl::ZeroDuration());
  EXPECT_EQ(summary.start_delay, absl::ZeroDuration());
  EXPECT_EQ(profiler_.GetJobSummary("JobTest/Unknown").runs, 0);
}

TEST_F(ProfilerTest, ReadingNonJobAsJobDies) {
  ProfilePoint scope(ProfilePoint::Kind::kScope, "JobTest/Scope");
  EXPECT_DEATH(profiler_.GetJobSummary("JobTest/Scope"), "wrong kind");
}

TEST_F(ProfilerTest, JobPercentiles) {
  // Runs of 1us to 1000us, in a shuffled order.
  ProfilePoint job(ProfilePoint::Kind::kJob, "JobTest/Percentiles");
  for (int i = 0; i < 1000; ++i) {
    RunJob(profiler_, ticks_, job, 0, ((i * 7919) % 1000 + 1) * 1000);
  }
  const Profiler::JobSummary summary =
      profiler_.GetJobSummary("JobTest/Percentiles");
  EXPECT_EQ(summary.runs, 1000);
  EXPECT_EQ(summary.total, absl::Microseconds(500500));
  EXPECT_EQ(summary.average, absl::Microseconds(500.5));
  ExpectPercentile(summary.p50, absl::Microseconds(500));
  ExpectPercentile(summary.p90, absl::Microseconds(900));
  ExpectPercentile(summary.p99, absl::Microseconds(990));
  EXPECT_EQ(summary.max, absl::Microseconds(1000));
}

TEST_F(ProfilerTest, JoinedThreadJobsAddToMain) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "JobTest/Joined");
  RunJob(profiler_, ticks_, job, 5, 10);
  std::optional<ProfiledThread> thread(std::in_place, &profiler_);
  thread->Run([&job](Profiler& profiler, FakeProfilerCpu& ticks) {
    RunJob(profiler, ticks, job, 15, 30);
    {
      ProfileJobTimer timer(job, profiler.ReadTicks());
      const int64_t ready_ticks = profiler.ReadTicks();
      ticks.Advance(7);
      Profiler::AddResumeDelay(ready_ticks);
    }

    // A joined Profiler reads only its own thread.
    EXPECT_EQ(profiler.GetJobSummary("JobTest/Joined").runs, 2);
  });

  Profiler::JobSummary summary = profiler_.GetJobSummary("JobTest/Joined");
  EXPECT_EQ(summary.runs, 3);
  EXPECT_EQ(summary.total, absl::Nanoseconds(47));
  EXPECT_EQ(summary.max, absl::Nanoseconds(30));
  EXPECT_EQ(summary.start_delay, absl::Nanoseconds(20) / 3);
  EXPECT_EQ(summary.max_start_delay, absl::Nanoseconds(15));
  EXPECT_EQ(summary.resumes, 1);
  EXPECT_EQ(summary.resume_delay, absl::Nanoseconds(7));

  // What the thread recorded stays once it is destroyed.
  thread.reset();
  summary = profiler_.GetJobSummary("JobTest/Joined");
  EXPECT_EQ(summary.runs, 3);
  EXPECT_EQ(summary.total, absl::Nanoseconds(47));
  EXPECT_EQ(summary.max, absl::Nanoseconds(30));
  EXPECT_EQ(summary.resumes, 1);
  ExpectPercentile(summary.p90, absl::Nanoseconds(30));
}

TEST_F(ProfilerTest, ResetClearsJobsOnEveryThread) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "JobTest/Reset");
  RunJob(profiler_, ticks_, job, 100, 100);
  ProfiledThread thread(&profiler_);
  thread.Run([&job](Profiler& profiler, FakeProfilerCpu& ticks) {
    RunJob(profiler, ticks, job, 200, 200);
  });
  std::optional<ProfiledThread> destroyed(std::in_place, &profiler_);
  destroyed->Run([&job](Profiler& profiler, FakeProfilerCpu& ticks) {
    RunJob(profiler, ticks, job, 300, 300);
  });
  destroyed.reset();
  ASSERT_EQ(profiler_.GetJobSummary("JobTest/Reset").runs, 3);

  profiler_.Reset();
  Profiler::JobSummary summary = profiler_.GetJobSummary("JobTest/Reset");
  EXPECT_EQ(summary.runs, 0);
  EXPECT_EQ(summary.max, absl::ZeroDuration());
  EXPECT_EQ(profiler_.GetReport().find("JobTest/Reset"), std::string::npos);

  // What each thread records after the reset counts, with nothing from before.
  RunJob(profiler_, ticks_, job, 1, 1);
  thread.Run([&job](Profiler& profiler, FakeProfilerCpu& ticks) {
    RunJob(profiler, ticks, job, 2, 2);
  });
  summary = profiler_.GetJobSummary("JobTest/Reset");
  EXPECT_EQ(summary.runs, 2);
  EXPECT_EQ(summary.total, absl::Nanoseconds(3));
  EXPECT_EQ(summary.max, absl::Nanoseconds(2));
  EXPECT_EQ(summary.max_start_delay, absl::Nanoseconds(2));
  EXPECT_EQ(summary.p99, absl::Nanoseconds(2));
}

TEST_F(ProfilerTest, ResumedJobIsInJobsTableBeforeItEnds) {
  ProfilePoint job(ProfilePoint::Kind::kJob, "JobTest/Resumed");
  ProfileJobTimer timer(job, profiler_.ReadTicks());
  const int64_t ready_ticks = profiler_.ReadTicks();
  ticks_.Advance(40);
  Profiler::AddResumeDelay(ready_ticks);
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find("\nJobTest/Resumed      0    0         -    -    -    -"
                        "    -            -          -        1        40.0ns"
                        "      40.0ns\n"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, JobsTable) {
  ProfilePoint render(ProfilePoint::Kind::kJob, "JobTest/Render");
  ProfilePoint load(ProfilePoint::Kind::kJob, "JobTest/Load");
  ProfilePoint unused(ProfilePoint::Kind::kJob, "JobTest/Unused");
  RunJob(profiler_, ticks_, render, 10, 1000);
  RunJob(profiler_, ticks_, render, 30, 3000);
  RunJob(profiler_, ticks_, load, 0, 500);

  // Percentiles are the middle of their histogram bucket, up to the max.
  std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Job             Count     Run  Run/call    P50     P90     P99     Max  Start delay  Max start
JobTest/Load        1   500ns     500ns  496ns   496ns   496ns   500ns            0          0
JobTest/Render      2  4.00us    2.00us  992ns  2.94us  2.94us  3.00us       20.0ns     30.0ns
)"),
            std::string::npos)
      << report;

  // Once a job resumes, the table has resumes.
  {
    ProfileJobTimer timer(load, profiler_.ReadTicks());
    const int64_t ready_ticks = profiler_.ReadTicks();
    ticks_.Advance(40);
    Profiler::AddResumeDelay(ready_ticks);
  }
  report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Job             Count     Run  Run/call     P50     P90     P99     Max  Start delay  Max start  Resumes  Resume delay  Max resume
JobTest/Load        2   540ns     270ns  42.0ns   496ns   496ns   500ns            0          0        1        40.0ns      40.0ns
JobTest/Render      2  4.00us    2.00us   992ns  2.94us  2.94us  3.00us       20.0ns     30.0ns        0             -           -
)"),
            std::string::npos)
      << report;
}

//------------------------------------------------------------------------------
// Threadless main Profiler
//------------------------------------------------------------------------------

class ProfilerThreadlessTest : public ::testing::Test {
 protected:
  FakeProfilerCpu ticks_;
  std::vector<std::string> reports_;
  Profiler profiler_{{.fake_cpu = &ticks_,
                      .threadless = true,
                      .slow_frame = absl::Nanoseconds(500),
                      .on_slow_frame = [this](std::string_view report) {
                        reports_.emplace_back(report);
                      }}};
};

TEST_F(ProfilerThreadlessTest, IsNoThreadsProfiler) {
  {
    ProfileScope<"ThreadlessTest/Unrecorded"> scope;
    ticks_.Advance(10);
  }
  EXPECT_EQ(profiler_.GetCount("ThreadlessTest/Unrecorded"), 0);

  // So the thread can have a Profiler of its own.
  Profiler own({.fake_cpu = &ticks_});
  {
    ProfileScope<"ThreadlessTest/Unrecorded"> scope;
    ticks_.Advance(10);
  }
  EXPECT_EQ(own.GetCount("ThreadlessTest/Unrecorded"), 1);
  EXPECT_EQ(profiler_.GetCount("ThreadlessTest/Unrecorded"), 0);
}

TEST_F(ProfilerThreadlessTest, FramesOnEitherThread) {
  ProfiledThread thread_a(&profiler_, "a");
  ProfiledThread thread_b(&profiler_, "b");
  thread_a.Run([&thread_b](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileFrame<"ThreadlessTest/Frame"> frame;
    ticks.Advance(1000);
    thread_b.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileScope<"ThreadlessTest/Scope"> scope;
      ticks.Advance(30);
    });
  });
  thread_b.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileFrame<"ThreadlessTest/Frame"> frame;
    ProfileScope<"ThreadlessTest/Scope"> scope;
    ticks.Advance(300);
  });

  const Profiler::FrameSummary summary = profiler_.GetFrameSummary();
  EXPECT_EQ(summary.frames, 2);
  EXPECT_EQ(summary.total, absl::Nanoseconds(1300));
  EXPECT_EQ(profiler_.GetCount("ThreadlessTest/Frame"), 2);
  EXPECT_EQ(profiler_.GetCount("ThreadlessTest/Scope"), 2);
  EXPECT_EQ(profiler_.GetSelfTime("ThreadlessTest/Scope"),
            absl::Nanoseconds(330));

  // The slowest frame is the first, timed on a, with b's time apart from it.
  EXPECT_EQ(profiler_.GetSlowestFrameSelfTime("ThreadlessTest/Frame"),
            absl::Microseconds(1));
  EXPECT_EQ(profiler_.GetSlowestFrameSelfTime("ThreadlessTest/Scope"),
            absl::Nanoseconds(30));
  ASSERT_EQ(reports_.size(), 1);
  EXPECT_EQ(reports_[0], R"(Slow frame: 1.00us, core class 0, profiler 0
Kind   In frame  Alongside
frame    1.00us          -
scope         0     30.0ns
call          0          0

Point                 Kind   Count    Self
ThreadlessTest/Frame  frame      1  1.00us
ThreadlessTest/Scope  scope      1  30.0ns
)");

  // There is no main thread, and each thread is busy with the frames it timed.
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Thread  Busy/frame     Max  Share  class 0
a            500ns  1.00us  76.9%     100%
b            165ns   300ns  25.4%     100%

)"),
            std::string::npos)
      << report;
}

// A main loop run as a job, timing frames inside it, is busy in each frame for
// the frame's time, and not before or between them.
TEST_F(ProfilerThreadlessTest, MainLoopJobIsBusyInEachFrame) {
  std::thread thread([this] {
    Profiler profiler({.main = &profiler_, .name = "loop"});
    ProfileScope<"ThreadlessTest/Loop"> loop;
    ticks_.Advance(100);
    for (int64_t frame_ticks : {1000, 400}) {
      ProfileFrame<"ThreadlessTest/LoopFrame"> frame;
      ticks_.Advance(frame_ticks);
    }
    ticks_.Advance(100);
  });
  thread.join();
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Thread  Busy/frame     Max  Share  class 0
loop         700ns  1.00us   100%     100%
)"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerThreadlessTest, OneFrameAtATime) {
  EXPECT_DEATH(
      {
        ProfiledThread thread_a(&profiler_);
        ProfiledThread thread_b(&profiler_);
        thread_a.Run([&thread_b](Profiler& profiler, FakeProfilerCpu& ticks) {
          ProfileFrame<"ThreadlessTest/First"> frame;
          thread_b.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
            ProfileFrame<"ThreadlessTest/Second"> frame;
          });
        });
      },
      "Only one profile frame can be in progress at a time");
}

TEST_F(ProfilerThreadlessTest, FramesCantNest) {
  EXPECT_DEATH(
      {
        ProfiledThread thread(&profiler_);
        thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
          ProfileFrame<"ThreadlessTest/Outer"> outer;
          ProfileFrame<"ThreadlessTest/Inner"> inner;
        });
      },
      "can't nest");
}

TEST_F(ProfilerThreadlessTest, ResetBetweenFrames) {
  ProfiledThread thread_a(&profiler_);
  ProfiledThread thread_b(&profiler_);
  thread_a.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileFrame<"ThreadlessTest/ResetFrame"> frame;
    ticks.Advance(1000);
  });
  profiler_.Reset();
  thread_b.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
    ProfileFrame<"ThreadlessTest/ResetFrame"> frame;
    ticks.Advance(100);
  });
  const Profiler::FrameSummary summary = profiler_.GetFrameSummary();
  EXPECT_EQ(summary.frames, 1);
  EXPECT_EQ(summary.max, absl::Nanoseconds(100));
  EXPECT_EQ(profiler_.GetCount("ThreadlessTest/ResetFrame"), 1);
}

TEST_F(ProfilerThreadlessTest, ResetDuringFrameDies) {
  EXPECT_DEATH(
      {
        ProfiledThread thread(&profiler_);
        thread.Run([this](Profiler& profiler, FakeProfilerCpu& ticks) {
          ProfileFrame<"ThreadlessTest/ResetDuring"> frame;
          profiler_.Reset();
        });
      },
      "can't be reset during a frame");
}

TEST_F(ProfilerThreadlessTest, CantHaveMain) {
  Profiler main({.fake_cpu = &ticks_, .threadless = true});
  EXPECT_DEATH(
      Profiler({.fake_cpu = &ticks_, .main = &main, .threadless = true}),
      "A threadless Profiler can't have a main");
}

TEST(ProfilerThreadlessSetupTest, MadeAndDestroyedOnAnyThread) {
  FakeProfilerCpu ticks;
  std::optional<Profiler> main;
  std::thread make_thread([&main, &ticks] {
    main.emplace(Profiler::Options{.fake_cpu = &ticks, .threadless = true});
  });
  make_thread.join();
  std::thread frame_thread([&main] {
    Profiler profiler({.main = &*main});
    ProfileFrame<"ThreadlessTest/AnyThreadFrame"> frame;
  });
  frame_thread.join();
  EXPECT_EQ(main->GetFrameSummary().frames, 1);
  main.reset();
}

//------------------------------------------------------------------------------
// Own cost and budget
//------------------------------------------------------------------------------

// Every read of the ticks advances them by this much, so a timed point, which
// reads them twice, costs twice this.
constexpr int64_t kReadTicks = 100;

// Records a frame of `ticks`, with `scopes` scopes inside it.
void RecordFrame(FakeProfilerCpu& ticks, int64_t frame_ticks, int scopes) {
  ProfileFrame<"ProfilerCostTest/Frame"> frame;
  for (int i = 0; i < scopes; ++i) {
    ProfileScope<"ProfilerCostTest/Scope"> scope;
  }
  ticks.Advance(frame_ticks);
}

// A Profiler whose timed points each cost 2 * kReadTicks.
class ProfilerCostTest : public ::testing::Test {
 protected:
  ProfilerCostTest() {
    ticks_.SetAutoAdvance(kReadTicks);
    profiler_.emplace(Profiler::Options{.fake_cpu = &ticks_});
  }

  FakeProfilerCpu ticks_;
  std::optional<Profiler> profiler_;
};

TEST_F(ProfilerCostTest, MeasuresPointCost) {
  // The measurement's own closing read adds a little, spread over its points.
  EXPECT_NEAR(absl::ToDoubleNanoseconds(profiler_->GetPointCost()),
              2 * kReadTicks, 2);
}

TEST_F(ProfilerCostTest, CostPerFrame) {
  // Four timed points in two frames, and one outside them, which isn't in any
  // frame's cost.
  RecordFrame(ticks_, 1000, 2);
  {
    ProfileScope<"ProfilerCostTest/Outside"> scope;
  }
  RecordFrame(ticks_, 1000, 0);
  EXPECT_EQ(profiler_->GetFrameSummary().profiler_cost,
            profiler_->GetPointCost() * 2);
}

TEST_F(ProfilerCostTest, ResetClearsCostPerFrame) {
  const absl::Duration point_cost = profiler_->GetPointCost();
  RecordFrame(ticks_, 1000, 3);
  profiler_->Reset();
  RecordFrame(ticks_, 1000, 0);
  EXPECT_EQ(profiler_->GetFrameSummary().profiler_cost, point_cost);
  EXPECT_EQ(profiler_->GetPointCost(), point_cost);
}

TEST(ProfilerBudgetTest, NoBudgetIsInfinite) {
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks});
  RecordFrame(ticks, 1000, 0);
  EXPECT_EQ(profiler.GetFrameSummary().budget, absl::InfiniteDuration());
}

TEST(ProfilerBudgetTest, FractionAlone) {
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks, .budget_fraction = 0.25});
  RecordFrame(ticks, 1000, 0);
  EXPECT_EQ(profiler.GetFrameSummary().budget, absl::Nanoseconds(250));
}

TEST(ProfilerBudgetTest, LargerOfFixedAndFraction) {
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks,
                     .budget_per_frame = absl::Microseconds(500),
                     .budget_fraction = 0.25});

  // A quarter of 1ms is 250us, under the fixed 500us.
  RecordFrame(ticks, 1'000'000, 0);
  EXPECT_EQ(profiler.GetFrameSummary().budget, absl::Microseconds(500));

  // A quarter of the new average, 3ms, is 750us.
  RecordFrame(ticks, 5'000'000, 0);
  EXPECT_EQ(profiler.GetFrameSummary().budget, absl::Microseconds(750));
}

//------------------------------------------------------------------------------
// Report
//------------------------------------------------------------------------------

TEST_F(ProfilerTest, Report) {
  ProfileSetValue<"ReportTest/Tracks">(142);
  {
    ProfileFrame<"ReportTest/Run"> frame;
    ticks_.Advance(1000);
    {
      ProfileScope<"ReportTest/Refresh"> scope;
      ticks_.Advance(500);
    }
    for (int i = 0; i < 2; ++i) {
      ProfileCall<"ReportTest/GetTrack"> call;
      ticks_.Advance(400);
    }
    ProfileCount<"ReportTest/Messages">(3);
  }
  {
    ProfileScope<"ReportTest/Outside"> scope;
    ticks_.Advance(50);
  }
  {
    ProfileFrame<"ReportTest/Run"> frame;
    ticks_.Advance(300);
    ProfileCount<"ReportTest/Messages">(1);
  }

  // P50 and P90 are the middle of the buckets the frames are in. The scope
  // between frames is in the totals, but not the slowest frame. Each timed
  // point's share is its self time against the frames' total. The slowest
  // frame's points are slowest first, rather than in the report's order.
  EXPECT_EQ(profiler_.GetReport(),
            R"(Value              Latest
ReportTest/Tracks     142

Frames      2
Total       2.60us
Average     1.30us
P50         304ns
P90         2.18us
P99         2.18us
Max         2.30us
Profiler    0 per frame, budget none
Point cost  0
Tick rate   1.00GHz

Core     Frames  Average     Max
class 0       2   1.30us  2.30us

Point                Kind     Count  Count/frame    Self  Self/call  Self/frame  Share
ReportTest/Run       frame        2         1.00  1.30us      650ns       650ns  50.0%
ReportTest/Outside   scope        1         0.50  50.0ns     50.0ns      25.0ns  1.92%
ReportTest/Refresh   scope        1         0.50   500ns      500ns       250ns  19.2%
ReportTest/GetTrack  call         2         1.00   800ns      400ns       400ns  30.8%
ReportTest/Messages  counter      4         2.00       -          -           -      -

Slowest frame: 2.30us, core class 0, profiler 0
Kind     Self
frame  1.00us
scope   500ns
call    800ns

Point                Kind     Count    Self
ReportTest/Run       frame        1  1.00us
ReportTest/GetTrack  call         2   800ns
ReportTest/Refresh   scope        1   500ns
ReportTest/Messages  counter      3       -
)");
}

TEST_F(ProfilerTest, ReportWithNoFrames) {
  {
    ProfileScope<"ReportTest/NoFrames"> scope;
    ticks_.Advance(10);
  }
  EXPECT_EQ(profiler_.GetReport(),
            R"(Frames      0
Point cost  0
Tick rate   1.00GHz

Point                Kind   Count  Count/frame    Self  Self/call  Self/frame  Share
ReportTest/NoFrames  scope      1            -  10.0ns     10.0ns           -      -
)");
}

TEST_F(ProfilerTest, ReportWithNoFrameTime) {
  {
    ProfileFrame<"ReportTest/EmptyFrame"> frame;
  }
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find("ReportTest/EmptyFrame  frame      1         1.00     "
                        "0          0           0      -\n"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, ReportOrderIgnoresRegistration) {
  // Registered in the opposite order to the report's, and with a point that
  // this Profiler never records.
  ProfilePoint second(ProfilePoint::Kind::kScope, "ReportOrderTest/B");
  ProfilePoint unused(ProfilePoint::Kind::kScope, "ReportOrderTest/Unused");
  ProfilePoint first(ProfilePoint::Kind::kScope, "ReportOrderTest/A");
  ProfilePoint counter(ProfilePoint::Kind::kCounter, "ReportOrderTest/0");
  counter.Count(1);
  {
    ProfileTimer timer(second);
  }
  {
    ProfileTimer timer(first);
  }
  const std::string report = profiler_.GetReport();
  const int a = static_cast<int>(report.find("ReportOrderTest/A"));
  const int b = static_cast<int>(report.find("ReportOrderTest/B"));
  const int zero = static_cast<int>(report.find("ReportOrderTest/0"));
  EXPECT_GE(a, 0);
  EXPECT_LT(a, b);
  EXPECT_LT(b, zero);
  EXPECT_EQ(report.find("ReportOrderTest/Unused"), std::string::npos);
}

TEST(ProfilerReportTest, ProfilerCostAndBudget) {
  FakeProfilerCpu ticks;
  ticks.SetAutoAdvance(kReadTicks);
  Profiler profiler(
      {.fake_cpu = &ticks, .budget_per_frame = absl::Nanoseconds(300)});
  ticks.SetAutoAdvance(0);
  RecordFrame(ticks, 10'000, 1);

  // Two timed points at 201ns each (see ProfilerCostTest) are over budget.
  const std::string report = profiler.GetReport();
  EXPECT_NE(report.find("Profiler    402ns per frame, budget 300ns (over "
                        "budget)\nPoint cost  201ns\n"),
            std::string::npos)
      << report;
  EXPECT_NE(
      report.find("Slowest frame: 10.0us, core class 0, profiler 402ns\n"),
      std::string::npos)
      << report;
}

//------------------------------------------------------------------------------
// Tick rates and cores
//------------------------------------------------------------------------------

TEST(ProfilerTickRateTest, ReportsTickRate) {
  FakeProfilerCpu ticks(3'187'200'000);
  Profiler profiler({.fake_cpu = &ticks});
  const std::string report = profiler.GetReport();
  EXPECT_NE(report.find("Tick rate   3.19GHz\n"), std::string::npos) << report;
}

TEST_F(ProfilerTest, FramesByCoreClass) {
  ticks_.SetCoreClass(1);
  RecordFrames({1000});
  ticks_.SetCoreClass(0);
  RecordFrames({2000, 1000});

  // A frame that starts on one class and ends on another is mixed.
  {
    ProfileFrame<"ProfilerTest/Frames"> frame;
    ticks_.SetCoreClass(1);
    ticks_.Advance(3000);
  }

  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find(R"(
Core     Frames  Average     Max
class 0       2   1.50us  2.00us
class 1       1   1.00us  1.00us
mixed         1   3.00us  3.00us
Warning: frames ran on more than one class of core, which run at different speeds
)"),
            std::string::npos)
      << report;
  EXPECT_NE(report.find("Slowest frame: 3.00us, core mixed, profiler 0\n"),
            std::string::npos)
      << report;
}

TEST_F(ProfilerTest, OutOfRangeCoreClassesAreClamped) {
  ticks_.SetCoreClass(100);
  RecordFrames({1000});
  ticks_.SetCoreClass(-1);
  RecordFrames({1000});
  const std::string report = profiler_.GetReport();
  EXPECT_NE(report.find("\nclass 0       1"), std::string::npos) << report;
  EXPECT_NE(report.find("\nclass 7       1"), std::string::npos) << report;
}

TEST_F(ProfilerTest, ResetClearsCoreClasses) {
  ticks_.SetCoreClass(1);
  RecordFrames({1000});
  profiler_.Reset();
  ticks_.SetCoreClass(0);
  RecordFrames({1000});
  const std::string report = profiler_.GetReport();
  EXPECT_EQ(report.find("class 1"), std::string::npos) << report;
  EXPECT_EQ(report.find("Warning"), std::string::npos) << report;
}

//------------------------------------------------------------------------------
// Slow frames
//------------------------------------------------------------------------------

class ProfilerSlowFrameTest : public ::testing::Test {
 protected:
  FakeProfilerCpu ticks_;
  std::vector<std::string> reports_;
  Profiler profiler_{{.fake_cpu = &ticks_,
                      .slow_frame = absl::Microseconds(1),
                      .on_slow_frame = [this](std::string_view report) {
                        reports_.emplace_back(report);
                        ProfileCount<"SlowFrameTest/InCallback">(1);
                      }}};
};

TEST_F(ProfilerSlowFrameTest, ReportsOnlySlowFrames) {
  {
    ProfileFrame<"SlowFrameTest/Frame"> frame;
    ProfileScope<"SlowFrameTest/Fast"> scope;
    ticks_.Advance(1000);
  }
  EXPECT_TRUE(reports_.empty());

  {
    ProfileFrame<"SlowFrameTest/Frame"> frame;
    ProfileScope<"SlowFrameTest/Slow"> scope;
    ticks_.Advance(1500);
  }
  ASSERT_EQ(reports_.size(), 1);
  EXPECT_EQ(reports_[0], R"(Slow frame: 1.50us, core class 0, profiler 0
Kind     Self
frame       0
scope  1.50us
call        0

Point                Kind   Count    Self
SlowFrameTest/Slow   scope      1  1.50us
SlowFrameTest/Frame  frame      1       0
)");
}

TEST_F(ProfilerSlowFrameTest, BreakdownAddsUpKindsAndOrdersTiesByName) {
  {
    ProfileFrame<"SlowFrameTest/Frame"> frame;
    ticks_.Advance(100);
    {
      ProfileScope<"SlowFrameTest/B"> scope;
      ticks_.Advance(500);
    }
    {
      ProfileScope<"SlowFrameTest/A"> scope;
      ticks_.Advance(500);
    }
    {
      ProfileCall<"SlowFrameTest/Call"> call;
      ticks_.Advance(600);
    }
  }
  ASSERT_EQ(reports_.size(), 1);
  EXPECT_EQ(reports_[0], R"(Slow frame: 1.70us, core class 0, profiler 0
Kind     Self
frame   100ns
scope  1.00us
call    600ns

Point                Kind   Count   Self
SlowFrameTest/Call   call       1  600ns
SlowFrameTest/A      scope      1  500ns
SlowFrameTest/B      scope      1  500ns
SlowFrameTest/Frame  frame      1  100ns
)");
}

TEST_F(ProfilerSlowFrameTest, BreakdownHasOtherThreadsTime) {
  ProfiledThread thread(&profiler_);
  {
    ProfileFrame<"SlowFrameTest/Frame"> frame;
    ticks_.Advance(1000);
    thread.Run([](Profiler& profiler, FakeProfilerCpu& ticks) {
      ProfileScope<"SlowFrameTest/OnThread"> scope;
      ticks.Advance(300);
      ProfileCall<"SlowFrameTest/Call"> call;
      ticks.Advance(200);
    });
    ProfileScope<"SlowFrameTest/OnThread"> scope;
    ticks_.Advance(400);
  }

  // A point's self time is on every thread, and the frame's own adds up to the
  // frame's time.
  ASSERT_EQ(reports_.size(), 1);
  EXPECT_EQ(reports_[0], R"(Slow frame: 1.40us, core class 0, profiler 0
Kind   In frame  Alongside
frame    1.00us          -
scope     400ns      300ns
call          0      200ns

Point                   Kind   Count    Self
SlowFrameTest/Frame     frame      1  1.00us
SlowFrameTest/OnThread  scope      2   700ns
SlowFrameTest/Call      call       1   200ns
)");
}

TEST_F(ProfilerSlowFrameTest, CallbackIsOutsideFrame) {
  for (int64_t frame_ticks : {2000, 3000}) {
    ProfileFrame<"SlowFrameTest/CallbackFrame"> frame;
    ticks_.Advance(frame_ticks);
  }
  EXPECT_EQ(reports_.size(), 2);
  EXPECT_EQ(profiler_.GetCount("SlowFrameTest/InCallback"), 2);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("SlowFrameTest/InCallback"), 0);
}

// A slow frame time with more ticks than an int64_t holds is never reached.
TEST(ProfilerLongSlowFrameTest, IsNeverReached) {
  FakeProfilerCpu ticks;
  int reports = 0;
  Profiler profiler(
      {.fake_cpu = &ticks,
       .slow_frame = absl::Hours(24 * 365 * 1000),
       .on_slow_frame = [&reports](std::string_view report) { ++reports; }});
  {
    ProfileFrame<"SlowFrameTest/LongLimit"> frame;
    ticks.Advance(1000);
  }
  EXPECT_EQ(reports, 0);
}

// Tests that set up their own Profiler.

TEST(ProfilerSetupTest, NewProfilerStartsEmpty) {
  FakeProfilerCpu ticks;
  {
    Profiler profiler({.fake_cpu = &ticks});
    ProfileCount<"ProfilerTest/StartsEmpty">(1);
  }
  Profiler profiler({.fake_cpu = &ticks});
  EXPECT_EQ(profiler.GetCount("ProfilerTest/StartsEmpty"), 0);
}

TEST(ProfilerSetupTest, FreeReadsCostNothing) {
  FakeProfilerCpu ticks;
  Profiler profiler({.fake_cpu = &ticks});
  EXPECT_EQ(profiler.GetPointCost(), absl::ZeroDuration());
}

TEST(ProfilerSetupTest, OtherTicksPerSecond) {
  FakeProfilerCpu ticks(1'000);
  Profiler profiler({.fake_cpu = &ticks});
  {
    ProfileScope<"ProfilerTest/Milliseconds"> scope;
    ticks.Advance(3);
  }
  EXPECT_EQ(profiler.GetSelfTime("ProfilerTest/Milliseconds"),
            absl::Milliseconds(3));
}

// Only checks that the real timestamp counter and core are read. How long
// anything takes isn't checked, as no test depends on real time, but the report
// is logged, to see the cost of a timed point in an optimized build, and the
// tick rates.
TEST(ProfilerSetupTest, RealTicks) {
  Profiler profiler;
  {
    ProfileFrame<"ProfilerTest/RealFrame"> frame;
    ProfileScope<"ProfilerTest/RealTicks"> scope;
  }
  EXPECT_EQ(profiler.GetCount("ProfilerTest/RealTicks"), 1);
  EXPECT_GT(profiler.GetSelfTime("ProfilerTest/RealTicks"),
            absl::ZeroDuration());
  EXPECT_GT(profiler.GetPointCost(), absl::ZeroDuration());
  const std::string report = profiler.GetReport();
  EXPECT_NE(report.find("Slowest frame: "), std::string::npos) << report;
  LOG(INFO) << report;
}

}  // namespace
}  // namespace gb
