// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <ctime>

#include "gb/base/local_time.h"

namespace gb {

absl::TimeZone GetLocalTimeZone() {
  // A browser has neither a TZ variable nor zoneinfo files for
  // absl::LocalTimeZone() to load the zone from, so it would be UTC. The C
  // library's local time comes from JavaScript's Date, which knows the zone.
  const std::time_t now = std::time(nullptr);
  std::tm local;
  if (localtime_r(&now, &local) == nullptr) {
    return absl::UTCTimeZone();
  }
  return absl::FixedTimeZone(static_cast<int>(local.tm_gmtoff));
}

}  // namespace gb
