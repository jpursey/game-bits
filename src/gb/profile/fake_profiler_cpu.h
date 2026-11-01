// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_FAKE_PROFILER_CPU_H_
#define GB_PROFILE_FAKE_PROFILER_CPU_H_

#include <atomic>
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
//
// Any thread may call it, so Profilers on several threads can share one, as
// they share the CPU's timestamp counter. Every thread then sees the same
// ticks and class of core, and an auto advance applies to every read, on any
// thread. This class is thread-safe.
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
    const int64_t advance = auto_advance_.load(std::memory_order_relaxed);
    return ticks_.fetch_add(advance, std::memory_order_relaxed) + advance;
  }

  double GetCpuTicksPerSecond() const { return ticks_per_second_; }

  // The class of core, 0 unless set.
  int GetThisThreadCoreClass() const {
    return core_class_.load(std::memory_order_relaxed);
  }
  void SetCoreClass(int core_class) {
    core_class_.store(core_class, std::memory_order_relaxed);
  }

  void Advance(int64_t ticks) {
    ticks_.fetch_add(ticks, std::memory_order_relaxed);
  }

  // Sets how far each call to ReadCpuTicks() advances the ticks, such as to
  // give each read of the ticks a cost.
  void SetAutoAdvance(int64_t ticks) {
    auto_advance_.store(ticks, std::memory_order_relaxed);
  }

 private:
  const double ticks_per_second_;
  std::atomic<int> core_class_ = 0;
  std::atomic<int64_t> ticks_ = 0;
  std::atomic<int64_t> auto_advance_ = 0;
};

}  // namespace gb

#endif  // GB_PROFILE_FAKE_PROFILER_CPU_H_
