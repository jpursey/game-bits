// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "absl/time/clock.h"
#include "gb/base/sleep.h"

namespace gb {
namespace {

// absl::SleepFor() usually wakes within a fraction of this late, so
// PreciseSleepUntil() sleeps until this long before its time, and spins for
// the rest.
constexpr absl::Duration kSpinTime = absl::Microseconds(500);

}  // namespace

void PreciseSleepFor(absl::Duration duration) { absl::SleepFor(duration); }

void PreciseSleepUntil(absl::Time time) {
  // Sleeps again if woken early, or if the clock was set back, so only the last
  // kSpinTime is ever spun.
  absl::Duration remaining = time - absl::Now();
  while (remaining > kSpinTime) {
    PreciseSleepFor(remaining - kSpinTime);
    remaining = time - absl::Now();
  }
  while (remaining > absl::ZeroDuration()) {
    remaining = time - absl::Now();
  }
}

}  // namespace gb
