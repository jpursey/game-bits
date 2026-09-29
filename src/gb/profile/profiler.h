// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_PROFILER_H_
#define GB_PROFILE_PROFILER_H_

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string_view>

#include "absl/log/check.h"
#include "absl/time/time.h"
#include "gb/profile/fake_ticks.h"
#include "gb/profile/profile_point.h"

namespace gb {

//==============================================================================
// Profiler
//==============================================================================

// Records the profile points reached on the thread that created it.
//
// While a Profiler exists, it records every point on its thread: the time of
// each scope and call less the time of the timed points inside it (its self
// time), the total of each counter, and the latest of each value. Points on
// other threads, or on a thread with no Profiler, are ignored, and cost about a
// nanosecond.
//
// Time is read from the CPU's timestamp counter, which must be invariant (true
// of any x86-64 CPU in the last decade), unless a FakeTicks is given in the
// options.
//
// There can be at most one Profiler per thread, and it must be destroyed on the
// thread that created it, outside any timed point. This class is
// thread-compatible, and only its thread may call it.
class Profiler final {
 public:
  struct Options {
    // Replaces the CPU's timestamp counter, for tests. It must outlive the
    // Profiler.
    FakeTicks* fake_ticks = nullptr;
  };

  // Makes this the Profiler for points on the calling thread.
  //
  // Without a FakeTicks, the first Profiler in a program takes about a
  // millisecond to measure how fast the timestamp counter runs.
  Profiler();
  explicit Profiler(Options options);
  Profiler(const Profiler&) = delete;
  Profiler& operator=(const Profiler&) = delete;
  ~Profiler();

  //----------------------------------------------------------------------------
  // For tests
  //
  // Each of these reads the point named `name`, returning zero if there is no
  // such point, and CHECK-fails if it has the wrong kind.
  //----------------------------------------------------------------------------

  // Returns how many times a scope or call was timed, or a counter's total.
  int64_t GetCount(std::string_view name) const;

  // Returns the total self time of a scope or call.
  absl::Duration GetSelfTime(std::string_view name) const;

  // Returns the latest value set for a value.
  int64_t GetValue(std::string_view name) const;

  // Clears everything recorded. CHECK-fails inside a timed point.
  void Reset();

 private:
  friend class ProfilePoint;
  friend class ProfileTimer;

  // A timed point in progress, kept by whatever times it. Timings running on
  // the thread form a stack, linked through `parent`.
  struct Timing {
    Timing* parent = nullptr;
    int64_t start_ticks = 0;

    // The elapsed ticks of the timings inside this one.
    int64_t child_ticks = 0;
  };

  // What is recorded for each point.
  struct Slot {
    // Calls for a timed point, or the total for a counter.
    int64_t count = 0;
    int64_t self_ticks = 0;
    int64_t value = 0;
  };

  // Starts `timing` inside the innermost timing on this thread.
  void StartTiming(Timing& timing) {
    timing.parent = top_;
    top_ = &timing;
    timing.start_ticks = ReadTicks();
  }

  // Ends `timing`, which must be the innermost, and returns its self ticks.
  int64_t EndTiming(Timing& timing) {
    const int64_t elapsed_ticks = ReadTicks() - timing.start_ticks;
    CHECK(top_ == &timing)
        << "A timed point must end on the thread and fiber it started on, "
           "after the timed points inside it";
    top_ = timing.parent;
    if (top_ != nullptr) {
      top_->child_ticks += elapsed_ticks;
    }
    return elapsed_ticks - timing.child_ticks;
  }

  // Record into the slot at `index` (a ProfilePoint's index).
  void AddTime(int index, int64_t self_ticks) {
    Slot& slot = slots_[index];
    ++slot.count;
    slot.self_ticks += self_ticks;
  }
  void AddCount(int index, int64_t count) { slots_[index].count += count; }
  void SetValue(int index, int64_t value) { slots_[index].value = value; }

  // Returns the slot for `name`, or null if there is no such point.
  // CHECK-fails if the point's kind isn't one of `kinds`.
  const Slot* FindSlot(std::string_view name,
                       std::initializer_list<ProfilePoint::Kind> kinds) const;

  int64_t ReadTicks() const {
    if (fake_ticks_ != nullptr) {
      return fake_ticks_->Now();
    }
    return static_cast<int64_t>(__rdtsc());
  }

  absl::Duration TicksToDuration(int64_t ticks) const;

  static inline constinit thread_local Profiler* s_current = nullptr;

  FakeTicks* const fake_ticks_;
  const double ticks_per_second_;
  const std::unique_ptr<Slot[]> slots_;

  // The innermost timing on this thread, or null.
  Timing* top_ = nullptr;
};

}  // namespace gb

#endif  // GB_PROFILE_PROFILER_H_
