// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/cpu_ticks.h"

#include <cstdint>

#include "gb/thread/cpu_ticks_internal.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// A 1ms measurement is within 100ppm of the true rate, limited by the steady
// clock's resolution, so this allows ten times that.
inline constexpr double kRateTolerance = 1e-3;

TEST(CpuTicksTest, TicksIncrease) {
  const int64_t start = ReadCpuTicks();
  EXPECT_GT(ReadCpuTicks(), start);
}

TEST(CpuTicksTest, TicksPerSecondMatchesMeasuredRate) {
  const double measured = internal::MeasureCpuTicksPerSecond();
  EXPECT_GT(GetCpuTicksPerSecond(), 0);
  EXPECT_NEAR(GetCpuTicksPerSecond(), measured, measured * kRateTolerance);
}

TEST(CpuTicksTest, ReportedRateMatchesMeasuredRate) {
  const double reported = internal::GetReportedCpuTicksPerSecond();
  if (reported == 0) {
    GTEST_SKIP() << "The CPU does not report its timestamp counter's rate";
  }
  EXPECT_NEAR(internal::MeasureCpuTicksPerSecond(), reported,
              reported * kRateTolerance);
}

}  // namespace
}  // namespace gb
