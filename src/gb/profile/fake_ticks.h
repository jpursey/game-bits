// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_FAKE_TICKS_H_
#define GB_PROFILE_FAKE_TICKS_H_

#include <cstdint>

namespace gb {

//==============================================================================
// FakeTicks
//==============================================================================

// A timestamp source that a test controls, standing in for the CPU's timestamp
// counter when passed to a Profiler.
//
// The ticks start at zero and only change when the test advances them, or
// when they are read, if an auto advance is set.
//
// It also stands in for what the CPU reports about the counter: its rate, and
// the class of core it is read on.
class FakeTicks {
 public:
  // Every tick is a nanosecond by default.
  explicit FakeTicks(int64_t ticks_per_second = 1'000'000'000)
      : ticks_per_second_(ticks_per_second),
        cpu_ticks_per_second_(ticks_per_second) {}
  FakeTicks(const FakeTicks&) = delete;
  FakeTicks& operator=(const FakeTicks&) = delete;
  ~FakeTicks() = default;

  int64_t GetTicksPerSecond() const { return ticks_per_second_; }

  // The rate the CPU reports for the counter, or 0 if it reports none. This is
  // the counter's own rate unless set.
  int64_t GetCpuTicksPerSecond() const { return cpu_ticks_per_second_; }
  void SetCpuTicksPerSecond(int64_t ticks_per_second) {
    cpu_ticks_per_second_ = ticks_per_second;
  }

  // The class of the core the counter is read on (see Profiler), 0 unless set.
  int GetCoreClass() const { return core_class_; }
  void SetCoreClass(int core_class) { core_class_ = core_class; }

  // Advances the ticks by the auto advance, and returns them.
  int64_t Now() {
    ticks_ += auto_advance_;
    return ticks_;
  }

  void Advance(int64_t ticks) { ticks_ += ticks; }

  // Sets how far each call to Now() advances the ticks, such as to give each
  // read of the ticks a cost.
  void SetAutoAdvance(int64_t ticks) { auto_advance_ = ticks; }

 private:
  const int64_t ticks_per_second_;
  int64_t cpu_ticks_per_second_;
  int core_class_ = 0;
  int64_t ticks_ = 0;
  int64_t auto_advance_ = 0;
};

}  // namespace gb

#endif  // GB_PROFILE_FAKE_TICKS_H_
