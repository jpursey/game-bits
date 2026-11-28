// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <emscripten.h>

#include <cstdlib>
#include <optional>
#include <string>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gb/base/local_time.h"
#include "gb/base/scoped_call.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Returns JavaScript's current offset from UTC. getTimezoneOffset() is minutes
// to add to local time to get UTC, the opposite of an offset from UTC.
EM_JS(int, GetJavaScriptOffsetSecs, (),
      { return -new Date().getTimezoneOffset() * 60; });

int GetOffsetSecs(absl::TimeZone zone) { return zone.At(absl::Now()).offset; }

TEST(WebLocalTimeTest, OffsetMatchesJavaScript) {
  EXPECT_EQ(GetOffsetSecs(GetLocalTimeZone()), GetJavaScriptOffsetSecs());
}

TEST(WebLocalTimeTest, FollowsJavaScriptRatherThanTzVariable) {
  // A browser has no TZ variable or zoneinfo, so JavaScript's zone is the only
  // one there is. Setting TZ to a zone other than JavaScript's checks that the
  // zone comes from JavaScript, wherever there is zoneinfo to load TZ from.
  const int js_offset_secs = GetJavaScriptOffsetSecs();
  const char* tz_name =
      (js_offset_secs == 5 * 3600 + 30 * 60 ? "Asia/Tokyo" : "Asia/Kolkata");
  const char* tz = std::getenv("TZ");
  const std::optional<std::string> old_tz =
      (tz != nullptr ? std::optional<std::string>(tz) : std::nullopt);
  ScopedCall restore_tz([&] {
    if (old_tz.has_value()) {
      setenv("TZ", old_tz->c_str(), 1);
    } else {
      unsetenv("TZ");
    }
  });
  ASSERT_EQ(setenv("TZ", tz_name, 1), 0);
  EXPECT_EQ(GetOffsetSecs(GetLocalTimeZone()), js_offset_secs);
}

}  // namespace
}  // namespace gb
