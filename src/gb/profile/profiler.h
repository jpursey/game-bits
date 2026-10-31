// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_PROFILER_H_
#define GB_PROFILE_PROFILER_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/optimization.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gb/profile/fake_profiler_cpu.h"
#include "gb/profile/profile_point.h"
#include "gb/thread/cpu_ticks.h"

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
// of any x86-64 CPU in the last decade), unless a FakeProfilerCpu is given in
// the options.
//
// The counter counts time, not work, so the same code takes longer on a slower
// core. On a CPU with more than one kind of core (such as Intel's performance
// and efficiency cores), each frame records the class of core it ran on, where
// a higher class is faster, and the report splits frames by it.
//
// There can be at most one Profiler per thread, and it must be destroyed on the
// thread that created it, outside any timed point. This class is
// thread-compatible, and only its thread may call it.
class Profiler final {
 public:
  struct Options {
    // Replaces the CPU's timestamp counter, for tests. It must outlive the
    // Profiler.
    FakeProfilerCpu* fake_cpu = nullptr;

    // The profiler's own budget per frame is the larger of these: a fixed
    // time, and a fraction of the average frame. With neither set, there is
    // no budget. See FrameSummary.
    absl::Duration budget_per_frame;
    double budget_fraction = 0;

    // Called with a frame's breakdown (see GetReport()) when a frame takes
    // longer than `slow_frame`. It is called on the Profiler's thread, just
    // after the frame ends, so any points it reaches are outside the frame.
    absl::Duration slow_frame = absl::InfiniteDuration();
    absl::AnyInvocable<void(std::string_view report)> on_slow_frame;
  };

  // Makes this the Profiler for points on the calling thread.
  //
  // This measures what a timed point costs, which takes a few microseconds.
  // Without a FakeProfilerCpu, the first Profiler in a program may also take
  // about a millisecond, for GetCpuTicksPerSecond() (see
  // gb/thread/cpu_ticks.h).
  Profiler();
  explicit Profiler(Options options);
  Profiler(const Profiler&) = delete;
  Profiler& operator=(const Profiler&) = delete;
  ~Profiler();

  // Returns what one timed point costs, as measured when the Profiler was
  // created: the time a frame, scope, or call adds to the timed point around
  // it. This leaves out finding the thread's Profiler, about two nanoseconds.
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
    // only a few adds, and for a value a read of the timestamp counter.
    absl::Duration profiler_cost;

    // The budget for profiler_cost: the larger of the options'
    // budget_per_frame and budget_fraction of the average frame, or infinite
    // if neither is set. The profiler is within its budget if profiler_cost is
    // no more than this.
    absl::Duration budget;
  };
  FrameSummary GetFrameSummary() const;

  //----------------------------------------------------------------------------
  // Report
  //----------------------------------------------------------------------------

  // Returns the profile as plain text: the values, the frame summary, the tick
  // rate, the frames by class of core, one line for each point with its count
  // and self time (in total, per frame, per call, and as a share of the
  // frames' time), and the slowest frame's breakdown. Only points this Profiler
  // has recorded are included (a counter whose total is zero counts as not
  // recorded). Points are grouped by kind and sorted by name, so two reports of
  // the same program diff cleanly.
  //
  // The tick rate converts ticks to time. A rate that changes between runs
  // makes every time wrong by the same factor.
  //
  // A frame's class of core is the one it started and ended on, or "mixed" if
  // those differ. The report warns when frames ran on more than one class, as
  // their times aren't comparable.
  //
  // A frame's breakdown is read to find what made it slow, so it has the
  // frame's class of core and self time by kind (frames, scopes, and calls,
  // which add up to the frame's time), then the frame's count and self time of
  // each point, slowest first.
  //
  // A program can put its own header (such as its build and the date) before
  // the report.
  std::string GetReport() const;

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

  // Frames are counted for each class of core up to kCoreClasses (higher
  // classes count as the highest), and then for frames that started and ended
  // on different classes, as kMixedCores.
  static constexpr int kCoreClasses = 8;
  static constexpr int kMixedCores = kCoreClasses;

  // The frames that ran on one class of core (or kMixedCores).
  struct CoreFrames {
    int64_t frames = 0;
    int64_t ticks = 0;
    int64_t max_ticks = 0;
  };

  // A timed point in progress, kept by whatever times it. Timings running on
  // the thread form a stack, linked through `parent`.
  struct Timing {
    Timing* parent = nullptr;
    int64_t start_ticks = 0;

    // The elapsed ticks of the timings inside this one.
    int64_t child_ticks = 0;
  };

  // An int64_t that only one thread writes, and any thread may read. Reads and
  // writes are relaxed, and Add() is a load and a store rather than an atomic
  // add, so each costs what a plain int64_t's does. A read on another thread
  // sees each write whole, but in no particular order with other writes.
  class SingleWriterInt64 {
   public:
    SingleWriterInt64() = default;
    SingleWriterInt64& operator=(const SingleWriterInt64& other) {
      Set(other.Get());
      return *this;
    }

    int64_t Get() const { return value_.load(std::memory_order_relaxed); }
    void Set(int64_t value) { value_.store(value, std::memory_order_relaxed); }
    void Add(int64_t delta) { Set(Get() + delta); }

   private:
    std::atomic<int64_t> value_ = 0;
  };

  // What is recorded for each point. Only the Profiler's thread writes it.
  struct Slot {
    // Calls for a timed point, the total for a counter, or the times a value
    // was set. A point with a count of zero hasn't been recorded.
    SingleWriterInt64 count;
    SingleWriterInt64 self_ticks;

    // The same, for only the frame numbered `frame` (see frame_). They are
    // reset by the first add in a new frame, so nothing needs clearing when a
    // frame starts. They come before `value`, so the fields a timed point
    // writes are together.
    SingleWriterInt64 frame;
    SingleWriterInt64 frame_count;
    SingleWriterInt64 frame_self_ticks;

    // A value point's latest value, and when it was set.
    SingleWriterInt64 value;
    SingleWriterInt64 value_ticks;
  };

  // A point's totals, as the report and the test readers see them.
  struct PointTotals {
    int64_t count = 0;
    int64_t self_ticks = 0;
    int64_t value = 0;
  };

  // A frame's time, and the part each point had in it.
  struct FrameBreakdown {
    struct Point {
      int index;
      int64_t count;
      int64_t self_ticks;
    };

    int64_t ticks = 0;

    // The class of core the frame ran on, or kMixedCores.
    int core = 0;

    // The timed points ended in the frame, including its own.
    int64_t timed_points = 0;

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

  // Returns the current frame's time, `ticks`, the class of core it ran on,
  // `core`, and the part each point had in it.
  FrameBreakdown CaptureFrame(int64_t ticks, int core) const;

  // Returns the slot at `index` (a ProfilePoint's index), with its frame part
  // reset if it was for another frame.
  Slot& GetFrameSlot(int index) {
    Slot& slot = slots_[index];
    const int64_t frame = frame_->Get();
    if (slot.frame.Get() != frame) {
      slot.frame.Set(frame);
      slot.frame_count.Set(0);
      slot.frame_self_ticks.Set(0);
    }
    return slot;
  }

  void AddTime(int index, int64_t self_ticks) {
    Slot& slot = GetFrameSlot(index);
    slot.count.Add(1);
    slot.self_ticks.Add(self_ticks);
    slot.frame_count.Add(1);
    slot.frame_self_ticks.Add(self_ticks);
  }
  void AddCount(int index, int64_t count) {
    Slot& slot = GetFrameSlot(index);
    slot.count.Add(count);
    slot.frame_count.Add(count);
  }
  void SetValue(int index, int64_t value) {
    Slot& slot = slots_[index];
    slot.count.Add(1);
    slot.value.Set(value);
    slot.value_ticks.Set(ReadTicks());
  }

  // Returns the totals of every registered point, by index.
  std::vector<PointTotals> GatherPointTotals() const;

  // Report sections, each appended to `report`. AppendValues() and
  // AppendPoints() take every registered point with its totals, sorted for the
  // report, and AppendFrame() takes the registered points in index order.
  using ReportPoint = std::pair<ProfilePoint, PointTotals>;
  void AppendValues(std::string& report,
                    absl::Span<const ReportPoint> points) const;
  void AppendFrameSummary(std::string& report) const;
  void AppendCoreFrames(std::string& report) const;
  void AppendPoints(std::string& report,
                    absl::Span<const ReportPoint> points) const;
  void AppendFrame(std::string& report, std::string_view title,
                   const FrameBreakdown& frame,
                   absl::Span<const ProfilePoint> registered) const;

  // Returns the index of the point named `name`, or -1 if there is no such
  // point. CHECK-fails if the point's kind isn't one of `kinds`.
  int FindIndex(std::string_view name,
                absl::Span<const ProfilePoint::Kind> kinds) const;

  // As FindIndex(), returning the point's totals, which are zero if there is no
  // such point.
  PointTotals FindPointTotals(std::string_view name,
                              absl::Span<const ProfilePoint::Kind> kinds) const;

  // As FindIndex(), returning the point's part in the slowest frame, or null
  // if it had none.
  const FrameBreakdown::Point* FindSlowestFramePoint(
      std::string_view name, absl::Span<const ProfilePoint::Kind> kinds) const;

  // Returns the frame time that `fraction` of frames are at or under.
  int64_t GetFramePercentileTicks(double fraction) const;

  int64_t ReadTicks() const {
    if (fake_cpu_ != nullptr) {
      return fake_cpu_->ReadCpuTicks();
    }
    return ReadCpuTicks();
  }

  // Returns the class of core the thread is on, below kCoreClasses.
  int ReadCoreClass() const;

  // Formats a class of core, or kMixedCores, such as "class 1" or "mixed".
  static std::string FormatCore(int core);

  absl::Duration TicksToDuration(double ticks) const;

  // Returns the calling thread's Profiler, or null.
  //
  // This reads s_current through a call that is never inlined, so code on a
  // fiber that resumed on another thread reads the Profiler of the thread it is
  // on now. Read inline, the compiler may reuse the thread local's address from
  // before the switch, and so read the old thread's Profiler.
  static Profiler* GetCurrent();

  static inline constinit thread_local Profiler* s_current = nullptr;

  // What every timed point uses comes first, to share a cache line.
  FakeProfilerCpu* const fake_cpu_;
  const std::unique_ptr<Slot[]> slots_;

  // The number of the frame that points are recorded in: this Profiler's
  // current_frame_.
  const SingleWriterInt64* const frame_;

  // The innermost timing on this thread, or null.
  Timing* top_ = nullptr;

  // The timed points ended in the frame in progress, including its own.
  int64_t current_frame_timed_points_ = 0;

  // The number of the frame in progress, counting from 1, or 0 between
  // frames. Points between frames are recorded against frame 0, which is
  // never captured. It is read by every point, so it has a cache line of its
  // own, apart from what every point writes.
  struct alignas(ABSL_CACHELINE_SIZE) FrameNumber {
    SingleWriterInt64 number;
  };
  FrameNumber current_frame_;

  // The class of core the frame in progress started on.
  int current_frame_start_core_ = 0;

  const double ticks_per_second_;

  const absl::Duration budget_per_frame_;
  const double budget_fraction_;
  const int64_t slow_frame_ticks_;
  absl::AnyInvocable<void(std::string_view report)> on_slow_frame_;
  double point_cost_ticks_ = 0;

  // The timed points ended in every frame.
  int64_t frame_timed_points_ = 0;

  int64_t frames_ = 0;
  int64_t total_frame_ticks_ = 0;
  std::array<int64_t, kFrameBuckets> frame_buckets_ = {};
  std::array<CoreFrames, kCoreClasses + 1> core_frames_ = {};
  FrameBreakdown slowest_frame_;
};

}  // namespace gb

#endif  // GB_PROFILE_PROFILER_H_
