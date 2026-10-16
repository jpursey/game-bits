// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_FAKE_PROFILER_CPU_H_
#define GB_PROFILE_FAKE_PROFILER_CPU_H_

#include <cstdint>

namespace gb {

//==============================================================================
// FakeProfilerCpu
//==============================================================================

// Stands in for what a Profiler reads about the CPU, for tests: the timestamp
// counter and its rate (see gb/thread/cpu_ticks.h), and the class of core the
// thread is on (see GetThisThreadCoreClass() in gb/thread/thread.h). Each
// method has the name and type of the call it stands in for.
//
// The ticks start at zero and only change when the test advances them, or
// when they are read, if an auto advance is set.
class FakeProfilerCpu {
 public:
  // Every tick is a nanosecond by default.
  explicit FakeProfilerCpu(double ticks_per_second = 1e9)
      : ticks_per_second_(ticks_per_second) {}
  FakeProfilerCpu(const FakeProfilerCpu&) = delete;
  FakeProfilerCpu& operator=(const FakeProfilerCpu&) = delete;
  ~FakeProfilerCpu() = default;

  // Advances the ticks by the auto advance, and returns them.
  int64_t ReadCpuTicks() {
    ticks_ += auto_advance_;
    return ticks_;
  }

  double GetCpuTicksPerSecond() const { return ticks_per_second_; }

  // The class of core, 0 unless set.
  int GetThisThreadCoreClass() const { return core_class_; }
  void SetCoreClass(int core_class) { core_class_ = core_class; }

  void Advance(int64_t ticks) { ticks_ += ticks; }

  // Sets how far each call to ReadCpuTicks() advances the ticks, such as to
  // give each read of the ticks a cost.
  void SetAutoAdvance(int64_t ticks) { auto_advance_ = ticks; }

 private:
  const double ticks_per_second_;
  int core_class_ = 0;
  int64_t ticks_ = 0;
  int64_t auto_advance_ = 0;
};

}  // namespace gb

#endif  // GB_PROFILE_FAKE_PROFILER_CPU_H_
