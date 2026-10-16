// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/cpu_ticks.h"

#include "gb/thread/cpu_ticks_internal.h"

namespace gb {

namespace {

// Returns the rate the CPU reports, if it does, and otherwise measures it.
double FindCpuTicksPerSecond() {
  const double reported = internal::GetReportedCpuTicksPerSecond();
  if (reported > 0) {
    return reported;
  }
  return internal::MeasureCpuTicksPerSecond();
}

}  // namespace

double GetCpuTicksPerSecond() {
  static const double s_ticks_per_second = FindCpuTicksPerSecond();
  return s_ticks_per_second;
}

}  // namespace gb
