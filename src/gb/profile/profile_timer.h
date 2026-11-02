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
// timer is created until it is destroyed. Job points are timed by
// ProfileJobTimer.
//
// The point is charged with its self time: its elapsed time less that of the
// timers created inside it.
//
// A timer must be destroyed on the fiber it was created on, and timers must be
// destroyed in the reverse order they were created (as scoped variables are).
// Destroying a timer that isn't the innermost on its fiber CHECK-fails.
//
// Code on fibers, such as jobs, can keep a timer across a switch to another
// fiber, such as a wait, even if it resumes on another thread. It times only
// what its fiber ran, all recorded in the Profiler of the thread it is
// destroyed on, as Profiler describes.
//
// Timing a point costs two reads of the CPU's timestamp counter, and a few
// adds. With no Profiler on the thread, it costs about a nanosecond.
class ProfileTimer final {
 public:
  [[nodiscard]] explicit ProfileTimer(const ProfilePoint& point) {
    Profiler* const profiler = Profiler::GetCurrent();
    if (profiler == nullptr) {
      return;
    }
    DCHECK(point.GetKind() == ProfilePoint::Kind::kScope ||
           point.GetKind() == ProfilePoint::Kind::kCall)
        << point.GetName() << " is not a scope or call";
    profiler->StartTiming(point.GetIndex(), timing_);
  }
  ProfileTimer(const ProfileTimer&) = delete;
  ProfileTimer& operator=(const ProfileTimer&) = delete;

  // This ends the timing in its own Profiler, which a fiber switch updates,
  // rather than reading the thread's again, as the compiler may cache a
  // thread_local's address across a fiber switch. That Profiler then CHECKs
  // that this is its innermost timing.
  ~ProfileTimer() {
    if (timing_.profiler == nullptr) {
      return;
    }
    timing_.profiler->EndTiming(timing_);
  }

 private:
  Profiler::Timing timing_;
};

//==============================================================================
// ProfileJobTimer
//==============================================================================

// Times a job point in the calling thread's Profiler, from when the timer is
// created until it is destroyed. A job system (see gb/job) puts one around each
// job it runs.
//
// `queued_ticks` is Profiler::ReadTicks() from when the job was queued, read
// through the calling thread's Profiler, its main Profiler, or another Profiler
// joined to that, and the job's start delay is from then until the timer is
// created. If the job suspends to wait, the job system adds its resume delay
// with Profiler::AddResumeDelay().
//
// The point is charged with its self time, as by ProfileTimer, and also
// records the job's run time and delays (see Profiler::GetJobSummary()), in
// the Profiler of the thread the timer is destroyed on. It follows the same
// rules as ProfileTimer, and costs a little more: a histogram add, and a few
// adds for the delays.
class ProfileJobTimer final {
 public:
  [[nodiscard]] ProfileJobTimer(const ProfilePoint& job, int64_t queued_ticks) {
    Profiler* const profiler = Profiler::GetCurrent();
    if (profiler == nullptr) {
      return;
    }
    DCHECK(job.GetKind() == ProfilePoint::Kind::kJob)
        << job.GetName() << " is not a job";
    profiler->StartJob(job.GetIndex(), queued_ticks, job_);
  }
  ProfileJobTimer(const ProfileJobTimer&) = delete;
  ProfileJobTimer& operator=(const ProfileJobTimer&) = delete;

  // As ~ProfileTimer().
  ~ProfileJobTimer() {
    if (job_.timing.profiler == nullptr) {
      return;
    }
    job_.timing.profiler->EndJob(job_);
  }

 private:
  Profiler::JobTiming job_;
};

//==============================================================================
// ProfileIdleTimer
//==============================================================================

// Times the calling thread waiting for work, as its idle time, from when the
// timer is created until it is destroyed. A job system (see gb/job) puts one
// around each job thread's wait for a job to run.
//
// The idle time of a thread whose Profiler is joined to a main Profiler is in
// the main Profiler's threads table (see Profiler::GetReport()). Other threads'
// idle time isn't reported. With no Profiler on the thread, the timer does
// nothing.
//
// The timer must be outside every timed point on its thread, and can't nest,
// span a fiber switch, or outlive the thread's Profiler, and on a joined
// Profiler's thread no timed point can start inside it (CHECK). Timing a wait
// costs two reads of the CPU's timestamp counter, and a few stores.
class ProfileIdleTimer final {
 public:
  [[nodiscard]] ProfileIdleTimer() {
    Profiler* const profiler = Profiler::GetCurrent();
    if (profiler == nullptr) {
      return;
    }
    profiler->StartIdle();
    profiler_ = profiler;
  }
  ProfileIdleTimer(const ProfileIdleTimer&) = delete;
  ProfileIdleTimer& operator=(const ProfileIdleTimer&) = delete;
  ~ProfileIdleTimer() {
    if (profiler_ == nullptr) {
      return;
    }
    profiler_->EndIdle();
  }

 private:
  Profiler* profiler_ = nullptr;
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
//
// Frames are timed on the main Profiler's thread, where a frame can't span a
// fiber switch (CHECK). For a threadless main Profiler (see
// Profiler::Options::threadless), they are timed on any of its joined
// Profilers' threads, one frame at a time (CHECK), and a frame can span fiber
// switches, so it can wait on other work, and end on another thread. Its time
// then includes the time its fiber was suspended. Its fiber must only resume on
// threads joined to the same main Profiler (CHECK).
template <ProfileName kName>
class ProfileFrame final {
 public:
  [[nodiscard]] ProfileFrame() {
    const ProfilePoint& point =
        internal::GetNamedPoint<ProfilePoint::Kind::kFrame, kName>();
    Profiler* const profiler = Profiler::GetCurrent();
    if (profiler == nullptr) {
      return;
    }
    profiler->StartFrame(point.GetIndex(), timing_);
  }
  ProfileFrame(const ProfileFrame&) = delete;
  ProfileFrame& operator=(const ProfileFrame&) = delete;
  ~ProfileFrame() {
    if (timing_.profiler == nullptr) {
      return;
    }
    timing_.profiler->EndFrame(timing_);
  }

 private:
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
