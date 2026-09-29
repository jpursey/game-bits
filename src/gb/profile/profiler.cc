// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profiler.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <optional>

#include "absl/log/check.h"

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

int64_t Profiler::GetCount(std::string_view name) const {
  const Slot* slot =
      FindSlot(name, {ProfilePoint::Kind::kScope, ProfilePoint::Kind::kCall,
                      ProfilePoint::Kind::kCounter});
  return slot != nullptr ? slot->count : 0;
}

absl::Duration Profiler::GetSelfTime(std::string_view name) const {
  const Slot* slot =
      FindSlot(name, {ProfilePoint::Kind::kScope, ProfilePoint::Kind::kCall});
  return slot != nullptr ? TicksToDuration(slot->self_ticks)
                         : absl::ZeroDuration();
}

int64_t Profiler::GetValue(std::string_view name) const {
  const Slot* slot = FindSlot(name, {ProfilePoint::Kind::kValue});
  return slot != nullptr ? slot->value : 0;
}

void Profiler::Reset() {
  CHECK(top_ == nullptr) << "A Profiler can't be reset in a timed point";
  std::fill_n(slots_.get(), kMaxProfilePoints, Slot());
}

const Profiler::Slot* Profiler::FindSlot(
    std::string_view name,
    std::initializer_list<ProfilePoint::Kind> kinds) const {
  const std::optional<ProfilePoint> point = ProfilePoint::Find(name);
  if (!point.has_value()) {
    return nullptr;
  }
  CHECK(std::find(kinds.begin(), kinds.end(), point->GetKind()) != kinds.end())
      << "Profile point \"" << name << "\" has the wrong kind";
  return &slots_[point->GetIndex()];
}

absl::Duration Profiler::TicksToDuration(int64_t ticks) const {
  return absl::Nanoseconds(static_cast<double>(ticks) * 1e9 /
                           ticks_per_second_);
}

}  // namespace gb
