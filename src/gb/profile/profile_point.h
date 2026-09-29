// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_PROFILE_POINT_H_
#define GB_PROFILE_PROFILE_POINT_H_

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>

namespace gb {

// The most points that can be registered in a program.
inline constexpr int kMaxProfilePoints = 1024;

//==============================================================================
// ProfilePoint
//==============================================================================

// A named place in a program that a Profiler records.
//
// A point is registered once, and the same name always means the same point,
// wherever it is registered. Most points are defined where they are used, with
// the name as a template argument (ProfileScope, ProfileCall, ProfileCount,
// ProfileSetValue). A ProfilePoint is for a point whose name is only known at
// runtime, and is kept by the program to avoid registering it again.
//
// ProfilePoint is a small value type, and can be copied freely. This class is
// thread-safe.
class ProfilePoint final {
 public:
  enum class Kind {
    kFrame,    // One iteration of the program's loop.
    kScope,    // A section of the program's own code.
    kCall,     // A call out to another system.
    kCounter,  // Work done, added to as it happens.
    kValue,    // A number describing the workload, set when it changes.
  };

  // Registers the point, or returns the one already registered with `name`.
  //
  // CHECK-fails if `name` was registered with another kind, or if more than
  // kMaxProfilePoints would be registered.
  ProfilePoint(Kind kind, std::string_view name);

  // Returns the point registered with `name`, if there is one.
  static std::optional<ProfilePoint> Find(std::string_view name);

  Kind GetKind() const { return kind_; }
  std::string_view GetName() const { return name_; }

  // Returns the point's slot in every Profiler, in [0, kMaxProfilePoints).
  int GetIndex() const { return index_; }

  // Adds `count` to a counter point, in the calling thread's Profiler, if any.
  void Count(int64_t count) const;

  // Sets a value point to `value`, in the calling thread's Profiler, if any.
  void SetValue(int64_t value) const;

 private:
  ProfilePoint(Kind kind, std::string_view name, int index)
      : kind_(kind), name_(name), index_(index) {}

  Kind kind_;
  std::string_view name_;
  int index_;
};

//==============================================================================
// Named points
//==============================================================================

// A string literal that names a point as a template argument.
//
// Constructing one implicitly from a string literal is what lets a point be
// named as `ProfileCount<"midi/messages">`.
template <int kSize>
struct ProfileName {
  consteval ProfileName(const char (&name)[kSize]) {  // NOLINT: Implicit.
    std::copy_n(name, kSize, chars);
  }

  constexpr std::string_view GetName() const { return {chars, kSize - 1}; }

  // Public, so ProfileName can be a template argument.
  char chars[kSize];
};

namespace internal {

// Returns the point of `kKind` named `kName`, registering it the first time.
template <ProfilePoint::Kind kKind, ProfileName kName>
const ProfilePoint& GetNamedPoint() {
  static const ProfilePoint s_point(kKind, kName.GetName());
  return s_point;
}

}  // namespace internal

// Adds `count` to the counter point named `kName`, in the calling thread's
// Profiler, if any.
template <ProfileName kName>
void ProfileCount(int64_t count) {
  internal::GetNamedPoint<ProfilePoint::Kind::kCounter, kName>().Count(count);
}

// Sets the value point named `kName` to `value`, in the calling thread's
// Profiler, if any.
template <ProfileName kName>
void ProfileSetValue(int64_t value) {
  internal::GetNamedPoint<ProfilePoint::Kind::kValue, kName>().SetValue(value);
}

}  // namespace gb

#endif  // GB_PROFILE_PROFILE_POINT_H_
