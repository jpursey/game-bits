// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_PROFILE_CALL_HOOK_H_
#define GB_PROFILE_PROFILE_CALL_HOOK_H_

#include <string_view>
#include <utility>

#include "absl/log/check.h"
#include "gb/profile/profile_point.h"
#include "gb/profile/profile_timer.h"

namespace gb {

//==============================================================================
// ProfileCallHook
//==============================================================================

// A Hook for FunctionHook (see gb/base/function_hook.h) that times every call
// through the hooked pointer as the call point named when it is installed:
//
//   FunctionHook<&GetTrack, ProfileCallHook> hook("reaper/GetTrack");
//
// Each call is timed in the calling thread's Profiler, if any, and follows the
// rules of ProfileTimer. With no Profiler on the thread, it is only called
// through.
//
// Only install it over a function that was loaded. Installing any FunctionHook
// makes the pointer non-null, so code that checks whether the function exists
// would find it, and this hook can't stand in for it.
class ProfileCallHook final {
 public:
  // Registers the call point named `name`.
  explicit ProfileCallHook(std::string_view name)
      : point_(ProfilePoint::Kind::kCall, name) {}
  ProfileCallHook(const ProfileCallHook&) = delete;
  ProfileCallHook& operator=(const ProfileCallHook&) = delete;
  ~ProfileCallHook() = default;

  // Calls `original`, timing it as the call point.
  template <typename Function, typename... Args>
  auto Call(Function original, Args&&... args) {
    DCHECK(original != nullptr) << point_.GetName() << " was never loaded";
    ProfileTimer timer(point_);
    return original(std::forward<Args>(args)...);
  }

 private:
  const ProfilePoint point_;
};

}  // namespace gb

#endif  // GB_PROFILE_PROFILE_CALL_HOOK_H_
