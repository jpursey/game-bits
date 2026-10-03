// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profiler.h"

#ifndef _MSC_VER
#include <cpuid.h>
#endif

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "gb/profile/cpu_info.h"

namespace gb {

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
    const int64_t before = static_cast<int64_t>(__rdtsc());
    const SteadyClock::time_point time = SteadyClock::now();
    const int64_t after = static_cast<int64_t>(__rdtsc());
    if (after - before < best_spread) {
      best_spread = after - before;
      best = {before + (after - before) / 2, time};
    }
  }
  return best;
}

// Measures how fast the CPU's timestamp counter runs, against the steady clock
// (which, unlike the wall clock, is never adjusted).
double MeasureTicksPerSecond() {
  const ClockPair start = ReadClockPair();
  ClockPair end;
  do {
    end = ReadClockPair();
  } while (end.time - start.time < kCalibrationTime);
  return static_cast<double>(end.ticks - start.ticks) /
         std::chrono::duration<double>(end.time - start.time).count();
}

// Returns how fast the CPU's timestamp counter runs, which is the same for the
// whole program as the counter is invariant.
double GetTicksPerSecond() {
  static const double s_ticks_per_second = MeasureTicksPerSecond();
  return s_ticks_per_second;
}

// Returns how fast the CPU says its timestamp counter runs, or 0 if it doesn't
// say. CPUID leaf 0x15 gives the rate as a ratio (EBX / EAX) of the core
// crystal clock (ECX, in Hz), each 0 if the CPU doesn't report it.
double GetCpuTicksPerSecond() {
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

// How the cost of a timed point is measured: this many rounds of this many
// points, keeping the fastest round.
constexpr int kCostRounds = 5;
constexpr int kCostPoints = 100;

// The kinds of point each reader accepts.
constexpr ProfilePoint::Kind kCountedKinds[] = {
    ProfilePoint::Kind::kFrame, ProfilePoint::Kind::kScope,
    ProfilePoint::Kind::kCall, ProfilePoint::Kind::kCounter};
constexpr ProfilePoint::Kind kTimedKinds[] = {ProfilePoint::Kind::kFrame,
                                              ProfilePoint::Kind::kScope,
                                              ProfilePoint::Kind::kCall};
constexpr ProfilePoint::Kind kValueKinds[] = {ProfilePoint::Kind::kValue};

// Returns the histogram bucket for a frame of `ticks`. Buckets 0 to 7 hold
// exactly 0 to 7 ticks. Above that, the bucket comes from the position of the
// highest set bit and the three bits below it.
int GetFrameBucket(int64_t ticks) {
  if (ticks < 8) {
    return static_cast<int>(std::max<int64_t>(ticks, 0));
  }
  const int high_bit = std::bit_width(static_cast<uint64_t>(ticks)) - 1;
  const int next_bits = static_cast<int>((ticks >> (high_bit - 3)) & 7);
  return (high_bit - 2) * 8 + next_bits;
}

// Returns the middle of the range of ticks in `bucket`.
int64_t GetFrameBucketMiddle(int bucket) {
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
    case ProfilePoint::Kind::kCounter:
      return "counter";
    case ProfilePoint::Kind::kValue:
      return "value";
  }
  return "unknown";
}

// Formats a point's self time, which only a timed point has.
std::string FormatSelfTime(ProfilePoint::Kind kind, absl::Duration self_time) {
  if (!absl::c_linear_search(kTimedKinds, kind)) {
    return std::string(kNotApplicable);
  }
  return FormatTime(self_time);
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
    : fake_ticks_(options.fake_ticks),
      slots_(std::make_unique<Slot[]>(kMaxProfilePoints)),
      ticks_per_second_(
          fake_ticks_ != nullptr
              ? static_cast<double>(fake_ticks_->GetTicksPerSecond())
              : GetTicksPerSecond()),
      cpu_ticks_per_second_(
          fake_ticks_ != nullptr
              ? static_cast<double>(fake_ticks_->GetCpuTicksPerSecond())
              : GetCpuTicksPerSecond()),
      budget_per_frame_(options.budget_per_frame),
      budget_fraction_(options.budget_fraction),
      slow_frame_ticks_(
          options.slow_frame == absl::InfiniteDuration()
              ? std::numeric_limits<int64_t>::max()
              : static_cast<int64_t>(absl::ToDoubleSeconds(options.slow_frame) *
                                     ticks_per_second_)),
      on_slow_frame_(std::move(options.on_slow_frame)) {
  CHECK(s_current == nullptr) << "Only one Profiler may exist per thread";
  s_current = this;

  // The first read of the core's class finds every core's class, which is
  // done here rather than in the first frame.
  if (fake_ticks_ == nullptr) {
    internal::GetCoreClass();
  }

  // Measuring times points into the slots, which are then cleared.
  point_cost_ticks_ = MeasurePointCost();
  Reset();
}

Profiler::~Profiler() {
  CHECK(s_current == this)
      << "A Profiler must be destroyed on the thread that created it";
  CHECK(top_ == nullptr) << "A Profiler must not be destroyed in a timed point";
  s_current = nullptr;
}

absl::Duration Profiler::GetPointCost() const {
  return TicksToDuration(point_cost_ticks_);
}

Profiler::FrameSummary Profiler::GetFrameSummary() const {
  if (frames_ == 0) {
    return {};
  }
  const absl::Duration total = TicksToDuration(total_frame_ticks_);
  const absl::Duration average = total / frames_;
  const bool has_budget =
      budget_per_frame_ != absl::ZeroDuration() || budget_fraction_ != 0;
  return {
      .frames = frames_,
      .total = total,
      .average = average,
      .p50 = TicksToDuration(GetFramePercentileTicks(0.5)),
      .p90 = TicksToDuration(GetFramePercentileTicks(0.9)),
      .p99 = TicksToDuration(GetFramePercentileTicks(0.99)),
      .max = TicksToDuration(slowest_frame_.ticks),
      .profiler_cost =
          TicksToDuration(static_cast<double>(frame_timed_points_) *
                          point_cost_ticks_ / frames_),
      .budget = has_budget
                    ? std::max(budget_per_frame_, average * budget_fraction_)
                    : absl::InfiniteDuration(),
  };
}

int64_t Profiler::GetCount(std::string_view name) const {
  const Slot* slot = FindSlot(name, kCountedKinds);
  return slot != nullptr ? slot->count : 0;
}

absl::Duration Profiler::GetSelfTime(std::string_view name) const {
  const Slot* slot = FindSlot(name, kTimedKinds);
  return slot != nullptr ? TicksToDuration(slot->self_ticks)
                         : absl::ZeroDuration();
}

int64_t Profiler::GetValue(std::string_view name) const {
  const Slot* slot = FindSlot(name, kValueKinds);
  return slot != nullptr ? slot->value : 0;
}

int64_t Profiler::GetSlowestFrameCount(std::string_view name) const {
  const FrameBreakdown::Point* point =
      FindSlowestFramePoint(name, kCountedKinds);
  return point != nullptr ? point->count : 0;
}

absl::Duration Profiler::GetSlowestFrameSelfTime(std::string_view name) const {
  const FrameBreakdown::Point* point = FindSlowestFramePoint(name, kTimedKinds);
  return point != nullptr ? TicksToDuration(point->self_ticks)
                          : absl::ZeroDuration();
}

void Profiler::Reset() {
  CHECK(top_ == nullptr) << "A Profiler can't be reset in a timed point";
  std::fill_n(slots_.get(), kMaxProfilePoints, Slot());
  frame_timed_points_ = 0;
  frames_ = 0;
  total_frame_ticks_ = 0;
  frame_buckets_ = {};
  core_frames_ = {};
  slowest_frame_ = {};
}

double Profiler::MeasurePointCost() {
  // The points are timed inside another, as most are, so each also adds its
  // time to its parent's.
  Timing outer;
  StartTiming(outer);
  int64_t fastest_ticks = std::numeric_limits<int64_t>::max();
  for (int round = 0; round < kCostRounds; ++round) {
    const int64_t start_ticks = ReadTicks();
    for (int i = 0; i < kCostPoints; ++i) {
      Timing timing;
      StartTiming(timing);
      EndTiming(0, timing);
    }
    fastest_ticks = std::min(fastest_ticks, ReadTicks() - start_ticks);
  }
  EndTiming(0, outer);
  return static_cast<double>(fastest_ticks) / kCostPoints;
}

void Profiler::StartFrame(Timing& timing) {
  CHECK(current_frame_ == 0) << "Profile frames can't nest";
  current_frame_ = frames_ + 1;
  current_frame_timed_points_ = 0;
  current_frame_start_core_ = ReadCoreClass();
  StartTiming(timing);
}

void Profiler::EndFrame(int index, Timing& timing) {
  const int64_t frame_ticks = EndTiming(index, timing);
  frame_timed_points_ += current_frame_timed_points_;
  ++frames_;
  total_frame_ticks_ += frame_ticks;
  ++frame_buckets_[GetFrameBucket(frame_ticks)];
  const int end_core = ReadCoreClass();
  const int core =
      end_core == current_frame_start_core_ ? end_core : kMixedCores;
  CoreFrames& core_frames = core_frames_[core];
  ++core_frames.frames;
  core_frames.ticks += frame_ticks;
  core_frames.max_ticks = std::max(core_frames.max_ticks, frame_ticks);
  const bool is_slowest = frame_ticks > slowest_frame_.ticks;
  if (is_slowest) {
    slowest_frame_ = CaptureFrame(frame_ticks, core);
  }
  std::string slow_frame_report;
  if (frame_ticks > slow_frame_ticks_ && on_slow_frame_ != nullptr) {
    FrameBreakdown slow_frame;
    if (!is_slowest) {
      slow_frame = CaptureFrame(frame_ticks, core);
    }
    AppendFrame(slow_frame_report, "Slow frame",
                is_slowest ? slowest_frame_ : slow_frame,
                ProfilePoint::GetRegisteredPoints());
  }
  current_frame_ = 0;

  // The callback is called after the frame ends, so any points it reaches are
  // outside the frame.
  if (!slow_frame_report.empty()) {
    on_slow_frame_(slow_frame_report);
  }
}

Profiler::FrameBreakdown Profiler::CaptureFrame(int64_t ticks, int core) const {
  FrameBreakdown frame = {.ticks = ticks,
                          .core = core,
                          .timed_points = current_frame_timed_points_};
  const int point_count = ProfilePoint::GetRegisteredCount();
  for (int i = 0; i < point_count; ++i) {
    const Slot& slot = slots_[i];
    if (slot.frame == current_frame_) {
      frame.points.push_back({i, slot.frame_count, slot.frame_self_ticks});
    }
  }
  return frame;
}

std::string Profiler::GetReport() const {
  const std::vector<ProfilePoint> registered =
      ProfilePoint::GetRegisteredPoints();
  std::vector<ProfilePoint> points = registered;
  absl::c_sort(points, IsBeforeInReport);
  std::string report;
  AppendValues(report, points);
  AppendFrameSummary(report);
  AppendCoreFrames(report);
  AppendPoints(report, points);
  if (frames_ > 0) {
    absl::StrAppend(&report, "\n");
    AppendFrame(report, "Slowest frame", slowest_frame_, registered);
  }
  return report;
}

void Profiler::AppendValues(std::string& report,
                            absl::Span<const ProfilePoint> points) const {
  Table table = {{"Value", "Latest"}};
  for (const ProfilePoint& point : points) {
    const Slot& slot = slots_[point.GetIndex()];
    if (point.GetKind() == ProfilePoint::Kind::kValue && slot.count > 0) {
      table.push_back({std::string(point.GetName()), absl::StrCat(slot.value)});
    }
  }
  if (table.size() > 1) {
    AppendTable(report, table, 1);
    absl::StrAppend(&report, "\n");
  }
}

void Profiler::AppendFrameSummary(std::string& report) const {
  const FrameSummary summary = GetFrameSummary();
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
  table.push_back(
      {"Tick rate",
       absl::StrCat(FormatTickRate(ticks_per_second_), " measured, ",
                    cpu_ticks_per_second_ > 0
                        ? FormatTickRate(cpu_ticks_per_second_)
                        : std::string("none"),
                    " reported")});
  AppendTable(report, table, 2);
}

void Profiler::AppendCoreFrames(std::string& report) const {
  if (frames_ == 0) {
    return;
  }
  Table table = {{"Core", "Frames", "Average", "Max"}};
  int cores = 0;
  for (int core = 0; core < static_cast<int>(core_frames_.size()); ++core) {
    const CoreFrames& core_frames = core_frames_[core];
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

void Profiler::AppendPoints(std::string& report,
                            absl::Span<const ProfilePoint> points) const {
  Table table = {{"Point", "Kind", "Count", "Count/frame", "Self", "Self/call",
                  "Self/frame"}};
  for (const ProfilePoint& point : points) {
    const Slot& slot = slots_[point.GetIndex()];
    if (point.GetKind() == ProfilePoint::Kind::kValue || slot.count == 0) {
      continue;
    }
    const ProfilePoint::Kind kind = point.GetKind();
    const absl::Duration self_time = TicksToDuration(slot.self_ticks);
    const bool has_frames = frames_ > 0;
    table.push_back(
        {std::string(point.GetName()), std::string(GetKindName(kind)),
         absl::StrCat(slot.count),
         has_frames ? FormatNumber(static_cast<double>(slot.count) / frames_)
                    : std::string(kNotApplicable),
         FormatSelfTime(kind, self_time),
         FormatSelfTime(kind, self_time / slot.count),
         has_frames ? FormatSelfTime(kind, self_time / frames_)
                    : std::string(kNotApplicable)});
  }
  absl::StrAppend(&report, "\n");
  AppendTable(report, table, 2);
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

  // The self times of the kinds add up to the frame's time.
  Table kinds = {{"Kind", "Self"}};
  for (ProfilePoint::Kind kind : kTimedKinds) {
    int64_t self_ticks = 0;
    for (const auto& [point, frame_point] : points) {
      if (point.GetKind() == kind) {
        self_ticks += frame_point.self_ticks;
      }
    }
    kinds.push_back({std::string(GetKindName(kind)),
                     FormatTime(TicksToDuration(self_ticks))});
  }
  AppendTable(report, kinds, 1);
  absl::StrAppend(&report, "\n");

  Table table = {{"Point", "Kind", "Count", "Self"}};
  for (const auto& [point, frame_point] : points) {
    table.push_back({std::string(point.GetName()),
                     std::string(GetKindName(point.GetKind())),
                     absl::StrCat(frame_point.count),
                     FormatSelfTime(point.GetKind(),
                                    TicksToDuration(frame_point.self_ticks))});
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

const Profiler::Slot* Profiler::FindSlot(
    std::string_view name, absl::Span<const ProfilePoint::Kind> kinds) const {
  const int index = FindIndex(name, kinds);
  return index >= 0 ? &slots_[index] : nullptr;
}

const Profiler::FrameBreakdown::Point* Profiler::FindSlowestFramePoint(
    std::string_view name, absl::Span<const ProfilePoint::Kind> kinds) const {
  const int index = FindIndex(name, kinds);
  for (const FrameBreakdown::Point& point : slowest_frame_.points) {
    if (point.index == index) {
      return &point;
    }
  }
  return nullptr;
}

int64_t Profiler::GetFramePercentileTicks(double fraction) const {
  const int64_t target =
      std::max<int64_t>(1, static_cast<int64_t>(std::ceil(fraction * frames_)));
  int64_t frames = 0;
  for (int bucket = 0; bucket < kFrameBuckets; ++bucket) {
    frames += frame_buckets_[bucket];
    if (frames >= target) {
      return std::min(GetFrameBucketMiddle(bucket), slowest_frame_.ticks);
    }
  }
  return slowest_frame_.ticks;
}

int Profiler::ReadCoreClass() const {
  const int core_class = fake_ticks_ != nullptr ? fake_ticks_->GetCoreClass()
                                                : internal::GetCoreClass();
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
