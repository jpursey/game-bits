// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profiler.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/internal/cycleclock.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/thread/cpu_ticks.h"
#include "gb/thread/fiber.h"
#include "gb/thread/thread.h"

#ifdef _MSC_VER
#define GB_PROFILE_NOINLINE __declspec(noinline)
#else
#define GB_PROFILE_NOINLINE __attribute__((noinline))
#endif

namespace gb {

namespace {

// Returns `time` in ticks, limited to what an int64_t holds (so an infinite
// time is the most ticks there can be).
int64_t ToTicks(absl::Duration time, double ticks_per_second) {
  const double ticks = absl::ToDoubleSeconds(time) * ticks_per_second;
  if (ticks >= 0x1p63) {
    return std::numeric_limits<int64_t>::max();
  }
  if (ticks < -0x1p63) {
    return std::numeric_limits<int64_t>::min();
  }
  return static_cast<int64_t>(ticks);
}

// How the cost of a timed point is measured: this many rounds of this many
// points, keeping the fastest round.
constexpr int kCostRounds = 5;
constexpr int kCostPoints = 100;

// The point contended absl::Mutex locks are recorded in (see
// RecordMutexWait()).
const ProfilePoint& GetMutexPoint() {
  return internal::GetNamedPoint<ProfilePoint::Kind::kCall, "absl::Mutex">();
}

// The kinds of point each reader accepts.
constexpr ProfilePoint::Kind kCountedKinds[] = {
    ProfilePoint::Kind::kFrame, ProfilePoint::Kind::kScope,
    ProfilePoint::Kind::kCall, ProfilePoint::Kind::kJob,
    ProfilePoint::Kind::kCounter};
constexpr ProfilePoint::Kind kTimedKinds[] = {
    ProfilePoint::Kind::kFrame, ProfilePoint::Kind::kScope,
    ProfilePoint::Kind::kCall, ProfilePoint::Kind::kJob};
constexpr ProfilePoint::Kind kValueKinds[] = {ProfilePoint::Kind::kValue};
constexpr ProfilePoint::Kind kJobKinds[] = {ProfilePoint::Kind::kJob};

// Returns the histogram bucket for a time of `ticks`. Buckets 0 to 7 hold
// exactly 0 to 7 ticks. Above that, the bucket comes from the position of the
// highest set bit and the three bits below it.
int GetHistogramBucket(int64_t ticks) {
  if (ticks < 8) {
    return static_cast<int>(std::max<int64_t>(ticks, 0));
  }
  const int high_bit = std::bit_width(static_cast<uint64_t>(ticks)) - 1;
  const int next_bits = static_cast<int>((ticks >> (high_bit - 3)) & 7);
  return (high_bit - 2) * 8 + next_bits;
}

// Returns the middle of the range of ticks in `bucket`.
int64_t GetHistogramBucketMiddle(int bucket) {
  if (bucket < 8) {
    return bucket;
  }
  const int high_bit = bucket / 8 + 2;
  const int64_t width = int64_t{1} << (high_bit - 3);
  return (8 + bucket % 8) * width + width / 2;
}

//------------------------------------------------------------------------------
// Report formatting
//------------------------------------------------------------------------------

using Table = std::vector<std::vector<std::string>>;

// Shown in a table for a number that doesn't apply.
constexpr std::string_view kNotApplicable = "-";

// Formats a number with three significant digits, such as "0.50", "27.1", or
// "142".
std::string FormatNumber(double number) {
  const int decimals = number >= 100 ? 0 : (number >= 10 ? 1 : 2);
  return absl::StrFormat("%.*f", decimals, number);
}

// Formats a time with three significant digits, in the largest unit it has at
// least one of, such as "27.1us". Zero is "0".
std::string FormatTime(absl::Duration time) {
  const double nanoseconds = absl::ToDoubleNanoseconds(time);
  if (nanoseconds == 0) {
    return "0";
  }
  if (nanoseconds >= 1e9) {
    return FormatNumber(nanoseconds / 1e9) + "s";
  }
  if (nanoseconds >= 1e6) {
    return FormatNumber(nanoseconds / 1e6) + "ms";
  }
  if (nanoseconds >= 1e3) {
    return FormatNumber(nanoseconds / 1e3) + "us";
  }
  return FormatNumber(nanoseconds) + "ns";
}

// Formats a tick rate in GHz with three significant digits, such as "3.19GHz".
std::string FormatTickRate(double ticks_per_second) {
  return FormatNumber(ticks_per_second / 1e9) + "GHz";
}

std::string_view GetKindName(ProfilePoint::Kind kind) {
  switch (kind) {
    case ProfilePoint::Kind::kFrame:
      return "frame";
    case ProfilePoint::Kind::kScope:
      return "scope";
    case ProfilePoint::Kind::kCall:
      return "call";
    case ProfilePoint::Kind::kJob:
      return "job";
    case ProfilePoint::Kind::kCounter:
      return "counter";
    case ProfilePoint::Kind::kValue:
      return "value";
  }
  return "unknown";
}

// Formats a point's self time or wait time, which only a timed point has.
std::string FormatPointTime(ProfilePoint::Kind kind, absl::Duration time) {
  if (!absl::c_linear_search(kTimedKinds, kind)) {
    return std::string(kNotApplicable);
  }
  return FormatTime(time);
}

// As FormatPointTime(), for `time` per call over `count` calls. A point that
// has waited, but not yet ended, has no calls, and so no time per call.
std::string FormatPerCall(ProfilePoint::Kind kind, absl::Duration time,
                          int64_t count) {
  if (count == 0) {
    return std::string(kNotApplicable);
  }
  return FormatPointTime(kind, time / count);
}

// Formats `part` as a share of `whole`, as a percentage with three significant
// digits, such as "19.2%". There is no share of a `whole` of zero.
std::string FormatPercent(int64_t part, int64_t whole) {
  if (whole <= 0) {
    return std::string(kNotApplicable);
  }
  return FormatNumber(static_cast<double>(part) * 100 /
                      static_cast<double>(whole)) +
         "%";
}

// Formats a point's self time as a share of the frames' time, `frame_ticks`.
// Only a timed point has a share, and only once frames have taken time.
std::string FormatShare(ProfilePoint::Kind kind, int64_t self_ticks,
                        int64_t frame_ticks) {
  if (!absl::c_linear_search(kTimedKinds, kind)) {
    return std::string(kNotApplicable);
  }
  return FormatPercent(self_ticks, frame_ticks);
}

// Returns whether `a` comes before `b` in a report: grouped by kind, and
// sorted by name.
bool IsBeforeInReport(const ProfilePoint& a, const ProfilePoint& b) {
  return std::pair(a.GetKind(), a.GetName()) <
         std::pair(b.GetKind(), b.GetName());
}

// Appends `table` to `report` as columns, each as wide as its widest cell and
// separated by two spaces. Every row has the same number of cells. The first
// `left_columns` columns are aligned left, and the rest right. No line has
// trailing spaces.
void AppendTable(std::string& report, const Table& table, int left_columns) {
  if (table.empty()) {
    return;
  }
  const int columns = static_cast<int>(table[0].size());
  std::vector<int> widths(columns, 0);
  for (const std::vector<std::string>& row : table) {
    for (int i = 0; i < columns; ++i) {
      widths[i] = std::max(widths[i], static_cast<int>(row[i].size()));
    }
  }
  for (const std::vector<std::string>& row : table) {
    std::string line;
    for (int i = 0; i < columns; ++i) {
      const std::string padding(widths[i] - row[i].size(), ' ');
      absl::StrAppend(&line, i > 0 ? "  " : "",
                      i < left_columns ? row[i] : padding,
                      i < left_columns ? padding : row[i]);
    }
    line.erase(line.find_last_not_of(' ') + 1);
    absl::StrAppend(&report, line, "\n");
  }
}

}  // namespace

Profiler::Profiler() : Profiler(Options()) {}

Profiler::Profiler(Options options)
    : fake_cpu_(options.fake_cpu == nullptr && options.main != nullptr
                    ? options.main->fake_cpu_
                    : options.fake_cpu),
      slots_(std::make_unique<Slot[]>(kMaxProfilePoints)),
      job_stats_(std::make_unique<JobStatsPointer[]>(kMaxProfilePoints)),
      frame_profiler_(options.main != nullptr ? options.main : this),
      main_(options.main),
      threadless_(options.threadless),
      ticks_per_second_(fake_cpu_ != nullptr ? fake_cpu_->GetCpuTicksPerSecond()
                                             : GetCpuTicksPerSecond()),
      ticks_per_mutex_cycle_(ticks_per_second_ /
                             absl::base_internal::CycleClock::Frequency()),
      budget_per_frame_(options.budget_per_frame),
      budget_fraction_(options.budget_fraction),
      slow_frame_ticks_(ToTicks(options.slow_frame, ticks_per_second_)),
      on_slow_frame_(std::move(options.on_slow_frame)),
      name_(options.name.empty() ? "main" : options.name) {
  if (threadless_) {
    CHECK(main_ == nullptr) << "A threadless Profiler can't have a main";
  } else {
    CHECK(s_current == nullptr) << "Only one Profiler may exist per thread";
    s_current = this;
  }
  if (main_ != nullptr) {
    CHECK(main_->main_ == nullptr)
        << "A joined Profiler's main Profiler can't itself be joined";
    CHECK(ticks_per_second_ == main_->ticks_per_second_)
        << "A joined Profiler must have its main Profiler's tick rate";
  }

  // No timing is open before the first Profiler, so no switch with one is
  // missed. Setting them again does nothing.
  static constexpr FiberSwitchCallbacks kFiberSwitchCallbacks = {
      .on_suspend = &SuspendTimings,
      .on_resume = &ResumeTimings,
  };
  SetFiberSwitchCallbacks(&kFiberSwitchCallbacks);
  RegisterMutexHook();

  // The first read of the core's class finds every core's class, which is
  // done here rather than in the first frame.
  if (fake_cpu_ == nullptr) {
    GetThisThreadCoreClass();
  }

  // Measuring times points into the slots, which are then cleared.
  point_cost_ticks_ = MeasurePointCost();
  ResetOwn();

  // The main Profiler reads the slots from here on, so this joins it last.
  if (main_ != nullptr) {
    // GetThreadName() is "null" for a thread CreateThread() didn't make.
    std::string_view name = options.name;
    if (name.empty() && GetThisThread() != nullptr) {
      name = GetThreadName(GetThisThread());
    }
    joined_index_ = main_->AddJoinedThread(this, name);
  }
}

Profiler::~Profiler() {
  CHECK(threadless_ || s_current == this)
      << "A Profiler must be destroyed on the thread that created it";
  CHECK(top_ == nullptr) << "A Profiler must not be destroyed in a timed point";
  CHECK(thread_time_.run_state.Get() != kIdle)
      << "A Profiler must not be destroyed in an idle timer";
  if (main_ != nullptr) {
    main_->RemoveJoinedThread(joined_index_);
  }
  {
    absl::MutexLock lock(&joined_mutex_);
    for (const JoinedThread& thread : joined_threads_) {
      CHECK(thread.profiler == nullptr)
          << "A main Profiler must outlive its joined Profilers, but \""
          << thread.name << "\" still exists";
    }
  }
  if (!threadless_) {
    s_current = nullptr;
  }
}

int Profiler::AddJoinedThread(Profiler* profiler, std::string_view name) {
  absl::MutexLock lock(&joined_mutex_);

  // Numbered entries are added in the order of their numbers, so the first
  // one free has the lowest number free.
  int numbered_count = 0;
  for (int i = 0; i < static_cast<int>(joined_threads_.size()); ++i) {
    JoinedThread& thread = joined_threads_[i];
    numbered_count += thread.numbered ? 1 : 0;
    if (thread.profiler == nullptr &&
        (name.empty() ? thread.numbered : thread.name == name)) {
      thread.profiler = profiler;
      return i;
    }
  }
  joined_threads_.push_back(
      {.profiler = profiler,
       .name = name.empty() ? absl::StrCat("thread ", numbered_count + 1)
                            : std::string(name),
       .numbered = name.empty()});
  return static_cast<int>(joined_threads_.size()) - 1;
}

void Profiler::RemoveJoinedThread(int index) {
  const int point_count = ProfilePoint::GetRegisteredCount();
  const int64_t frame_number = numbers_.frame.Get();
  absl::MutexLock lock(&joined_mutex_);
  JoinedThread& thread = joined_threads_[index];
  if (static_cast<int>(thread.folded.size()) < point_count) {
    thread.folded.resize(point_count);
  }
  thread.profiler->AddOwnTotals(absl::MakeSpan(thread.folded), thread.baseline);
  if (static_cast<int>(thread.folded_jobs.size()) < point_count) {
    thread.folded_jobs.resize(point_count);
  }
  thread.profiler->AddOwnJobTotals(numbers_.resets.Get(),
                                   absl::MakeSpan(thread.folded_jobs));

  // If the frame has ended since its number was read, its part is kept but
  // never captured, and its busy time counts in the next frame. The thread is
  // outside every timed point and idle timer, so it has no run in progress to
  // read the ticks for.
  if (frame_number != 0) {
    thread.left_busy_ticks +=
        GatherThreadTicks(thread, /*now_ticks=*/0, /*in_frame=*/true);
    if (left_frame_number_ != frame_number) {
      left_frame_number_ = frame_number;
      left_frame_points_.clear();
    }
    if (static_cast<int>(left_frame_points_.size()) < point_count) {
      left_frame_points_.resize(point_count);
    }
    thread.profiler->AddOwnFramePoints(frame_number,
                                       absl::MakeSpan(left_frame_points_));
  }

  // The next Profiler under this name starts from no busy or idle time.
  thread.gathered_ticks = {};
  thread.profiler = nullptr;
  thread.baseline.clear();
}

// Never inlined, even by link-time code generation.
GB_PROFILE_NOINLINE Profiler* Profiler::GetCurrent() { return s_current; }

// The callbacks don't span a switch, as SwitchToFiber() calls each through a
// pointer, on the thread it runs on, so they read s_current directly.
void* Profiler::SuspendTimings() {
  Profiler* const profiler = s_current;
  if (profiler == nullptr) {
    return nullptr;
  }
  CHECK(profiler->thread_time_.run_state.Get() != kIdle)
      << "A fiber can't switch in an idle timer";
  if (profiler->top_ == nullptr) {
    return nullptr;
  }
  CHECK(profiler->frame_profiler_->threadless_ || !profiler->top_->in_frame)
      << "A profile frame can't span a fiber switch, unless its main Profiler "
         "is threadless";
  // Each timing keeps its Profiler, so a timer ended on another fiber meanwhile
  // fails EndTiming()'s CHECK, rather than being ignored.
  const int64_t ticks = profiler->ReadTicks();
  for (Timing* timing = profiler->top_; timing != nullptr;
       timing = timing->parent) {
    timing->start_ticks = ticks - timing->start_ticks;
  }
  if (profiler->main_ != nullptr) {
    profiler->EndRun(ticks);
  }
  profiler->top_->suspend_ticks = ticks;
  return std::exchange(profiler->top_, nullptr);
}

void Profiler::ResumeTimings(void* state) {
  if (state == nullptr) {
    return;
  }
  Timing* const top = static_cast<Timing*>(state);
  Profiler* const profiler = s_current;

  // A frame can only end in a Profiler that records it in a main Profiler with
  // a frame in progress, which can only be its own. The Profiler the frame
  // suspended from may be destroyed by now, so it isn't compared.
  CHECK(!top->in_frame ||
        (profiler != nullptr &&
         profiler->frame_profiler_->numbers_.frame.Get() != 0))
      << "A profile frame's fiber must resume on a thread joined to the "
         "frame's main Profiler";
  if (profiler == nullptr) {
    for (Timing* timing = top; timing != nullptr; timing = timing->parent) {
      timing->profiler = nullptr;
    }
    return;
  }
  CHECK(profiler->top_ == nullptr)
      << "A fiber resumed on a thread with timed points open";
  const int64_t ticks = profiler->ReadTicks();

  // The wait is measured across two threads' ticks, which may disagree by a
  // little, so it is never less than zero.
  profiler->AddWaitTime(top->index,
                        std::max<int64_t>(ticks - top->suspend_ticks, 0),
                        top->in_frame);
  for (Timing* timing = top; timing != nullptr; timing = timing->parent) {
    timing->start_ticks = ticks - timing->start_ticks;
    timing->profiler = profiler;
  }
  profiler->top_ = top;
  if (profiler->main_ != nullptr) {
    profiler->StartRun(ticks, profiler->ReadCoreClass());
  }
}

void Profiler::StartJob(int index, int64_t queued_ticks, JobTiming& job) {
  StartTiming(index, job.timing);
  job.timing.is_job = true;

  // The job may have been queued on another thread, whose ticks may disagree
  // by a little, so the delay is never less than zero.
  job.start_delay_ticks =
      std::max<int64_t>(job.timing.start_ticks - queued_ticks, 0);
}

void Profiler::EndJob(JobTiming& job) {
  const int64_t run_ticks = EndTiming(job.timing);
  JobStats& stats = GetJobStats(job.timing.index);
  stats.run_ticks.Add(run_ticks);
  stats.max_run_ticks.SetMax(run_ticks);
  stats.run_buckets[GetHistogramBucket(run_ticks)].Add(1);
  stats.start_delay_ticks.Add(job.start_delay_ticks);
  stats.max_start_delay_ticks.SetMax(job.start_delay_ticks);
}

void Profiler::AddResumeDelay(int64_t ready_ticks) {
  Profiler* const profiler = GetCurrent();
  if (profiler == nullptr) {
    return;
  }
  for (Timing* timing = profiler->top_; timing != nullptr;
       timing = timing->parent) {
    if (!timing->is_job) {
      continue;
    }

    // What the job waited on may have been ready on another thread.
    const int64_t delay_ticks =
        std::max<int64_t>(profiler->ReadTicks() - ready_ticks, 0);
    JobStats& stats = profiler->GetJobStats(timing->index);
    stats.resumes.Add(1);
    stats.resume_delay_ticks.Add(delay_ticks);
    stats.max_resume_delay_ticks.SetMax(delay_ticks);
    return;
  }
}

void Profiler::RegisterMutexHook() {
  // The hook's point is registered first, as registering it locks a mutex,
  // which the hook can't do from inside an unlock. Registering either again
  // does nothing.
  GetMutexPoint();
  absl::RegisterMutexProfiler(&RecordMutexWait);
}

void Profiler::RecordMutexWait(int64_t wait_cycles) {
  Profiler* const profiler = GetCurrent();
  if (profiler == nullptr) {
    return;
  }
  const int index = GetMutexPoint().GetIndex();
  const int64_t wait_ticks = static_cast<int64_t>(
      static_cast<double>(wait_cycles) * profiler->ticks_per_mutex_cycle_);
  profiler->AddCount(index, 1);
  profiler->AddWaitTime(index, wait_ticks, /*in_frame=*/false);
}

Profiler::JobStats& Profiler::GetJobStats(int index) {
  // Only this thread writes the pointer, so it reads it relaxed.
  std::atomic<JobStats*>& pointer = job_stats_[index].stats;
  JobStats* stats = pointer.load(std::memory_order_relaxed);
  const int64_t resets = frame_profiler_->numbers_.resets.Get();
  if (stats == nullptr) {
    stats = new JobStats();
    stats->resets.Set(resets);
    pointer.store(stats, std::memory_order_release);
  } else if (stats->resets.Get() != resets) {
    // A reader ignores the stats until `resets` is set, after they are
    // cleared.
    stats->Clear();
    stats->resets.SetRelease(resets);
  }
  return *stats;
}

absl::Duration Profiler::GetPointCost() const {
  return TicksToDuration(point_cost_ticks_);
}

Profiler::FrameSummary Profiler::GetFrameSummary() const {
  absl::MutexLock lock(&frame_mutex_);
  return DoGetFrameSummary(frame_totals_);
}

Profiler::FrameSummary Profiler::DoGetFrameSummary(
    const FrameTotals& frames) const {
  if (frames.frames == 0) {
    return {};
  }
  const absl::Duration total = TicksToDuration(frames.ticks);
  const absl::Duration average = total / frames.frames;
  const bool has_budget =
      budget_per_frame_ != absl::ZeroDuration() || budget_fraction_ != 0;
  return {
      .frames = frames.frames,
      .total = total,
      .average = average,
      .p50 = TicksToDuration(GetPercentileTicks(frames.buckets, frames.frames,
                                                frames.slowest.ticks, 0.5)),
      .p90 = TicksToDuration(GetPercentileTicks(frames.buckets, frames.frames,
                                                frames.slowest.ticks, 0.9)),
      .p99 = TicksToDuration(GetPercentileTicks(frames.buckets, frames.frames,
                                                frames.slowest.ticks, 0.99)),
      .max = TicksToDuration(frames.slowest.ticks),
      .profiler_cost =
          TicksToDuration(static_cast<double>(frames.timed_points) *
                          point_cost_ticks_ / frames.frames),
      .budget = has_budget
                    ? std::max(budget_per_frame_, average * budget_fraction_)
                    : absl::InfiniteDuration(),
  };
}

Profiler::JobSummary Profiler::GetJobSummary(std::string_view name) const {
  const int index = FindIndex(name, kJobKinds);
  if (index < 0) {
    return {};
  }
  return DoGetJobSummary(GatherJobTotals()[index]);
}

Profiler::JobSummary Profiler::DoGetJobSummary(const JobTotals& job) const {
  JobSummary summary = {.runs = job.runs, .resumes = job.resumes};
  if (job.runs > 0) {
    summary.total = TicksToDuration(job.run_ticks);
    summary.average = summary.total / job.runs;
    summary.p50 = TicksToDuration(
        GetPercentileTicks(job.run_buckets, job.runs, job.max_run_ticks, 0.5));
    summary.p90 = TicksToDuration(
        GetPercentileTicks(job.run_buckets, job.runs, job.max_run_ticks, 0.9));
    summary.p99 = TicksToDuration(
        GetPercentileTicks(job.run_buckets, job.runs, job.max_run_ticks, 0.99));
    summary.max = TicksToDuration(job.max_run_ticks);
    summary.start_delay = TicksToDuration(job.start_delay_ticks) / job.runs;
    summary.max_start_delay = TicksToDuration(job.max_start_delay_ticks);
  }
  if (job.resumes > 0) {
    summary.resume_delay =
        TicksToDuration(job.resume_delay_ticks) / job.resumes;
    summary.max_resume_delay = TicksToDuration(job.max_resume_delay_ticks);
  }
  return summary;
}

int64_t Profiler::GetCount(std::string_view name) const {
  return FindPointTotals(name, kCountedKinds).count;
}

absl::Duration Profiler::GetSelfTime(std::string_view name) const {
  return TicksToDuration(FindPointTotals(name, kTimedKinds).self_ticks);
}

absl::Duration Profiler::GetWaitTime(std::string_view name) const {
  return TicksToDuration(FindPointTotals(name, kTimedKinds).wait_ticks);
}

int64_t Profiler::GetValue(std::string_view name) const {
  return FindPointTotals(name, kValueKinds).value;
}

int64_t Profiler::GetSlowestFrameCount(std::string_view name) const {
  return FindSlowestFramePoint(name, kCountedKinds).count;
}

absl::Duration Profiler::GetSlowestFrameSelfTime(std::string_view name) const {
  return TicksToDuration(FindSlowestFramePoint(name, kTimedKinds).self_ticks);
}

void Profiler::Reset() {
  CHECK(main_ == nullptr)
      << "A joined Profiler can't be reset, only its main Profiler";
  CHECK(top_ == nullptr) << "A Profiler can't be reset in a timed point";
  ResetOwn();

  // The joined Profilers' slots are written by their threads, so rather than
  // being cleared, what they hold now is subtracted from them, and their
  // JobStats are cleared by their threads as each is next written.
  absl::MutexLock lock(&joined_mutex_);
  numbers_.resets.Add(1);
  for (JoinedThread& thread : joined_threads_) {
    thread.folded.clear();
    thread.folded_jobs.clear();
    thread.state_ticks = {};
    thread.max_frame_busy_ticks = 0;
    thread.left_busy_ticks = 0;
    if (thread.profiler != nullptr) {
      thread.baseline = thread.profiler->GatherOwnTotals();
    }
  }
}

void Profiler::ResetOwn() {
  {
    // Only a threadless Profiler can be outside every timed point here while
    // a frame is in progress, on another thread.
    absl::MutexLock lock(&frame_mutex_);
    CHECK(numbers_.frame.Get() == 0)
        << "A threadless Profiler can't be reset during a frame";
    frame_totals_ = {};
  }
  std::fill_n(slots_.get(), kMaxProfilePoints, Slot());
  thread_time_ = ThreadTime();
}

double Profiler::MeasurePointCost() {
  // The points are timed inside another, as most are, so each also adds its
  // time to its parent's.
  Timing outer;
  StartTiming(0, outer);
  int64_t fastest_ticks = std::numeric_limits<int64_t>::max();
  for (int round = 0; round < kCostRounds; ++round) {
    const int64_t start_ticks = ReadTicks();
    for (int i = 0; i < kCostPoints; ++i) {
      Timing timing;
      StartTiming(0, timing);
      EndTiming(timing);
    }
    fastest_ticks = std::min(fastest_ticks, ReadTicks() - start_ticks);
  }
  EndTiming(outer);
  return static_cast<double>(fastest_ticks) / kCostPoints;
}

void Profiler::StartFrame(int index, Timing& timing) {
  CHECK(main_ == nullptr || main_->threadless_)
      << "A joined Profiler can't time frames, unless its main Profiler is "
         "threadless";
  CHECK(top_ == nullptr || !top_->in_frame) << "Profile frames can't nest";
  const int start_core = ReadCoreClass();
  StartTiming(index, timing);
  timing.in_frame = true;
  frame_profiler_->OpenFrame(timing.start_ticks, start_core);
}

void Profiler::EndFrame(Timing& timing) {
  // The frame's fiber may have been suspended, so its end is from its start
  // ticks, which resuming moves on by the time suspended.
  const int64_t end_ticks = timing.start_ticks + EndTiming(timing);
  frame_profiler_->RecordFrame(end_ticks, ReadCoreClass(),
                               timing.child_points + 1);
}

void Profiler::OpenFrame(int64_t start_ticks, int start_core) {
  absl::MutexLock lock(&frame_mutex_);
  CHECK(numbers_.frame.Get() == 0)
      << "Only one profile frame can be in progress at a time";
  numbers_.frame.Set(++last_frame_number_);
  current_frame_start_ticks_ = start_ticks;
  current_frame_start_core_ = start_core;

  // Busy and idle time before the frame isn't counted.
  absl::MutexLock joined_lock(&joined_mutex_);
  for (JoinedThread& thread : joined_threads_) {
    if (thread.profiler != nullptr) {
      GatherThreadTicks(thread, start_ticks, /*in_frame=*/false);
    }
  }
}

void Profiler::RecordFrame(int64_t end_ticks, int end_core,
                           int64_t timed_points) {
  std::optional<FrameBreakdown> slow_frame;
  {
    absl::MutexLock lock(&frame_mutex_);

    // A frame that ended on another thread is timed across two threads'
    // ticks, which may disagree by a little, so it is never less than zero.
    const int64_t ticks =
        std::max<int64_t>(end_ticks - current_frame_start_ticks_, 0);
    FrameTotals& totals = frame_totals_;
    totals.timed_points += timed_points;
    ++totals.frames;
    totals.ticks += ticks;
    ++totals.buckets[GetHistogramBucket(ticks)];
    const int core =
        end_core == current_frame_start_core_ ? end_core : kMixedCores;
    CoreFrames& core_frames = totals.cores[core];
    ++core_frames.frames;
    core_frames.ticks += ticks;
    core_frames.max_ticks = std::max(core_frames.max_ticks, ticks);
    const bool is_slowest = ticks > totals.slowest.ticks;
    if (is_slowest) {
      totals.slowest = CaptureFrame(ticks, core, timed_points);
    }
    if (ticks > slow_frame_ticks_ && on_slow_frame_ != nullptr) {
      slow_frame =
          is_slowest ? totals.slowest : CaptureFrame(ticks, core, timed_points);
    }
    AddFrameThreadTicks(end_ticks);
    numbers_.frame.Set(0);
  }

  // The report is made outside the lock, and the callback is called after the
  // frame ends, so any points it reaches are outside the frame.
  if (slow_frame.has_value()) {
    std::string report;
    AppendFrame(report, "Slow frame", *slow_frame,
                ProfilePoint::GetRegisteredPoints());
    on_slow_frame_(report);
  }
}

Profiler::FrameBreakdown Profiler::CaptureFrame(int64_t ticks, int core,
                                                int64_t timed_points) const {
  FrameBreakdown frame = {
      .ticks = ticks, .core = core, .timed_points = timed_points};
  const int64_t frame_number = numbers_.frame.Get();
  frame.points.resize(ProfilePoint::GetRegisteredCount());
  AddOwnFramePoints(frame_number, absl::MakeSpan(frame.points));
  {
    absl::MutexLock lock(&joined_mutex_);
    frame.has_joined = !joined_threads_.empty();
    for (const JoinedThread& thread : joined_threads_) {
      if (thread.profiler != nullptr) {
        thread.profiler->AddOwnFramePoints(frame_number,
                                           absl::MakeSpan(frame.points));
      }
    }
    if (left_frame_number_ == frame_number) {
      // Points may be registered while this runs, as in GatherPointTotals().
      const int count = std::min(static_cast<int>(frame.points.size()),
                                 static_cast<int>(left_frame_points_.size()));
      for (int i = 0; i < count; ++i) {
        if (left_frame_points_[i].index >= 0) {
          AddFramePoint(frame.points[i], left_frame_points_[i]);
        }
      }
    }
  }
  std::erase_if(frame.points, [](const FrameBreakdown::Point& point) {
    return point.index < 0;
  });
  return frame;
}

void Profiler::AddOwnFramePoints(
    int64_t frame_number, absl::Span<FrameBreakdown::Point> points) const {
  for (int i = 0; i < static_cast<int>(points.size()); ++i) {
    const Slot& slot = slots_[i];
    if (slot.frame.GetAcquire() != frame_number) {
      continue;
    }
    AddFramePoint(points[i],
                  {.index = i,
                   .count = slot.frame_count.Get(),
                   .self_ticks = slot.frame_self_ticks.Get(),
                   .wait_ticks = slot.frame_wait_ticks.Get(),
                   .other_self_ticks = slot.frame_other_self_ticks.Get(),
                   .other_wait_ticks = slot.frame_other_wait_ticks.Get()});
  }
}

void Profiler::AddFramePoint(FrameBreakdown::Point& point,
                             const FrameBreakdown::Point& part) {
  point.index = part.index;
  point.count += part.count;
  point.self_ticks += part.self_ticks;
  point.wait_ticks += part.wait_ticks;
  point.other_self_ticks += part.other_self_ticks;
  point.other_wait_ticks += part.other_wait_ticks;
}

void Profiler::StartRun(int64_t ticks, int state) {
  DCHECK(thread_time_.run_state.Get() < 0)
      << "A timed point can't be inside an idle timer";

  // The start is written before the state, which says there is a run, so a
  // reader that sees the state sees the start (see ReadThreadTicks()).
  thread_time_.run_start_ticks.Set(ticks);
  thread_time_.run_state.SetRelease(state);
}

void Profiler::EndRun(int64_t ticks) {
  // The run is ended before it is added, so a reader that sees it added sees
  // it ended, and doesn't count it twice (see ReadThreadTicks()).
  const int state = static_cast<int>(thread_time_.run_state.Get());
  DCHECK(state >= 0) << "A run ended without starting";
  thread_time_.run_state.Set(-1);
  SingleWriterInt64& state_ticks = thread_time_.state_ticks[state];
  state_ticks.SetRelease(state_ticks.Get() + ticks -
                         thread_time_.run_start_ticks.Get());
}

void Profiler::StartIdle() {
  CHECK(top_ == nullptr) << "An idle timer must be outside every timed point";
  CHECK(thread_time_.run_state.Get() != kIdle) << "Idle timers can't nest";
  StartRun(ReadTicks(), kIdle);
}

void Profiler::EndIdle() {
  // StartRun() only DCHECKs this, as every outermost point reaches it.
  CHECK(thread_time_.run_state.Get() == kIdle)
      << "A timed point can't be inside an idle timer";
  EndRun(ReadTicks());
}

Profiler::StateTicks Profiler::ReadThreadTicks(int64_t now_ticks) const {
  // The ended runs are read before the run in progress, as EndRun() writes
  // them in the other order. A run that ends while this reads is either in
  // the ended runs, or in neither.
  StateTicks ticks;
  for (int state = 0; state < static_cast<int>(ticks.size()); ++state) {
    ticks[state] = thread_time_.state_ticks[state].GetAcquire();
  }
  const int run_state = static_cast<int>(thread_time_.run_state.GetAcquire());
  if (run_state >= 0) {
    ticks[run_state] +=
        std::max<int64_t>(now_ticks - thread_time_.run_start_ticks.Get(), 0);
  }
  return ticks;
}

int64_t Profiler::GatherThreadTicks(JoinedThread& thread, int64_t now_ticks,
                                    bool in_frame) {
  // A read can leave out a run that a previous read included, if it ended
  // while this read, so time already gathered is never taken back.
  const StateTicks ticks = thread.profiler->ReadThreadTicks(now_ticks);
  int64_t busy_ticks = 0;
  for (int state = 0; state < static_cast<int>(ticks.size()); ++state) {
    const int64_t added =
        std::max<int64_t>(ticks[state] - thread.gathered_ticks[state], 0);
    thread.gathered_ticks[state] += added;
    if (in_frame) {
      thread.state_ticks[state] += added;
      busy_ticks += state != kIdle ? added : 0;
    }
  }
  return busy_ticks;
}

void Profiler::AddFrameThreadTicks(int64_t end_ticks) {
  absl::MutexLock lock(&joined_mutex_);
  for (JoinedThread& thread : joined_threads_) {
    int64_t ticks = thread.left_busy_ticks;
    thread.left_busy_ticks = 0;
    if (thread.profiler != nullptr) {
      ticks += GatherThreadTicks(thread, end_ticks, /*in_frame=*/true);
    }
    thread.max_frame_busy_ticks = std::max(thread.max_frame_busy_ticks, ticks);
  }
}

std::string Profiler::GetReport() const {
  const std::vector<ProfilePoint> registered =
      ProfilePoint::GetRegisteredPoints();
  const std::vector<PointTotals> totals = GatherPointTotals();
  const std::vector<JobTotals> jobs = GatherJobTotals();
  std::vector<ReportPoint> points;
  points.reserve(registered.size());
  for (const ProfilePoint& point : registered) {
    points.emplace_back(point, totals[point.GetIndex()]);
  }
  absl::c_sort(points, [](const ReportPoint& a, const ReportPoint& b) {
    return IsBeforeInReport(a.first, b.first);
  });
  FrameTotals frames;
  {
    absl::MutexLock lock(&frame_mutex_);
    frames = frame_totals_;
  }
  std::string report;
  AppendValues(report, points);
  AppendFrameSummary(report, frames);
  AppendCoreFrames(report, frames);
  AppendThreads(report, frames);
  AppendPoints(report, frames, points);
  AppendJobs(report, points, jobs);
  if (frames.frames > 0) {
    absl::StrAppend(&report, "\n");
    AppendFrame(report, "Slowest frame", frames.slowest, registered);
  }
  return report;
}

void Profiler::AppendValues(std::string& report,
                            absl::Span<const ReportPoint> points) const {
  Table table = {{"Value", "Latest"}};
  for (const auto& [point, totals] : points) {
    if (point.GetKind() == ProfilePoint::Kind::kValue && totals.IsRecorded()) {
      table.push_back(
          {std::string(point.GetName()), absl::StrCat(totals.value)});
    }
  }
  if (table.size() > 1) {
    AppendTable(report, table, 1);
    absl::StrAppend(&report, "\n");
  }
}

void Profiler::AppendFrameSummary(std::string& report,
                                  const FrameTotals& frames) const {
  const FrameSummary summary = DoGetFrameSummary(frames);
  Table table = {{"Frames", absl::StrCat(summary.frames)}};
  if (summary.frames > 0) {
    std::string profiler =
        absl::StrCat(FormatTime(summary.profiler_cost), " per frame, budget ",
                     summary.budget == absl::InfiniteDuration()
                         ? "none"
                         : FormatTime(summary.budget));
    if (summary.profiler_cost > summary.budget) {
      absl::StrAppend(&profiler, " (over budget)");
    }
    table.push_back({"Total", FormatTime(summary.total)});
    table.push_back({"Average", FormatTime(summary.average)});
    table.push_back({"P50", FormatTime(summary.p50)});
    table.push_back({"P90", FormatTime(summary.p90)});
    table.push_back({"P99", FormatTime(summary.p99)});
    table.push_back({"Max", FormatTime(summary.max)});
    table.push_back({"Profiler", std::move(profiler)});
  }
  table.push_back({"Point cost", FormatTime(GetPointCost())});
  table.push_back({"Tick rate", FormatTickRate(ticks_per_second_)});
  AppendTable(report, table, 2);
}

void Profiler::AppendCoreFrames(std::string& report,
                                const FrameTotals& frames) const {
  if (frames.frames == 0) {
    return;
  }
  Table table = {{"Core", "Frames", "Average", "Max"}};
  int cores = 0;
  for (int core = 0; core < static_cast<int>(frames.cores.size()); ++core) {
    const CoreFrames& core_frames = frames.cores[core];
    if (core_frames.frames == 0) {
      continue;
    }
    ++cores;
    table.push_back(
        {FormatCore(core), absl::StrCat(core_frames.frames),
         FormatTime(TicksToDuration(core_frames.ticks) / core_frames.frames),
         FormatTime(TicksToDuration(core_frames.max_ticks))});
  }
  absl::StrAppend(&report, "\n");
  AppendTable(report, table, 1);
  if (cores > 1) {
    absl::StrAppend(&report,
                    "Warning: frames ran on more than one class of core, "
                    "which run at different speeds\n");
  }
}

void Profiler::AppendThreads(std::string& report,
                             const FrameTotals& frames) const {
  // A thread's busy time in frames, by class of core, where the main thread's
  // frames can also be kMixedCores, and a joined thread's idle time in frames.
  struct ThreadBusy {
    std::string name;
    int64_t max_frame_ticks = 0;
    std::array<int64_t, kCoreClasses + 1> core_ticks = {};
    std::optional<int64_t> idle_ticks;
  };
  std::vector<ThreadBusy> threads;
  {
    absl::MutexLock lock(&joined_mutex_);
    if (joined_threads_.empty()) {
      return;
    }
    for (const JoinedThread& thread : joined_threads_) {
      ThreadBusy& busy = threads.emplace_back(
          ThreadBusy{.name = thread.name,
                     .max_frame_ticks = thread.max_frame_busy_ticks,
                     .idle_ticks = thread.state_ticks[kIdle]});
      // Only the busy states, as core_ticks' last is kMixedCores, not kIdle.
      std::copy_n(thread.state_ticks.begin(), kCoreClasses,
                  busy.core_ticks.begin());
    }
  }
  if (!threadless_) {
    ThreadBusy& main = threads.emplace_back(
        ThreadBusy{.name = name_, .max_frame_ticks = frames.slowest.ticks});
    for (int core = 0; core < static_cast<int>(frames.cores.size()); ++core) {
      main.core_ticks[core] = frames.cores[core].ticks;
    }
  }
  absl::c_stable_sort(threads, [](const ThreadBusy& a, const ThreadBusy& b) {
    return a.name < b.name;
  });

  // A column for each class of core any thread was busy on.
  std::vector<int> cores;
  for (int core = 0; core < kCoreClasses + 1; ++core) {
    if (absl::c_any_of(threads, [core](const ThreadBusy& busy) {
          return busy.core_ticks[core] > 0;
        })) {
      cores.push_back(core);
    }
  }
  const bool has_idle = absl::c_any_of(
      threads, [](const ThreadBusy& busy) { return busy.idle_ticks > 0; });

  Table table = {{"Thread", "Busy/frame", "Max", "Share"}};
  for (int core : cores) {
    table[0].push_back(FormatCore(core));
  }
  if (has_idle) {
    table[0].push_back("Idle/frame");
    table[0].push_back("Idle share");
  }
  const bool has_frames = frames.frames > 0;
  for (const ThreadBusy& busy : threads) {
    int64_t busy_ticks = 0;
    for (int64_t ticks : busy.core_ticks) {
      busy_ticks += ticks;
    }
    std::vector<std::string>& row = table.emplace_back();
    row.push_back(busy.name);
    row.push_back(has_frames
                      ? FormatTime(TicksToDuration(busy_ticks) / frames.frames)
                      : std::string(kNotApplicable));
    row.push_back(has_frames ? FormatTime(TicksToDuration(busy.max_frame_ticks))
                             : std::string(kNotApplicable));
    row.push_back(FormatPercent(busy_ticks, frames.ticks));
    for (int core : cores) {
      row.push_back(busy.core_ticks[core] > 0
                        ? FormatPercent(busy.core_ticks[core], busy_ticks)
                        : std::string(kNotApplicable));
    }

    // The main thread has no idle time.
    if (has_idle) {
      const std::optional<int64_t>& idle_ticks = busy.idle_ticks;
      row.push_back(
          idle_ticks.has_value() && has_frames
              ? FormatTime(TicksToDuration(*idle_ticks) / frames.frames)
              : std::string(kNotApplicable));
      row.push_back(idle_ticks.has_value()
                        ? FormatPercent(*idle_ticks, frames.ticks)
                        : std::string(kNotApplicable));
    }
  }
  absl::StrAppend(&report, "\n");
  AppendTable(report, table, 1);
}

void Profiler::AppendPoints(std::string& report, const FrameTotals& frames,
                            absl::Span<const ReportPoint> points) const {
  Table table = {{"Point", "Kind", "Count", "Count/frame", "Self", "Self/call",
                  "Self/frame", "Share"}};
  const bool has_waits = absl::c_any_of(points, [](const ReportPoint& point) {
    return point.second.wait_ticks > 0;
  });
  if (has_waits) {
    table[0].push_back("Wait");
    table[0].push_back("Wait/call");
  }
  const bool has_frames = frames.frames > 0;
  for (const auto& [point, totals] : points) {
    if (point.GetKind() == ProfilePoint::Kind::kValue || !totals.IsRecorded()) {
      continue;
    }
    const ProfilePoint::Kind kind = point.GetKind();
    const absl::Duration self_time = TicksToDuration(totals.self_ticks);
    std::vector<std::string>& row = table.emplace_back();
    row.push_back(std::string(point.GetName()));
    row.push_back(std::string(GetKindName(kind)));
    row.push_back(absl::StrCat(totals.count));
    row.push_back(has_frames ? FormatNumber(static_cast<double>(totals.count) /
                                            frames.frames)
                             : std::string(kNotApplicable));
    row.push_back(FormatPointTime(kind, self_time));
    row.push_back(FormatPerCall(kind, self_time, totals.count));
    row.push_back(has_frames ? FormatPointTime(kind, self_time / frames.frames)
                             : std::string(kNotApplicable));
    row.push_back(FormatShare(kind, totals.self_ticks, frames.ticks));
    if (has_waits) {
      const absl::Duration wait_time = TicksToDuration(totals.wait_ticks);
      row.push_back(FormatPointTime(kind, wait_time));
      row.push_back(FormatPerCall(kind, wait_time, totals.count));
    }
  }
  absl::StrAppend(&report, "\n");
  AppendTable(report, table, 2);
}

void Profiler::AppendJobs(std::string& report,
                          absl::Span<const ReportPoint> points,
                          absl::Span<const JobTotals> jobs) const {
  Table table = {{"Job", "Count", "Run", "Run/call", "P50", "P90", "P99", "Max",
                  "Start delay", "Max start"}};
  const bool has_resumes = absl::c_any_of(
      jobs, [](const JobTotals& job) { return job.resumes > 0; });
  if (has_resumes) {
    table[0].push_back("Resumes");
    table[0].push_back("Resume delay");
    table[0].push_back("Max resume");
  }
  for (const ReportPoint& report_point : points) {
    const ProfilePoint& point = report_point.first;
    if (point.GetKind() != ProfilePoint::Kind::kJob) {
      continue;
    }
    const JobTotals& job = jobs[point.GetIndex()];
    if (job.runs == 0 && job.resumes == 0) {
      continue;
    }
    const JobSummary summary = DoGetJobSummary(job);
    const bool has_runs = summary.runs > 0;
    std::vector<std::string>& row = table.emplace_back();
    row.push_back(std::string(point.GetName()));
    row.push_back(absl::StrCat(summary.runs));
    row.push_back(FormatTime(summary.total));
    for (absl::Duration time :
         {summary.average, summary.p50, summary.p90, summary.p99, summary.max,
          summary.start_delay, summary.max_start_delay}) {
      row.push_back(has_runs ? FormatTime(time) : std::string(kNotApplicable));
    }
    if (has_resumes) {
      const bool has_resumed = summary.resumes > 0;
      row.push_back(absl::StrCat(summary.resumes));
      for (absl::Duration time :
           {summary.resume_delay, summary.max_resume_delay}) {
        row.push_back(has_resumed ? FormatTime(time)
                                  : std::string(kNotApplicable));
      }
    }
  }
  if (table.size() == 1) {
    return;
  }
  absl::StrAppend(&report, "\n");
  AppendTable(report, table, 1);
}

void Profiler::AppendFrame(std::string& report, std::string_view title,
                           const FrameBreakdown& frame,
                           absl::Span<const ProfilePoint> registered) const {
  absl::StrAppend(
      &report, title, ": ", FormatTime(TicksToDuration(frame.ticks)), ", core ",
      FormatCore(frame.core), ", profiler ",
      FormatTime(TicksToDuration(static_cast<double>(frame.timed_points) *
                                 point_cost_ticks_)),
      "\n");

  // The frame's points, each with its part in the frame, slowest first.
  std::vector<std::pair<ProfilePoint, FrameBreakdown::Point>> points;
  points.reserve(frame.points.size());
  for (const FrameBreakdown::Point& frame_point : frame.points) {
    points.emplace_back(registered[frame_point.index], frame_point);
  }
  absl::c_sort(points, [](const auto& a, const auto& b) {
    if (a.second.self_ticks != b.second.self_ticks) {
      return a.second.self_ticks > b.second.self_ticks;
    }
    return IsBeforeInReport(a.first, b.first);
  });

  // The frame's own self times of the kinds, and its wait, add up to the
  // frame's time. Only one frame is in progress, so nothing outside it has
  // frame time. Jobs have a row only in frames that ran one, as most programs
  // have none, and waits only in frames where a point in the frame waited, or
  // with a column alongside it, where any point waited.
  int64_t wait_ticks = 0;
  int64_t other_wait_ticks = 0;
  for (const FrameBreakdown::Point& frame_point : frame.points) {
    wait_ticks += frame_point.wait_ticks;
    other_wait_ticks += frame_point.other_wait_ticks;
  }
  const bool has_waits = wait_ticks > 0;
  Table kinds = {{"Kind", "Self"}};
  if (frame.has_joined) {
    kinds[0] = {"Kind", "In frame", "Alongside"};
  }
  for (ProfilePoint::Kind kind : kTimedKinds) {
    bool has_kind = false;
    int64_t self_ticks = 0;
    int64_t other_self_ticks = 0;
    for (const auto& [point, frame_point] : points) {
      if (point.GetKind() == kind) {
        has_kind = true;
        self_ticks += frame_point.self_ticks;
        other_self_ticks += frame_point.other_self_ticks;
      }
    }
    if (kind == ProfilePoint::Kind::kJob && !has_kind) {
      continue;
    }
    std::vector<std::string>& row = kinds.emplace_back();
    row.push_back(std::string(GetKindName(kind)));
    row.push_back(FormatTime(TicksToDuration(self_ticks - other_self_ticks)));
    if (frame.has_joined) {
      row.push_back(kind == ProfilePoint::Kind::kFrame
                        ? std::string(kNotApplicable)
                        : FormatTime(TicksToDuration(other_self_ticks)));
    }
  }
  const int64_t own_wait_ticks = wait_ticks - other_wait_ticks;
  if (frame.has_joined ? has_waits : own_wait_ticks > 0) {
    std::vector<std::string>& row = kinds.emplace_back();
    row.push_back("wait");
    row.push_back(FormatTime(TicksToDuration(own_wait_ticks)));
    if (frame.has_joined) {
      row.push_back(FormatTime(TicksToDuration(other_wait_ticks)));
    }
  }
  AppendTable(report, kinds, 1);
  absl::StrAppend(&report, "\n");

  Table table = {{"Point", "Kind", "Count", "Self"}};
  if (has_waits) {
    table[0].push_back("Wait");
  }
  for (const auto& [point, frame_point] : points) {
    const ProfilePoint::Kind kind = point.GetKind();
    std::vector<std::string>& row = table.emplace_back();
    row.push_back(std::string(point.GetName()));
    row.push_back(std::string(GetKindName(kind)));
    row.push_back(absl::StrCat(frame_point.count));
    row.push_back(
        FormatPointTime(kind, TicksToDuration(frame_point.self_ticks)));
    if (has_waits) {
      row.push_back(
          FormatPointTime(kind, TicksToDuration(frame_point.wait_ticks)));
    }
  }
  AppendTable(report, table, 2);
}

int Profiler::FindIndex(std::string_view name,
                        absl::Span<const ProfilePoint::Kind> kinds) const {
  const std::optional<ProfilePoint> point = ProfilePoint::Find(name);
  if (!point.has_value()) {
    return -1;
  }
  CHECK(absl::c_linear_search(kinds, point->GetKind()))
      << "Profile point \"" << name << "\" has the wrong kind";
  return point->GetIndex();
}

std::vector<Profiler::PointTotals> Profiler::GatherPointTotals() const {
  std::vector<PointTotals> totals = GatherOwnTotals();
  absl::MutexLock lock(&joined_mutex_);
  for (const JoinedThread& thread : joined_threads_) {
    // Points may be registered while this runs, so the totals of threads that
    // have left can cover more points than `totals` does, or fewer.
    const int count = std::min(static_cast<int>(totals.size()),
                               static_cast<int>(thread.folded.size()));
    for (int i = 0; i < count; ++i) {
      AddPointTotals(totals[i], thread.folded[i]);
    }
    if (thread.profiler != nullptr) {
      thread.profiler->AddOwnTotals(absl::MakeSpan(totals), thread.baseline);
    }
  }
  return totals;
}

std::vector<Profiler::PointTotals> Profiler::GatherOwnTotals() const {
  std::vector<PointTotals> totals(ProfilePoint::GetRegisteredCount());
  AddOwnTotals(absl::MakeSpan(totals), {});
  return totals;
}

void Profiler::AddOwnTotals(absl::Span<PointTotals> totals,
                            absl::Span<const PointTotals> baseline) const {
  const int baseline_count = static_cast<int>(baseline.size());
  for (int i = 0; i < static_cast<int>(totals.size()); ++i) {
    const Slot& slot = slots_[i];
    PointTotals own = {.count = slot.count.Get(),
                       .self_ticks = slot.self_ticks.Get(),
                       .value = slot.value.Get(),
                       .value_ticks = slot.value_ticks.Get(),
                       .wait_ticks = slot.wait_ticks.Get()};

    // Points registered since the baseline was gathered have none.
    if (i < baseline_count) {
      own.count -= baseline[i].count;
      own.self_ticks -= baseline[i].self_ticks;
      own.wait_ticks -= baseline[i].wait_ticks;
    }
    AddPointTotals(totals[i], own);
  }
}

void Profiler::AddPointTotals(PointTotals& totals, const PointTotals& thread) {
  // Setting a value counts, so a value set before the baseline was gathered
  // has a count of zero, and is left out.
  if (thread.count > 0 && thread.value_ticks >= totals.value_ticks) {
    totals.value = thread.value;
    totals.value_ticks = thread.value_ticks;
  }
  totals.count += thread.count;
  totals.self_ticks += thread.self_ticks;
  totals.wait_ticks += thread.wait_ticks;
}

std::vector<Profiler::JobTotals> Profiler::GatherJobTotals() const {
  std::vector<JobTotals> totals(ProfilePoint::GetRegisteredCount());
  absl::MutexLock lock(&joined_mutex_);
  const int64_t resets = frame_profiler_->numbers_.resets.Get();
  AddOwnJobTotals(resets, absl::MakeSpan(totals));
  for (const JoinedThread& thread : joined_threads_) {
    // As in GatherPointTotals(), the threads that have left can cover more
    // points, or fewer.
    const int count = std::min(static_cast<int>(totals.size()),
                               static_cast<int>(thread.folded_jobs.size()));
    for (int i = 0; i < count; ++i) {
      AddJobTotals(totals[i], thread.folded_jobs[i]);
    }
    if (thread.profiler != nullptr) {
      thread.profiler->AddOwnJobTotals(resets, absl::MakeSpan(totals));
    }
  }
  return totals;
}

void Profiler::AddOwnJobTotals(int64_t resets,
                               absl::Span<JobTotals> totals) const {
  for (int i = 0; i < static_cast<int>(totals.size()); ++i) {
    const JobStats* const stats =
        job_stats_[i].stats.load(std::memory_order_acquire);
    if (stats == nullptr || stats->resets.GetAcquire() != resets) {
      continue;
    }
    JobTotals& job = totals[i];
    job.run_ticks += stats->run_ticks.Get();
    job.max_run_ticks = std::max(job.max_run_ticks, stats->max_run_ticks.Get());
    job.start_delay_ticks += stats->start_delay_ticks.Get();
    job.max_start_delay_ticks =
        std::max(job.max_start_delay_ticks, stats->max_start_delay_ticks.Get());
    job.resumes += stats->resumes.Get();
    job.resume_delay_ticks += stats->resume_delay_ticks.Get();
    job.max_resume_delay_ticks = std::max(job.max_resume_delay_ticks,
                                          stats->max_resume_delay_ticks.Get());
    if (job.run_buckets.empty()) {
      job.run_buckets.resize(kHistogramBuckets);
    }
    for (int bucket = 0; bucket < kHistogramBuckets; ++bucket) {
      const int64_t runs = stats->run_buckets[bucket].Get();
      job.run_buckets[bucket] += runs;
      job.runs += runs;
    }
  }
}

void Profiler::AddJobTotals(JobTotals& totals, const JobTotals& thread) {
  totals.runs += thread.runs;
  totals.run_ticks += thread.run_ticks;
  totals.max_run_ticks = std::max(totals.max_run_ticks, thread.max_run_ticks);
  totals.start_delay_ticks += thread.start_delay_ticks;
  totals.max_start_delay_ticks =
      std::max(totals.max_start_delay_ticks, thread.max_start_delay_ticks);
  totals.resumes += thread.resumes;
  totals.resume_delay_ticks += thread.resume_delay_ticks;
  totals.max_resume_delay_ticks =
      std::max(totals.max_resume_delay_ticks, thread.max_resume_delay_ticks);
  if (thread.run_buckets.empty()) {
    return;
  }
  if (totals.run_buckets.empty()) {
    totals.run_buckets.resize(kHistogramBuckets);
  }
  for (int bucket = 0; bucket < kHistogramBuckets; ++bucket) {
    totals.run_buckets[bucket] += thread.run_buckets[bucket];
  }
}

Profiler::PointTotals Profiler::FindPointTotals(
    std::string_view name, absl::Span<const ProfilePoint::Kind> kinds) const {
  const int index = FindIndex(name, kinds);
  return index >= 0 ? GatherPointTotals()[index] : PointTotals();
}

Profiler::FrameBreakdown::Point Profiler::FindSlowestFramePoint(
    std::string_view name, absl::Span<const ProfilePoint::Kind> kinds) const {
  const int index = FindIndex(name, kinds);
  absl::MutexLock lock(&frame_mutex_);
  for (const FrameBreakdown::Point& point : frame_totals_.slowest.points) {
    if (point.index == index) {
      return point;
    }
  }
  return {};
}

int64_t Profiler::GetPercentileTicks(absl::Span<const int64_t> buckets,
                                     int64_t count, int64_t max_ticks,
                                     double fraction) {
  const int64_t target =
      std::max<int64_t>(1, static_cast<int64_t>(std::ceil(fraction * count)));
  int64_t counted = 0;
  for (int bucket = 0; bucket < static_cast<int>(buckets.size()); ++bucket) {
    counted += buckets[bucket];
    if (counted >= target) {
      return std::min(GetHistogramBucketMiddle(bucket), max_ticks);
    }
  }
  return max_ticks;
}

int Profiler::ReadCoreClass() const {
  const int core_class = fake_cpu_ != nullptr
                             ? fake_cpu_->GetThisThreadCoreClass()
                             : GetThisThreadCoreClass();
  return std::clamp(core_class, 0, kCoreClasses - 1);
}

std::string Profiler::FormatCore(int core) {
  if (core == kMixedCores) {
    return "mixed";
  }
  return absl::StrCat("class ", core);
}

absl::Duration Profiler::TicksToDuration(double ticks) const {
  return absl::Nanoseconds(ticks * 1e9 / ticks_per_second_);
}

}  // namespace gb
