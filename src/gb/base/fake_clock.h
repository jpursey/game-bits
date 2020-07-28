#ifndef GB_BASE_FAKE_CLOCK_H_
#define GB_BASE_FAKE_CLOCK_H_

#include "gb/base/clock.h"

namespace gb {

class FakeClock : public Clock {
 public:
  FakeClock() = default;
  ~FakeClock() override = default;

  absl::Time Now() override {
    now_ += auto_advance_;
    return now_;
  }
  void SleepFor(absl::Duration duration) override { now_ += duration + sleep_offset_; }

  absl::Time GetTime() { return now_; }
  void SetTime(absl::Time now) { now_ = now; }
  void AdvanceTime(absl::Duration duration) { now_ += duration; }
  void SetAutoAdvance(absl::Duration auto_advance) {
    auto_advance_ = auto_advance;
  }
  void SetSleepOffset(absl::Duration sleep_offset) {
    sleep_offset_ = sleep_offset;
  }

 private:
  absl::Time now_;
  absl::Duration auto_advance_;
  absl::Duration sleep_offset_;
};

}  // namespace gb

#endif  // GB_BASE_FAKE_CLOCK_H_
