// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "absl/time/clock.h"
#include "gb/base/sleep.h"

namespace gb {

void PreciseSleepFor(absl::Duration duration) { absl::SleepFor(duration); }

}  // namespace gb
