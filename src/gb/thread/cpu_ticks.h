// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_CPU_TICKS_H_
#define GB_THREAD_CPU_TICKS_H_

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include <cstdint>

namespace gb {

//==============================================================================
// This file defines access to the CPU's timestamp counter, which counts ticks
// at a fixed rate whatever speed each core runs at. The counter must be
// invariant, which is true of any x86-64 CPU in the last decade.
//==============================================================================

// Returns the CPU's timestamp counter, in ticks.
//
// This function is thread-safe.
inline int64_t ReadCpuTicks() { return static_cast<int64_t>(__rdtsc()); }

// Returns how many ticks the CPU's timestamp counter counts per second, which
// converts a difference in ticks to time.
//
// The first call may take about a millisecond, and later calls return the same
// rate.
//
// This function is thread-safe.
double GetCpuTicksPerSecond();

}  // namespace gb

#endif  // GB_THREAD_CPU_TICKS_H_
