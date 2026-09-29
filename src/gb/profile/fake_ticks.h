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
// The ticks start at zero and only change when the test advances them.
class FakeTicks {
 public:
  // Every tick is a nanosecond by default.
  explicit FakeTicks(int64_t ticks_per_second = 1'000'000'000)
      : ticks_per_second_(ticks_per_second) {}
  FakeTicks(const FakeTicks&) = delete;
  FakeTicks& operator=(const FakeTicks&) = delete;
  ~FakeTicks() = default;

  int64_t GetTicksPerSecond() const { return ticks_per_second_; }

  // Returns the current ticks.
  int64_t Now() const { return ticks_; }

  void Advance(int64_t ticks) { ticks_ += ticks; }

 private:
  const int64_t ticks_per_second_;
  int64_t ticks_ = 0;
};

}  // namespace gb

#endif  // GB_PROFILE_FAKE_TICKS_H_
