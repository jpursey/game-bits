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

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "absl/log/check.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
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
// Frames (see ProfileFrame) are timed points that also record a histogram of
// frame times, and the breakdown by point of the slowest frame so far. Points
// outside any frame are included in the totals, but in no frame's breakdown.
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

    // The profiler's own budget per frame is the larger of these: a fixed
    // time, and a fraction of the average frame. With neither set, there is
    // no budget. See FrameSummary.
    absl::Duration budget_per_frame;
    double budget_fraction = 0;
  };

  // Makes this the Profiler for points on the calling thread.
  //
  // This measures what a timed point costs, which takes a few microseconds.
  // Without a FakeTicks, the first Profiler in a program also takes about a
  // millisecond to measure how fast the timestamp counter runs.
  Profiler();
  explicit Profiler(Options options);
  Profiler(const Profiler&) = delete;
  Profiler& operator=(const Profiler&) = delete;
  ~Profiler();

  // Returns what one timed point costs, as measured when the Profiler was
  // created: the time a frame, scope, or call adds to the timed point around
  // it. This leaves out finding the thread's Profiler, about a nanosecond.
  absl::Duration GetPointCost() const;

  //----------------------------------------------------------------------------
  // Frames
  //----------------------------------------------------------------------------

  // Statistics over every frame recorded, whatever its point's name. The
  // percentiles come from a histogram, and are within 1/16th of the true
  // value. With no frames, everything is zero.
  struct FrameSummary {
    int64_t frames = 0;
    absl::Duration total;
    absl::Duration average;
    absl::Duration p50;
    absl::Duration p90;
    absl::Duration p99;
    absl::Duration max;

    // The profiler's own average cost per frame: the timed points in frames,
    // times GetPointCost(). Counters and values aren't included, as they cost
    // only a few adds.
    absl::Duration profiler_cost;

    // The budget for profiler_cost: the larger of the options'
    // budget_per_frame and budget_fraction of the average frame, or infinite
    // if neither is set. The profiler is within its budget if profiler_cost is
    // no more than this.
    absl::Duration budget;
  };
  FrameSummary GetFrameSummary() const;

  //----------------------------------------------------------------------------
  // For tests
  //
  // Each of these reads the point named `name`, returning zero if there is no
  // such point, and CHECK-fails if it has the wrong kind.
  //----------------------------------------------------------------------------

  // Returns how many times a frame, scope, or call was timed, or a counter's
  // total.
  int64_t GetCount(std::string_view name) const;

  // Returns the total self time of a frame, scope, or call.
  absl::Duration GetSelfTime(std::string_view name) const;

  // Returns the latest value set for a value.
  int64_t GetValue(std::string_view name) const;

  // As GetCount() and GetSelfTime(), for only the slowest frame so far.
  int64_t GetSlowestFrameCount(std::string_view name) const;
  absl::Duration GetSlowestFrameSelfTime(std::string_view name) const;

  // Clears everything recorded. CHECK-fails inside a timed point.
  void Reset();

 private:
  friend class ProfilePoint;
  friend class ProfileTimer;
  template <ProfileName kName>
  friend class ProfileFrame;

  // Frame times are counted in 8 buckets for each power of two. The highest
  // bit a positive int64_t can have is bit 62, which lands in the last of
  // these (see GetFrameBucket in profiler.cc).
  static constexpr int kFrameBuckets = 61 * 8;

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

    // The same, for only the frame numbered `frame` (see current_frame_). They
    // are reset by the first add in a new frame, so nothing needs clearing
    // when a frame starts.
    int64_t frame = 0;
    int64_t frame_count = 0;
    int64_t frame_self_ticks = 0;
  };

  // A frame's time, and the part each point had in it.
  struct FrameBreakdown {
    struct Point {
      int index;
      int64_t count;
      int64_t self_ticks;
    };

    int64_t ticks = 0;
    std::vector<Point> points;
  };

  // Starts `timing` inside the innermost timing on this thread.
  void StartTiming(Timing& timing) {
    timing.parent = top_;
    top_ = &timing;
    timing.start_ticks = ReadTicks();
  }

  // Ends `timing`, which must be the innermost, and adds its self time to the
  // point at `index`. Returns its elapsed ticks.
  int64_t EndTiming(int index, Timing& timing) {
    const int64_t elapsed_ticks = ReadTicks() - timing.start_ticks;
    CHECK(top_ == &timing)
        << "A timed point must end on the thread and fiber it started on, "
           "after the timed points inside it";
    top_ = timing.parent;
    if (top_ != nullptr) {
      top_->child_ticks += elapsed_ticks;
    }
    AddTime(index, elapsed_ticks - timing.child_ticks);
    ++current_frame_timed_points_;
    return elapsed_ticks;
  }

  // Returns the ticks a timed point adds to the timed point around it, the
  // least of several measurements, so a thread switch doesn't skew it.
  double MeasurePointCost();

  // Frames are timings that also record the frame as a whole. Frames can't
  // nest.
  void StartFrame(Timing& timing);
  void EndFrame(int index, Timing& timing);

  // Returns the current frame's time, `ticks`, and the part each point had in
  // it.
  FrameBreakdown CaptureFrame(int64_t ticks) const;

  // Returns the slot at `index` (a ProfilePoint's index), with its frame part
  // reset if it was for another frame.
  Slot& GetFrameSlot(int index) {
    Slot& slot = slots_[index];
    if (slot.frame != current_frame_) {
      slot.frame = current_frame_;
      slot.frame_count = 0;
      slot.frame_self_ticks = 0;
    }
    return slot;
  }

  void AddTime(int index, int64_t self_ticks) {
    Slot& slot = GetFrameSlot(index);
    ++slot.count;
    slot.self_ticks += self_ticks;
    ++slot.frame_count;
    slot.frame_self_ticks += self_ticks;
  }
  void AddCount(int index, int64_t count) {
    Slot& slot = GetFrameSlot(index);
    slot.count += count;
    slot.frame_count += count;
  }
  void SetValue(int index, int64_t value) { slots_[index].value = value; }

  // Returns the index of the point named `name`, or -1 if there is no such
  // point. CHECK-fails if the point's kind isn't one of `kinds`.
  int FindIndex(std::string_view name,
                absl::Span<const ProfilePoint::Kind> kinds) const;

  // As FindIndex(), returning the point's slot, or null.
  const Slot* FindSlot(std::string_view name,
                       absl::Span<const ProfilePoint::Kind> kinds) const;

  // As FindIndex(), returning the point's part in the slowest frame, or null
  // if it had none.
  const FrameBreakdown::Point* FindSlowestFramePoint(
      std::string_view name, absl::Span<const ProfilePoint::Kind> kinds) const;

  // Returns the frame time that `fraction` of frames are at or under.
  int64_t GetFramePercentileTicks(double fraction) const;

  int64_t ReadTicks() const {
    if (fake_ticks_ != nullptr) {
      return fake_ticks_->Now();
    }
    return static_cast<int64_t>(__rdtsc());
  }

  absl::Duration TicksToDuration(double ticks) const;

  static inline constinit thread_local Profiler* s_current = nullptr;

  // What every timed point uses comes first, to share a cache line.
  FakeTicks* const fake_ticks_;
  const std::unique_ptr<Slot[]> slots_;

  // The innermost timing on this thread, or null.
  Timing* top_ = nullptr;

  // The number of the frame in progress, counting from 1, or 0 between
  // frames. Points between frames are recorded against frame 0, which is
  // never captured.
  int64_t current_frame_ = 0;

  // The timed points ended in the frame in progress, including its own.
  int64_t current_frame_timed_points_ = 0;

  const double ticks_per_second_;
  const absl::Duration budget_per_frame_;
  const double budget_fraction_;
  double point_cost_ticks_ = 0;

  // The timed points ended in every frame.
  int64_t frame_timed_points_ = 0;

  int64_t frames_ = 0;
  int64_t total_frame_ticks_ = 0;
  std::array<int64_t, kFrameBuckets> frame_buckets_ = {};
  FrameBreakdown slowest_frame_;
};

}  // namespace gb

#endif  // GB_PROFILE_PROFILER_H_
