// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "absl/log/check.h"
#include "gb/base/sleep.h"
#include "gb/base/win_platform.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb {
namespace {

// A high resolution waitable timer, closed when this is destroyed.
//
// A waitable timer has a single due time, so each thread needs its own.
class HighResolutionTimer {
 public:
  HighResolutionTimer()
      : handle_(CreateWaitableTimerExW(nullptr, nullptr,
                                       CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                       TIMER_MODIFY_STATE | SYNCHRONIZE)) {
    CHECK(handle_ != nullptr)
        << "High resolution timers need Windows 10 version 1803 or later: "
        << GetWindowsError();
  }
  HighResolutionTimer(const HighResolutionTimer&) = delete;
  HighResolutionTimer& operator=(const HighResolutionTimer&) = delete;
  ~HighResolutionTimer() { CloseHandle(handle_); }

  HANDLE GetHandle() const { return handle_; }

 private:
  const HANDLE handle_;
};

}  // namespace

void PreciseSleepFor(absl::Duration duration) {
  if (duration <= absl::ZeroDuration()) {
    return;
  }

  // Created on this thread's first sleep, and closed when the thread exits.
  thread_local HighResolutionTimer s_timer;
  const HANDLE timer = s_timer.GetHandle();

  // The due time is in 100 ns units, rounded up so the sleep is never shorter
  // than asked. SetWaitableTimer takes a negative due time as relative to now
  // (a positive one is an absolute time).
  const absl::Duration kUnit = absl::Nanoseconds(100);
  LARGE_INTEGER due_time;
  due_time.QuadPart = -(absl::Ceil(duration, kUnit) / kUnit);
  CHECK(SetWaitableTimer(timer, &due_time, 0, nullptr, nullptr, FALSE))
      << "SetWaitableTimer failed: " << GetWindowsError();
  CHECK(WaitForSingleObject(timer, INFINITE) == WAIT_OBJECT_0)
      << "WaitForSingleObject failed: " << GetWindowsError();
}

}  // namespace gb
