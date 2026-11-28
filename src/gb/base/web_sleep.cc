// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "absl/time/clock.h"
#include "gb/base/sleep.h"

namespace gb {
namespace {

// absl::Now() reads JavaScript's Date.now(), which counts whole milliseconds,
// while absl::SleepFor() wakes by performance.now(), which is finer. So a sleep
// can end before absl::Now() says its time has passed, and then sleeps on in
// steps of kCatchUpStep until it does.
constexpr absl::Duration kCatchUpStep = absl::Microseconds(100);

// absl::Now() catches up within its next tick or two, so this bounds the steps
// in case the clock was set back.
constexpr absl::Duration kMaxCatchUp = absl::Milliseconds(2);

}  // namespace

void PreciseSleepFor(absl::Duration duration) {
  if (duration <= absl::ZeroDuration()) {
    return;
  }
  const absl::Time end = absl::Now() + duration;
  absl::SleepFor(duration);
  for (absl::Duration caught_up; caught_up < kMaxCatchUp && absl::Now() < end;
       caught_up += kCatchUpStep) {
    absl::SleepFor(kCatchUpStep);
  }
}

void PreciseSleepUntil(absl::Time time) {
  // PreciseSleepFor() already ends only once absl::Now() reaches its time, and
  // a spin couldn't wake closer to a clock that only moves once a millisecond.
  // This sleeps again if the clock was set back.
  absl::Duration remaining = time - absl::Now();
  while (remaining > absl::ZeroDuration()) {
    PreciseSleepFor(remaining);
    remaining = time - absl::Now();
  }
}

}  // namespace gb
