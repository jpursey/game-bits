// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profiler.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>

#include "absl/log/check.h"
#include "absl/types/span.h"

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

}  // namespace

Profiler::Profiler() : Profiler(Options()) {}

Profiler::Profiler(Options options)
    : fake_ticks_(options.fake_ticks),
      ticks_per_second_(
          fake_ticks_ != nullptr
              ? static_cast<double>(fake_ticks_->GetTicksPerSecond())
              : GetTicksPerSecond()),
      slots_(std::make_unique<Slot[]>(kMaxProfilePoints)) {
  CHECK(s_current == nullptr) << "Only one Profiler may exist per thread";
  s_current = this;
}

Profiler::~Profiler() {
  CHECK(s_current == this)
      << "A Profiler must be destroyed on the thread that created it";
  CHECK(top_ == nullptr) << "A Profiler must not be destroyed in a timed point";
  s_current = nullptr;
}

Profiler::FrameSummary Profiler::GetFrameSummary() const {
  if (frames_ == 0) {
    return {};
  }
  return {
      .frames = frames_,
      .total = TicksToDuration(total_frame_ticks_),
      .average = TicksToDuration(total_frame_ticks_) / frames_,
      .p50 = TicksToDuration(GetFramePercentileTicks(0.5)),
      .p90 = TicksToDuration(GetFramePercentileTicks(0.9)),
      .p99 = TicksToDuration(GetFramePercentileTicks(0.99)),
      .max = TicksToDuration(slowest_frame_.ticks),
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
  frames_ = 0;
  total_frame_ticks_ = 0;
  frame_buckets_ = {};
  slowest_frame_ = {};
}

void Profiler::StartFrame(Timing& timing) {
  CHECK(current_frame_ == 0) << "Profile frames can't nest";
  current_frame_ = frames_ + 1;
  StartTiming(timing);
}

void Profiler::EndFrame(int index, Timing& timing) {
  const int64_t frame_ticks = EndTiming(index, timing);
  ++frames_;
  total_frame_ticks_ += frame_ticks;
  ++frame_buckets_[GetFrameBucket(frame_ticks)];
  if (frame_ticks > slowest_frame_.ticks) {
    slowest_frame_ = CaptureFrame(frame_ticks);
  }
  current_frame_ = 0;
}

Profiler::FrameBreakdown Profiler::CaptureFrame(int64_t ticks) const {
  FrameBreakdown frame = {.ticks = ticks};
  const int point_count = ProfilePoint::GetRegisteredCount();
  for (int i = 0; i < point_count; ++i) {
    const Slot& slot = slots_[i];
    if (slot.frame == current_frame_) {
      frame.points.push_back({i, slot.frame_count, slot.frame_self_ticks});
    }
  }
  return frame;
}

int Profiler::FindIndex(std::string_view name,
                        absl::Span<const ProfilePoint::Kind> kinds) const {
  const std::optional<ProfilePoint> point = ProfilePoint::Find(name);
  if (!point.has_value()) {
    return -1;
  }
  CHECK(std::find(kinds.begin(), kinds.end(), point->GetKind()) != kinds.end())
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

absl::Duration Profiler::TicksToDuration(int64_t ticks) const {
  return absl::Nanoseconds(static_cast<double>(ticks) * 1e9 /
                           ticks_per_second_);
}

}  // namespace gb
