#ifndef GBITS_BASE_CLOCK_H_
#define GBITS_BASE_CLOCK_H_

#include "absl/time/clock.h"

namespace gb {

class Clock {
 public:
  Clock(const Clock&) = delete;
  Clock& operator=(const Clock&) = delete;
  virtual ~Clock() = default;

  // Returns the current time.
  virtual absl::Time Now() = 0;

  // Sleeps the current thread for the specified duration.
  virtual void SleepFor(absl::Duration duration) = 0;

 protected:
  Clock() = default;
};

class RealtimeClock : public Clock {
 public:
  RealtimeClock() = default;
  ~RealtimeClock() override = default;

  static Clock* GetClock() {
    static RealtimeClock clock;
    return &clock;
  }

  absl::Time Now() override { return absl::Now(); }
  void SleepFor(absl::Duration duration) override { absl::SleepFor(duration); }
};

}  // namespace gb

#endif  // GBITS_BASE_CLOCK_H_
