#include "gb/base/clock.h"
#include "gb/base/fake_clock.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(RealtimeClockTest, Now) {
  RealtimeClock clock;
  auto clock_now = clock.Now();
  auto now = absl::Now();
  EXPECT_GE(now, clock_now);
  EXPECT_LT(clock_now, now + absl::Seconds(1));
}

TEST(RealtimeClockTest, SleepFor) {
  RealtimeClock clock;
  auto sleep_duration = absl::Milliseconds(10);
  auto now = absl::Now();
  clock.SleepFor(sleep_duration);
  auto after = absl::Now();
  EXPECT_GE(after, now + sleep_duration);
  EXPECT_LT(after, now + sleep_duration + absl::Seconds(1));
}

TEST(FakeClockTest, StartsAtEpoch) {
  FakeClock clock;
  EXPECT_EQ(clock.Now(), absl::Time());
  EXPECT_EQ(clock.GetTime(), absl::Time());
}

TEST(FakeClockTest, SetNow) {
  FakeClock clock;
  auto now = absl::Now();
  clock.SetTime(now);
  EXPECT_EQ(clock.Now(), now);
  EXPECT_EQ(clock.GetTime(), now);
}

TEST(FakeClockTest, Advance) {
  FakeClock clock;
  auto now = absl::Now();
  clock.SetTime(now);
  auto advance_amount = absl::Hours(1);
  clock.AdvanceTime(advance_amount);
  EXPECT_EQ(clock.Now(), now + advance_amount);
  EXPECT_EQ(clock.GetTime(), now + advance_amount);
}

TEST(FakeClockTest, NowDoesNotAutoAdvance) {
  FakeClock clock;
  auto now = absl::Now();
  clock.SetTime(now);
  EXPECT_EQ(clock.Now(), now);
  EXPECT_EQ(clock.Now(), now);
  EXPECT_EQ(clock.GetTime(), now);
}

TEST(FakeClockTest, NowDoesAutoAdvance) {
  FakeClock clock;
  auto now = absl::Now();
  clock.SetTime(now);
  auto advance_amount = absl::Minutes(1);
  clock.SetAutoAdvance(advance_amount);
  EXPECT_EQ(clock.Now(), now + advance_amount);
  EXPECT_EQ(clock.Now(), now + advance_amount * 2);
  EXPECT_EQ(clock.GetTime(), now + advance_amount * 2);
  EXPECT_EQ(clock.GetTime(), now + advance_amount * 2);
}

TEST(FakeClockTest, SleepForAdvances) {
  FakeClock clock;
  auto now = absl::Now();
  clock.SetTime(now);
  auto sleep_amount = absl::Seconds(1);
  clock.SleepFor(sleep_amount);
  EXPECT_EQ(clock.GetTime(), now + sleep_amount);
}

TEST(FakeClockTest, SleepForWithOffset) {
  FakeClock clock;
  auto now = absl::Now();
  clock.SetTime(now);
  auto sleep_offset = absl::Milliseconds(1);
  clock.SetSleepOffset(sleep_offset);
  auto sleep_amount = absl::Seconds(1);
  clock.SleepFor(sleep_amount);
  EXPECT_EQ(clock.GetTime(), now + sleep_amount + sleep_offset);
}

TEST(FakeClockTest, SleepOffsetDoesNotAffectNow) {
  FakeClock clock;
  auto now = absl::Now();
  clock.SetTime(now);
  clock.SetSleepOffset(absl::Milliseconds(1));
  EXPECT_EQ(clock.Now(), now);
  EXPECT_EQ(clock.Now(), now);
  EXPECT_EQ(clock.GetTime(), now);
}

TEST(FakeClockTest, AutoAdvanceDoesNotAffectSleepFor) {
  FakeClock clock;
  auto now = absl::Now();
  clock.SetTime(now);
  clock.SetAutoAdvance(absl::Milliseconds(1));
  auto sleep_amount = absl::Seconds(1);
  clock.SleepFor(sleep_amount);
  EXPECT_EQ(clock.GetTime(), now + sleep_amount);
}

}  // namespace 
}  // namespace gb
