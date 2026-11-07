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

}  // namespace gb

#endif  // GB_BASE_SLEEP_H_
