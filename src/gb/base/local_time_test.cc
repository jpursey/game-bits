// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/local_time.h"

#include <chrono>

#include "absl/time/time.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(LocalTimeTest, OffsetMatchesStandardLibrary) {
  const std::chrono::system_clock::time_point now =
      std::chrono::system_clock::now();
  const std::chrono::sys_info info = std::chrono::current_zone()->get_info(now);
  EXPECT_EQ(GetLocalTimeZone().At(absl::FromChrono(now)).offset,
            static_cast<int>(info.offset.count()));
}

}  // namespace
}  // namespace gb
