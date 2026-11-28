// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/sleep.h"

#include <algorithm>

#include "absl/strings/str_cat.h"
#include "absl/time/clock.h"
#include "gb/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// These sleep in real time, so they only check loosely that the sleep is about
// as long as asked, leaving room for a busy machine.

TEST(SleepTest, SleepsAtLeastDuration) {
  const absl::Duration duration = absl::Milliseconds(10);
  const absl::Time start = absl::Now();
  PreciseSleepFor(duration);
  const absl::Duration elapsed = absl::Now() - start;
  EXPECT_GE(elapsed, duration);
  EXPECT_LT(elapsed, duration + absl::Seconds(1));
}

TEST(SleepTest, EachShortSleepLastsAtLeastDuration) {
  // absl::Now() can be coarser than the clock a sleep wakes by, as it is on the
  // web, so a sleep that is long enough by its own clock can still look short.
  const int kSleepCount = 10;
  for (const absl::Duration duration :
       {absl::Milliseconds(1), absl::Microseconds(1500)}) {
    for (int i = 0; i < kSleepCount; ++i) {
      const absl::Time start = absl::Now();
      PreciseSleepFor(duration);
      EXPECT_GE(absl::Now() - start, duration)
          << "sleep " << i << " of " << duration;
    }
  }
}

TEST(SleepTest, WakesWithinTimerTick) {
  // Were they only woken at the system timer's tick, each of these short
  // sleeps would take a whole 15.6 ms tick, once the first has lined them up
  // with it. A busy machine can delay any one of them, so this only needs one
  // to wake well within a tick.
  const absl::Duration duration = absl::Milliseconds(1);
  const absl::Duration within_tick = absl::Milliseconds(8);
  const int kMaxSleepCount = 100;
  PreciseSleepFor(duration);
  absl::Duration shortest = absl::InfiniteDuration();
  for (int i = 0; i < kMaxSleepCount && shortest >= within_tick; ++i) {
    const absl::Time start = absl::Now();
    PreciseSleepFor(duration);
    shortest = std::min(shortest, absl::Now() - start);
  }
  EXPECT_LT(shortest, within_tick);
}

TEST(SleepTest, SleepsOnSeveralThreadsAtOnce) {
  GB_SKIP_WITHOUT_THREADS();
  // Threads sleep for different durations, so any thread woken by another's
  // timer would wake early.
  const int kThreadCount = 4;
  const int kSleepCount = 10;
  ThreadTester tester;
  for (int i = 0; i < kThreadCount; ++i) {
    const absl::Duration duration = absl::Milliseconds(1 + i * 3);
    tester.Run(absl::StrCat("thread-", i), [duration]() {
      for (int j = 0; j < kSleepCount; ++j) {
        const absl::Time start = absl::Now();
        PreciseSleepFor(duration);
        if (absl::Now() - start < duration) {
          return false;
        }
      }
      return true;
    });
  }
  EXPECT_TRUE(tester.Complete());
}

TEST(SleepTest, ReturnsAtOnceWhenNotPositive) {
  const absl::Time start = absl::Now();
  PreciseSleepFor(absl::ZeroDuration());
  PreciseSleepFor(-absl::Seconds(1));
  EXPECT_LT(absl::Now() - start, absl::Milliseconds(500));
}

TEST(SleepTest, SleepsUntilTime) {
  const absl::Time time = absl::Now() + absl::Milliseconds(10);
  PreciseSleepUntil(time);
  const absl::Time after = absl::Now();
  EXPECT_GE(after, time);
  EXPECT_LT(after, time + absl::Seconds(1));
}

TEST(SleepTest, WakesCloseToTime) {
  // Were they only woken at the system timer's tick, each of these short
  // sleeps would wake up to a whole 15.6 ms tick late. A busy machine can delay
  // any one of them, so this only needs one to wake well within a tick.
  const absl::Duration duration = absl::Milliseconds(2);
  const absl::Duration within_tick = absl::Milliseconds(4);
  const int kMaxSleepCount = 100;
  absl::Duration closest = absl::InfiniteDuration();
  for (int i = 0; i < kMaxSleepCount && closest >= within_tick; ++i) {
    const absl::Time time = absl::Now() + duration;
    PreciseSleepUntil(time);
    closest = std::min(closest, absl::Now() - time);
  }
  EXPECT_LT(closest, within_tick);
}

TEST(SleepTest, SleepUntilReturnsAtOnceWhenPast) {
  const absl::Time start = absl::Now();
  PreciseSleepUntil(start);
  PreciseSleepUntil(start - absl::Seconds(1));
  EXPECT_LT(absl::Now() - start, absl::Milliseconds(500));
}

}  // namespace
}  // namespace gb
