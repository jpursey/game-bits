// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/thread/cpu_ticks_internal.h"

#ifndef _MSC_VER
#include <cpuid.h>
#endif

#include <chrono>
#include <cstdint>
#include <limits>

#include "gb/thread/cpu_ticks.h"

namespace gb::internal {

namespace {

using SteadyClock = std::chrono::steady_clock;

// Long enough that the steady clock's resolution (100ns on Windows) is at most
// 0.01% of it.
constexpr SteadyClock::duration kCalibrationTime = std::chrono::milliseconds(1);

// How many times to read each end of the calibration, keeping the best.
constexpr int kClockPairReads = 5;

// The timestamp counter and the steady clock, read at the same moment.
struct ClockPair {
  int64_t ticks;
  SteadyClock::time_point time;
};

// Reads the steady clock between two reads of the timestamp counter, several
// times, and keeps the read with the least time between the two. A thread
// switch between the reads would pair the clock with the wrong ticks, so this
// discards any read that one landed in.
ClockPair ReadClockPair() {
  ClockPair best = {};
  int64_t best_spread = std::numeric_limits<int64_t>::max();
  for (int i = 0; i < kClockPairReads; ++i) {
    const int64_t before = ReadCpuTicks();
    const SteadyClock::time_point time = SteadyClock::now();
    const int64_t after = ReadCpuTicks();
    if (after - before < best_spread) {
      best_spread = after - before;
      best = {before + (after - before) / 2, time};
    }
  }
  return best;
}

}  // namespace

// CPUID leaf 0x15 gives the rate as a ratio (EBX / EAX) of the core crystal
// clock (ECX, in Hz), each 0 if the CPU doesn't report it.
double GetReportedCpuTicksPerSecond() {
  uint32_t eax = 0;
  uint32_t ebx = 0;
  uint32_t ecx = 0;
#ifdef _MSC_VER
  int registers[4];
  __cpuid(registers, 0);
  if (registers[0] >= 0x15) {
    __cpuid(registers, 0x15);
    eax = static_cast<uint32_t>(registers[0]);
    ebx = static_cast<uint32_t>(registers[1]);
    ecx = static_cast<uint32_t>(registers[2]);
  }
#else
  uint32_t edx = 0;
  __get_cpuid(0x15, &eax, &ebx, &ecx, &edx);
#endif
  if (eax == 0 || ebx == 0 || ecx == 0) {
    return 0;
  }
  return static_cast<double>(ecx) * ebx / eax;
}

// The steady clock, unlike the wall clock, is never adjusted.
double MeasureCpuTicksPerSecond() {
  const ClockPair start = ReadClockPair();
  ClockPair end;
  do {
    end = ReadClockPair();
  } while (end.time - start.time < kCalibrationTime);
  return static_cast<double>(end.ticks - start.ticks) /
         std::chrono::duration<double>(end.time - start.time).count();
}

}  // namespace gb::internal
