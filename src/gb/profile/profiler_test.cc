// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profiler.h"

#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "absl/log/log.h"
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
  // Records a frame of each of `frame_ticks`, with nothing inside it.
  void RecordFrames(const std::vector<int64_t>& frame_ticks) {
    for (int64_t ticks : frame_ticks) {
      ProfileFrame<"ProfilerTest/Frames"> frame;
      ticks_.Advance(ticks);
    }
  }

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

TEST_F(ProfilerTest, ReadingValueAsSlowestFrameDies) {
  ProfilePoint value(ProfilePoint::Kind::kValue, "ProfilerTest/SlowestValue");
  EXPECT_DEATH(profiler_.GetSlowestFrameCount("ProfilerTest/SlowestValue"),
               "wrong kind");
}

//------------------------------------------------------------------------------
// Own cost and budget
//------------------------------------------------------------------------------

// Every read of the ticks advances them by this much, so a timed point, which
// reads them twice, costs twice this.
constexpr int64_t kReadTicks = 100;

// Records a frame of `ticks`, with `scopes` scopes inside it.
void RecordFrame(FakeTicks& ticks, int64_t frame_ticks, int scopes) {
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
    profiler_.emplace(Profiler::Options{.fake_ticks = &ticks_});
  }

  FakeTicks ticks_;
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
  FakeTicks ticks;
  Profiler profiler({.fake_ticks = &ticks});
  RecordFrame(ticks, 1000, 0);
  EXPECT_EQ(profiler.GetFrameSummary().budget, absl::InfiniteDuration());
}

TEST(ProfilerBudgetTest, FractionAlone) {
  FakeTicks ticks;
  Profiler profiler({.fake_ticks = &ticks, .budget_fraction = 0.25});
  RecordFrame(ticks, 1000, 0);
  EXPECT_EQ(profiler.GetFrameSummary().budget, absl::Nanoseconds(250));
}

TEST(ProfilerBudgetTest, LargerOfFixedAndFraction) {
  FakeTicks ticks;
  Profiler profiler({.fake_ticks = &ticks,
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
  // between frames is in the totals, but not the slowest frame. The slowest
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
Tick rate   1.00GHz measured, 1.00GHz reported

Core     Frames  Average     Max
class 0       2   1.30us  2.30us

Point                Kind     Count  Count/frame    Self  Self/call  Self/frame
ReportTest/Run       frame        2         1.00  1.30us      650ns       650ns
ReportTest/Outside   scope        1         0.50  50.0ns     50.0ns      25.0ns
ReportTest/Refresh   scope        1         0.50   500ns      500ns       250ns
ReportTest/GetTrack  call         2         1.00   800ns      400ns       400ns
ReportTest/Messages  counter      4         2.00       -          -           -

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
Tick rate   1.00GHz measured, 1.00GHz reported

Point                Kind   Count  Count/frame    Self  Self/call  Self/frame
ReportTest/NoFrames  scope      1            -  10.0ns     10.0ns           -
)");
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
  FakeTicks ticks;
  ticks.SetAutoAdvance(kReadTicks);
  Profiler profiler(
      {.fake_ticks = &ticks, .budget_per_frame = absl::Nanoseconds(300)});
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

TEST(ProfilerTickRateTest, MeasuredAndReportedRates) {
  FakeTicks ticks(3'187'200'000);
  ticks.SetCpuTicksPerSecond(3'000'000'000);
  Profiler profiler({.fake_ticks = &ticks});
  const std::string report = profiler.GetReport();
  EXPECT_NE(report.find("Tick rate   3.19GHz measured, 3.00GHz reported\n"),
            std::string::npos)
      << report;
}

TEST(ProfilerTickRateTest, NoReportedRate) {
  FakeTicks ticks;
  ticks.SetCpuTicksPerSecond(0);
  Profiler profiler({.fake_ticks = &ticks});
  const std::string report = profiler.GetReport();
  EXPECT_NE(report.find("Tick rate   1.00GHz measured, none reported\n"),
            std::string::npos)
      << report;
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
  FakeTicks ticks_;
  std::vector<std::string> reports_;
  Profiler profiler_{{.fake_ticks = &ticks_,
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

TEST_F(ProfilerSlowFrameTest, CallbackIsOutsideFrame) {
  for (int64_t frame_ticks : {2000, 3000}) {
    ProfileFrame<"SlowFrameTest/CallbackFrame"> frame;
    ticks_.Advance(frame_ticks);
  }
  EXPECT_EQ(reports_.size(), 2);
  EXPECT_EQ(profiler_.GetCount("SlowFrameTest/InCallback"), 2);
  EXPECT_EQ(profiler_.GetSlowestFrameCount("SlowFrameTest/InCallback"), 0);
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

TEST(ProfilerSetupTest, FreeReadsCostNothing) {
  FakeTicks ticks;
  Profiler profiler({.fake_ticks = &ticks});
  EXPECT_EQ(profiler.GetPointCost(), absl::ZeroDuration());
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
