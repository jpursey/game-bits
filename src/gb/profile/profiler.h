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
#include "absl/synchronization/mutex.h"
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
// To profile several threads together, the thread that runs the main loop has
// a Profiler as above (the main Profiler), and each other thread has a Profiler
// made with Options::main (a joined Profiler). A joined Profiler records its
// thread's points as any Profiler does, and adds them to the main Profiler's
// report and test readers, even once it is destroyed. It can be made and
// destroyed at any time, but the main Profiler must outlive it. The main
// Profiler reads what the other threads record without stopping them, so a
// point that ends while it reads may be left out until its next read.
//
// The main Profiler keeps what each thread recorded by the thread's name (see
// Options::name). A Profiler that joins with the name of one that was
// destroyed carries on from it, so a program that keeps making threads with
// the same names doesn't keep adding to what the main Profiler holds.
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
// A timed point on a fiber, such as a job, can span a switch to another fiber
// (see gb/thread/fiber.h). Its timer pauses while the fiber is suspended, and
// carries on when it resumes, on any thread, so its time is the time its fiber
// ran, on every thread. Its self time and its part in a frame are recorded in
// the Profiler of the thread it ends on, while each joined Profiler's busy time
// has the part that ran on its thread. Timed points that resume on a thread
// with no Profiler record nothing, even if their fiber moves on to a thread
// with one. A frame can't span a switch: switching fibers in a frame
// CHECK-fails. The first Profiler sets the program's fiber switch callbacks, so
// no other library can set its own.
//
// The time from a fiber's suspending to its resuming is the wait time of the
// innermost timed point open on it, as self time is charged to the innermost
// point. It is recorded as the fiber resumes, in the Profiler of the thread it
// resumes on, so it counts in the frame in progress then. It is measured from
// one thread's timestamp counter to another's, which agree for an invariant
// counter (and for FakeProfilerCpus a test keeps in step).
//
// There can be at most one Profiler per thread, and it must be destroyed on the
// thread that created it, outside any timed point. This class is
// thread-compatible, and only its thread may call it.
class Profiler final {
 public:
  struct Options {
    // Replaces the CPU's timestamp counter, for tests. It must outlive the
    // Profiler. A Profiler with a `main` and no `fake_cpu` uses its main
    // Profiler's, if it has one. Making a Profiler reads its ticks many times
    // (see GetPointCost()), so with an auto advance set, making one that
    // shares a FakeProfilerCpu advances the ticks every sharer sees.
    FakeProfilerCpu* fake_cpu = nullptr;

    // Another Profiler to add this one's points to, normally the one on the
    // thread that runs the main loop, or null. That Profiler must outlive this
    // one, and must not have a `main` of its own. With a `main`, this Profiler
    // can't time frames, as its points count in the main Profiler's frames,
    // and its tick rate must match the main Profiler's (so a FakeProfilerCpu
    // of its own needs one on the main Profiler, at the same rate).
    Profiler* main = nullptr;

    // The thread's name in the main Profiler's threads table (see
    // GetReport()). If this is empty, a main Profiler's thread is "main". A
    // joined Profiler's thread has its own name (see GetThreadName() in
    // gb/thread/thread.h), as it is when the Profiler is made. A thread with
    // no name, including any thread CreateThread() didn't make, is "thread N",
    // for the lowest N no other existing Profiler joined to the same main
    // Profiler has.
    std::string_view name;

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
  // A main Profiler's counts, self times, and wait times are the sum over
  // every thread, and each value is the one set most recently on any thread.
  //
  // Once any point has waited, the points table also has each point's wait
  // time in total and per call, and a frame's breakdown, once a point in it
  // has waited, each point's wait in the frame. A point that has waited but
  // not yet ended counts as recorded.
  //
  // The tick rate converts ticks to time. A rate that changes between runs
  // makes every time wrong by the same factor.
  //
  // A frame's class of core is the one it started and ended on, or "mixed" if
  // those differ. The report warns when frames ran on more than one class, as
  // their times aren't comparable.
  //
  // A frame's breakdown is read to find what made it slow, so it has the
  // frame's class of core and self time by kind (frames, scopes, calls, and,
  // in a frame that ran any, jobs, which add up to the frame's time), then the
  // frame's count and self time of each point, slowest first.
  //
  // Once a Profiler has joined, a main Profiler's report also has a table of
  // threads, with a row for its own thread and one for each name it has had
  // joined Profilers under, sorted by name. Each has the thread's busy time
  // per frame on average and at most, the average as a share of the average
  // frame, and the share of its busy time on each class of core. A joined
  // thread's busy time is the time its outermost timed points ran on it,
  // counted when each ends, or its fiber suspends, during a frame, by the
  // class of core it was on then. The main thread's busy time is its frames'
  // time, by the class of core each frame ran on.
  //
  // Once a Profiler has joined a main Profiler, the main Profiler's frame
  // breakdowns include the points that ended on every joined Profiler's thread
  // while the frame was in progress, including on any destroyed by then. The
  // self time by kind has the main thread's beside the other threads', which
  // is CPU time spent alongside the frame, so only the main thread's adds up
  // to the frame's time.
  //
  // A program can put its own header (such as its build and the date) before
  // the report.
  std::string GetReport() const;

  //----------------------------------------------------------------------------
  // For tests
  //
  // Each of these reads the point named `name`, returning zero if there is no
  // such point, and CHECK-fails if it has the wrong kind. A main Profiler reads
  // what every thread recorded, as its report does, and a joined Profiler reads
  // only its own thread.
  //----------------------------------------------------------------------------

  // Returns how many times a frame, scope, or call was timed, or a counter's
  // total.
  int64_t GetCount(std::string_view name) const;

  // Returns the total self time of a frame, scope, or call.
  absl::Duration GetSelfTime(std::string_view name) const;

  // Returns the total wait time of a frame, scope, or call (see the class
  // comment).
  absl::Duration GetWaitTime(std::string_view name) const;

  // Returns the latest value set for a value.
  int64_t GetValue(std::string_view name) const;

  // As GetCount() and GetSelfTime(), for only the slowest frame so far.
  int64_t GetSlowestFrameCount(std::string_view name) const;
  absl::Duration GetSlowestFrameSelfTime(std::string_view name) const;

  // Clears everything recorded, on every thread for a main Profiler.
  // CHECK-fails inside a timed point, or on a joined Profiler. Timed points
  // open on a suspended fiber record all their time once they end, and their
  // whole wait once it resumes.
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

  // Ticks by class of core.
  using CoreTicks = std::array<int64_t, kCoreClasses>;

  // A timed point in progress, kept by whatever times it. Timings running on
  // the thread form a stack, linked through `parent`, which holds only the
  // running fiber's timings (see SuspendTimings()).
  struct Timing {
    // The Profiler the timing ends in, or null if it records nothing. While its
    // fiber is suspended, the Profiler it suspended from.
    Profiler* profiler = nullptr;

    // The point's index.
    int index = 0;

    Timing* parent = nullptr;

    // While its fiber is suspended, the ticks it has run so far instead.
    int64_t start_ticks = 0;

    // The elapsed ticks of the timings inside this one.
    int64_t child_ticks = 0;

    // For the outermost timing of a fiber, the ticks it ran before the fiber
    // last resumed, which are already in the busy time of the threads they ran
    // on.
    int64_t prior_run_ticks = 0;

    // For the innermost timing while its fiber is suspended, the ticks when it
    // suspended.
    int64_t suspend_ticks = 0;
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

    // As Get() and Set(), and a read with GetAcquire() that sees a value
    // written by SetRelease() also sees the writes before it. On x86-64 these
    // too are plain moves.
    int64_t GetAcquire() const {
      return value_.load(std::memory_order_acquire);
    }
    void SetRelease(int64_t value) {
      value_.store(value, std::memory_order_release);
    }

   private:
    std::atomic<int64_t> value_ = 0;
  };

  // What is recorded for each point. Only the Profiler's thread writes it.
  struct Slot {
    // Calls for a timed point, the total for a counter, or the times a value
    // was set. A point with a count of zero hasn't been recorded.
    SingleWriterInt64 count;
    SingleWriterInt64 self_ticks;

    // A timed point's wait (see GetWaitTime()), which only a fiber resuming
    // adds to.
    SingleWriterInt64 wait_ticks;

    // The same, for only the frame numbered `frame` (see frame_). They are
    // reset by the first add in a new frame, so nothing needs clearing when a
    // frame starts, and `frame` is set after them (with SetRelease()), so a
    // reader that sees the new frame number sees them reset. They come before
    // `value`, so the fields a timed point writes are together.
    SingleWriterInt64 frame;
    SingleWriterInt64 frame_count;
    SingleWriterInt64 frame_self_ticks;
    SingleWriterInt64 frame_wait_ticks;

    // A value point's latest value, and when it was set.
    SingleWriterInt64 value;
    SingleWriterInt64 value_ticks;
  };

  // A point's totals, as the report and the test readers see them.
  struct PointTotals {
    // Returns whether the point has recorded anything. A point that has
    // waited, but not yet ended, has.
    bool IsRecorded() const { return count > 0 || wait_ticks > 0; }

    int64_t count = 0;
    int64_t self_ticks = 0;
    int64_t value = 0;
    int64_t value_ticks = 0;
    int64_t wait_ticks = 0;
  };

  // The Profilers joined to this one under a name: at most one that exists,
  // and every one that was destroyed.
  struct JoinedThread {
    // The one that exists, or null.
    Profiler* profiler = nullptr;

    // The name, which is "thread N" if `numbered`, for threads with no name.
    std::string name;
    bool numbered = false;

    // While `profiler` exists, its totals when this was last reset, which its
    // slots still hold, by point index.
    std::vector<PointTotals> baseline;

    // What the destroyed ones recorded since this was last reset, by point
    // index.
    std::vector<PointTotals> folded;

    // The busy time in frames since this was last reset, by class of core,
    // and in the busiest frame, gathered at each frame's end.
    CoreTicks busy_core_ticks = {};
    int64_t max_frame_busy_ticks = 0;

    // `profiler`'s busy time (see BusyTime) when it was last gathered, by
    // class of core.
    CoreTicks gathered_busy_ticks = {};

    // The busy time the destroyed ones added since the last frame's end,
    // which counts in the frame in progress, or the next one.
    int64_t left_busy_ticks = 0;
  };

  // A joined Profiler's busy time in every frame (see GetReport()), by the
  // class of core each outermost point ended or suspended on. Only its thread
  // writes it, and the main Profiler gathers what it has added at each frame's
  // end. It has a cache line of its own, as both threads use it.
  struct alignas(ABSL_CACHELINE_SIZE) BusyTime {
    std::array<SingleWriterInt64, kCoreClasses> core_ticks;
  };

  // A frame's time, and the part each point had in it.
  struct FrameBreakdown {
    // A point's part in the frame, on every thread. An index of -1 is a point
    // with no part.
    struct Point {
      int index = -1;
      int64_t count = 0;
      int64_t self_ticks = 0;

      // The part of `self_ticks` on joined Profilers' threads.
      int64_t joined_self_ticks = 0;

      int64_t wait_ticks = 0;
    };

    int64_t ticks = 0;

    // The class of core the frame ran on, or kMixedCores.
    int core = 0;

    // The timed points ended in the frame on the main thread, including its
    // own.
    int64_t timed_points = 0;

    // Whether any Profiler had joined by the frame's end, so its breakdown
    // shows the other threads' time.
    bool has_joined = false;

    std::vector<Point> points;
  };

  // Starts `timing` for the point at `index`, inside the innermost timing on
  // this thread.
  void StartTiming(int index, Timing& timing) {
    timing.profiler = this;
    timing.index = index;
    timing.parent = top_;
    top_ = &timing;
    timing.start_ticks = ReadTicks();
  }

  // Ends `timing`, which must be the innermost, and adds its self time to its
  // point. Returns its elapsed ticks.
  int64_t EndTiming(Timing& timing) {
    const int64_t elapsed_ticks = ReadTicks() - timing.start_ticks;
    CHECK(top_ == &timing)
        << "A timed point must end on the fiber it started on, after the "
           "timed points inside it";
    top_ = timing.parent;
    if (top_ != nullptr) {
      top_->child_ticks += elapsed_ticks;
    } else if (main_ != nullptr) {
      AddBusyTime(elapsed_ticks - timing.prior_run_ticks);
    }
    AddTime(timing.index, elapsed_ticks - timing.child_ticks);
    ++current_frame_timed_points_;
    return elapsed_ticks;
  }

  // The fiber switch callbacks (see gb/thread/fiber.h). When a fiber
  // suspends, SuspendTimings() takes its open timings off the thread's stack,
  // with the ticks each has run so far, adds the outermost's run on this
  // thread to its busy time, notes the suspend ticks in the innermost, and
  // returns it, or null if there are none. When it resumes, ResumeTimings()
  // adds the wait to the innermost's point and makes them the stack of the
  // thread it resumed on, ending in that thread's Profiler, or in none if it
  // has none.
  static void* SuspendTimings();
  static void ResumeTimings(void* state);

  // Returns the ticks a timed point adds to the timed point around it, the
  // least of several measurements, so a thread switch doesn't skew it.
  double MeasurePointCost();

  // Frames are timings that also record the frame as a whole. Frames can't
  // nest.
  void StartFrame(int index, Timing& timing);
  void EndFrame(Timing& timing);

  // Returns the current frame's time, `ticks`, the class of core it ran on,
  // `core`, and the part each point had in it, on every thread.
  FrameBreakdown CaptureFrame(int64_t ticks, int core) const
      ABSL_LOCKS_EXCLUDED(joined_mutex_);

  // Adds the part this Profiler's own slots had in the frame numbered
  // `frame_number` to `points`, by point index. Any thread may call it.
  void AddOwnFramePoints(int64_t frame_number,
                         absl::Span<FrameBreakdown::Point> points) const;

  // Adds one thread's part in a frame for a point, `part`, to `point`.
  static void AddFramePoint(FrameBreakdown::Point& point,
                            const FrameBreakdown::Point& part);

  // As a joined Profiler, adds an outermost timed point's time on this thread,
  // `ticks`, to busy_, if a frame is in progress.
  void AddBusyTime(int64_t ticks);

  // As a main Profiler, adds the busy time `thread`'s joined Profiler has
  // added since it was last gathered to `thread`'s totals, and returns it.
  static int64_t GatherBusyTicks(JoinedThread& thread);

  // As a main Profiler, adds each joined thread's busy time in the frame in
  // progress to its entry in joined_threads_. A point that ends after this,
  // but before the frame does, counts in the next frame.
  void AddFrameBusyTimes() ABSL_LOCKS_EXCLUDED(joined_mutex_);

  // Returns the slot at `index` (a ProfilePoint's index), with its frame part
  // reset if it was for another frame.
  Slot& GetFrameSlot(int index) {
    Slot& slot = slots_[index];
    const int64_t frame = frame_->Get();
    if (slot.frame.Get() != frame) {
      slot.frame_count.Set(0);
      slot.frame_self_ticks.Set(0);
      slot.frame_wait_ticks.Set(0);
      slot.frame.SetRelease(frame);
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
  void AddWaitTime(int index, int64_t wait_ticks) {
    Slot& slot = GetFrameSlot(index);
    slot.wait_ticks.Add(wait_ticks);
    slot.frame_wait_ticks.Add(wait_ticks);
  }
  void SetValue(int index, int64_t value) {
    Slot& slot = slots_[index];
    slot.count.Add(1);
    slot.value.Set(value);
    slot.value_ticks.Set(ReadTicks());
  }

  // Returns the totals of every registered point, by index, from every thread.
  std::vector<PointTotals> GatherPointTotals() const;

  // As GatherPointTotals(), from only this Profiler's own slots. Any thread may
  // call it.
  std::vector<PointTotals> GatherOwnTotals() const;

  // Adds what this Profiler's own slots hold for each point in `totals`, less
  // `baseline` (see JoinedThread), to `totals`, both by point index. Any thread
  // may call it.
  void AddOwnTotals(absl::Span<PointTotals> totals,
                    absl::Span<const PointTotals> baseline) const;

  // Adds what one thread recorded for a point, `thread`, to `totals`, keeping
  // the value set most recently.
  static void AddPointTotals(PointTotals& totals, const PointTotals& thread);

  // Clears what this Profiler's own thread recorded.
  void ResetOwn();

  // As a main Profiler, adds `profiler` to joined_threads_ under `name`, or
  // the lowest "thread N" free if `name` is empty, and returns its index. It
  // takes over an entry of that name whose joined Profilers were all
  // destroyed, if there is one.
  int AddJoinedThread(Profiler* profiler, std::string_view name)
      ABSL_LOCKS_EXCLUDED(joined_mutex_);

  // As a main Profiler, removes the joined Profiler at `index` in
  // joined_threads_ (from AddJoinedThread()), keeping what it recorded in the
  // entry, and its part in the frame in progress. It is called on the joined
  // Profiler's thread, as it is destroyed.
  void RemoveJoinedThread(int index) ABSL_LOCKS_EXCLUDED(joined_mutex_);

  // Report sections, each appended to `report`. AppendValues() and
  // AppendPoints() take every registered point with its totals, sorted for the
  // report, and AppendFrame() takes the registered points in index order.
  using ReportPoint = std::pair<ProfilePoint, PointTotals>;
  void AppendValues(std::string& report,
                    absl::Span<const ReportPoint> points) const;
  void AppendFrameSummary(std::string& report) const;
  void AppendCoreFrames(std::string& report) const;
  void AppendThreads(std::string& report) const
      ABSL_LOCKS_EXCLUDED(joined_mutex_);
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

  // The number of the frame that points are recorded in: the main Profiler's
  // current_frame_ for a joined Profiler, or else this Profiler's own.
  const SingleWriterInt64* const frame_;

  // The main Profiler this is joined to, at joined_index_ in its
  // joined_threads_, or null.
  Profiler* const main_;

  // The innermost timing of the fiber running on this thread, or null.
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

  // The number of the last frame started. Reset() leaves it, so no number is
  // used twice, as joined Profilers' slots aren't cleared, and can still hold
  // frame parts from before.
  int64_t last_frame_number_ = 0;

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

  // This Profiler's entry in main_->joined_threads_, or -1.
  int joined_index_ = -1;

  // A main Profiler's thread's name, for the threads table. Only a main
  // Profiler uses it, as a joined Profiler's is in its main Profiler's
  // joined_threads_.
  const std::string name_;

  // Only a joined Profiler records busy time, as the main thread's is its
  // frames' time.
  BusyTime busy_;

  // The Profilers joined to this main Profiler, by name (see JoinedThread), in
  // the order they were first used.
  mutable absl::Mutex joined_mutex_;
  std::vector<JoinedThread> joined_threads_ ABSL_GUARDED_BY(joined_mutex_);

  // The part in the frame numbered `left_frame_number_` of the joined
  // Profilers destroyed while it was in progress, by point index.
  int64_t left_frame_number_ ABSL_GUARDED_BY(joined_mutex_) = 0;
  std::vector<FrameBreakdown::Point> left_frame_points_
      ABSL_GUARDED_BY(joined_mutex_);
};

}  // namespace gb

#endif  // GB_PROFILE_PROFILER_H_
