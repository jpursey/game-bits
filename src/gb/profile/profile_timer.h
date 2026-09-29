// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_PROFILE_TIMER_H_
#define GB_PROFILE_PROFILE_TIMER_H_

#include <cstdint>

#include "absl/log/check.h"
#include "gb/profile/profile_point.h"
#include "gb/profile/profiler.h"

namespace gb {

namespace internal {
template <ProfilePoint::Kind kKind, ProfileName kName>
class NamedProfileTimer;
}  // namespace internal

//==============================================================================
// ProfileTimer
//==============================================================================

// Times a scope or call point in the calling thread's Profiler, from when the
// timer is created until it is destroyed.
//
// The point is charged with its self time: its elapsed time less that of the
// timers created inside it.
//
// A timer must be destroyed on the thread and fiber it was created on, with no
// switch to another fiber in between, and timers must be destroyed in the
// reverse order they were created (as scoped variables are). Destroying a timer
// that isn't the innermost on its thread CHECK-fails.
//
// Timing a point costs two reads of the CPU's timestamp counter, and a few
// adds. With no Profiler on the thread, it costs about a nanosecond.
class ProfileTimer final {
 public:
  [[nodiscard]] explicit ProfileTimer(const ProfilePoint& point)
      : profiler_(Profiler::s_current), index_(point.GetIndex()) {
    if (profiler_ == nullptr) {
      return;
    }
    DCHECK(point.GetKind() == ProfilePoint::Kind::kScope ||
           point.GetKind() == ProfilePoint::Kind::kCall)
        << point.GetName() << " is not a scope or a call";
    profiler_->StartTiming(timing_);
  }
  ProfileTimer(const ProfileTimer&) = delete;
  ProfileTimer& operator=(const ProfileTimer&) = delete;

  // This ends the timing in the Profiler the timer started with, rather than
  // reading the thread's again, as the compiler may cache a thread_local's
  // address across a fiber switch. That Profiler then CHECKs that this is its
  // innermost timing.
  ~ProfileTimer() {
    if (profiler_ == nullptr) {
      return;
    }
    profiler_->EndTiming(index_, timing_);
  }

 private:
  Profiler* const profiler_;
  const int index_;
  Profiler::Timing timing_;
};

//==============================================================================
// Named timers
//==============================================================================

// Times the scope point named `kName` for the lifetime of the ProfileScope:
//
//   void TrackCache::Refresh() {
//     ProfileScope<"TrackCache::Refresh"> scope;
//     ...
//   }
//
// See ProfileTimer for the rules timers must follow.
template <ProfileName kName>
using ProfileScope =
    internal::NamedProfileTimer<ProfilePoint::Kind::kScope, kName>;

// Times the call point named `kName` for the lifetime of the ProfileCall, which
// is placed around a call out to another system:
//
//   {
//     ProfileCall<"reaper/GetTrack"> call;
//     track = GetTrack(project, index);
//   }
//
// See ProfileTimer for the rules timers must follow.
template <ProfileName kName>
using ProfileCall =
    internal::NamedProfileTimer<ProfilePoint::Kind::kCall, kName>;

//==============================================================================
// ProfileFrame
//==============================================================================

// Times one frame (one iteration of the program's loop) as the frame point
// named `kName`, for the lifetime of the ProfileFrame:
//
//   void Surface::Run() {
//     ProfileFrame<"Run"> frame;
//     ...
//   }
//
// A frame is a timed point like a scope, charged with its self time, that also
// records the frame as a whole (see Profiler::GetFrameSummary). Frames can't
// nest (CHECK), and follow the same rules as other timers (see ProfileTimer).
template <ProfileName kName>
class ProfileFrame final {
 public:
  [[nodiscard]] ProfileFrame()
      : profiler_(Profiler::s_current),
        index_(internal::GetNamedPoint<ProfilePoint::Kind::kFrame, kName>()
                   .GetIndex()) {
    if (profiler_ == nullptr) {
      return;
    }
    profiler_->StartFrame(timing_);
  }
  ProfileFrame(const ProfileFrame&) = delete;
  ProfileFrame& operator=(const ProfileFrame&) = delete;
  ~ProfileFrame() {
    if (profiler_ == nullptr) {
      return;
    }
    profiler_->EndFrame(index_, timing_);
  }

 private:
  Profiler* const profiler_;
  const int index_;
  Profiler::Timing timing_;
};

//==============================================================================
// Implementation
//==============================================================================

namespace internal {

template <ProfilePoint::Kind kKind, ProfileName kName>
class NamedProfileTimer final {
 public:
  [[nodiscard]] NamedProfileTimer() : timer_(GetNamedPoint<kKind, kName>()) {}
  NamedProfileTimer(const NamedProfileTimer&) = delete;
  NamedProfileTimer& operator=(const NamedProfileTimer&) = delete;
  ~NamedProfileTimer() = default;

 private:
  ProfileTimer timer_;
};

}  // namespace internal
}  // namespace gb

#endif  // GB_PROFILE_PROFILE_TIMER_H_
