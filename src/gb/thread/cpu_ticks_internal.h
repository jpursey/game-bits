// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_CPU_TICKS_INTERNAL_H_
#define GB_THREAD_CPU_TICKS_INTERNAL_H_

namespace gb::internal {

//==============================================================================
// The ways GetCpuTicksPerSecond() (see cpu_ticks.h) can find the rate of the
// CPU's timestamp counter. This is private to gb_thread and its tests.
//==============================================================================

// Returns the rate the CPU reports for its timestamp counter, or 0 if it
// reports none.
double GetReportedCpuTicksPerSecond();

// Measures the rate of the CPU's timestamp counter against the steady clock,
// which takes about a millisecond.
double MeasureCpuTicksPerSecond();

}  // namespace gb::internal

#endif  // GB_THREAD_CPU_TICKS_INTERNAL_H_
