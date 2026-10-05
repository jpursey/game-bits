// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_LOCAL_TIME_H_
#define GB_BASE_LOCAL_TIME_H_

#include "absl/time/time.h"

namespace gb {

// Returns the user's local time zone, as the operating system reports it.
//
// Use this rather than absl::LocalTimeZone(), which is UTC on Windows, where
// there is no zoneinfo database for it to load the zone from. On Windows, the
// zone returned is the current offset from UTC, so it is off by the daylight
// saving change for times on the other side of one.
absl::TimeZone GetLocalTimeZone();

}  // namespace gb

#endif  // GB_BASE_LOCAL_TIME_H_
