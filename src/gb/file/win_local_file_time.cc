// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/file/local_file_time.h"

namespace gb::internal {

namespace {

// Windows file times count 100 nanosecond ticks from 1601-01-01 UTC, as
// FILETIME does. std::chrono's own conversion goes through utc_clock, which
// loads the time zone database to look up leap seconds.
constexpr absl::Duration kTick = absl::Nanoseconds(100);
constexpr absl::Time kEpoch = absl::FromUnixSeconds(-11644473600);

}  // namespace

absl::Time FromLocalFileTime(std::filesystem::file_time_type time) {
  return kEpoch + kTick * time.time_since_epoch().count();
}

std::filesystem::file_time_type ToLocalFileTime(absl::Time time) {
  return std::filesystem::file_time_type(
      std::filesystem::file_time_type::duration((time - kEpoch) / kTick));
}

}  // namespace gb::internal
