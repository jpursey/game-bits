// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_SLEEP_H_
#define GB_BASE_SLEEP_H_

#include "absl/time/time.h"

namespace gb {

// Sleeps the current thread for `duration`, waking within about a millisecond
// of it without spinning. Returns immediately if `duration` is not positive.
//
// Use this rather than absl::SleepFor(), which on Windows wakes only at the
// system timer's tick (every 15.6 ms by default), so it can overshoot by most
// of a frame.
void PreciseSleepFor(absl::Duration duration);

// Sleeps the current thread until `time` (as absl::Now() reads it), waking as
// close to it as the platform allows, to pace work such as a game's frames.
// Returns immediately if `time` has passed.
//
// Where the platform's sleep can't wake close enough, this sleeps until shortly
// before `time` and spins for the rest, keeping a core busy for that part. To
// wait without ever spinning, use PreciseSleepFor().
void PreciseSleepUntil(absl::Time time);

}  // namespace gb

#endif  // GB_BASE_SLEEP_H_
