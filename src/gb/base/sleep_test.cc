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

}  // namespace
}  // namespace gb
