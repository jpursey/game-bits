// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/local_time.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb {

absl::TimeZone GetLocalTimeZone() {
  TIME_ZONE_INFORMATION info;
  const DWORD result = GetTimeZoneInformation(&info);
  if (result == TIME_ZONE_ID_INVALID) {
    return absl::UTCTimeZone();
  }

  // Windows biases are minutes to add to local time to get UTC, the opposite
  // of an offset from UTC.
  const LONG bias_mins =
      info.Bias +
      (result == TIME_ZONE_ID_DAYLIGHT ? info.DaylightBias : info.StandardBias);
  return absl::FixedTimeZone(-bias_mins * 60);
}

}  // namespace gb
