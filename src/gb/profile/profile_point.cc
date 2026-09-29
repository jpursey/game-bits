// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profile_point.h"

#include <array>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "gb/profile/profiler.h"

namespace gb {

namespace {

struct Registered {
  ProfilePoint::Kind kind;
  std::string name;
};

// Every registered point, by index, and each point's index by name.
//
// The points are in a fixed array, so each name stays at a fixed address,
// which the map's keys and every ProfilePoint refer to.
struct Registry {
  absl::Mutex mutex;
  std::array<Registered, kMaxProfilePoints> points ABSL_GUARDED_BY(mutex);
  int count ABSL_GUARDED_BY(mutex) = 0;
  absl::flat_hash_map<std::string_view, int> indices ABSL_GUARDED_BY(mutex);
};

Registry& GetRegistry() {
  static absl::NoDestructor<Registry> s_registry;
  return *s_registry;
}

}  // namespace

ProfilePoint::ProfilePoint(Kind kind, std::string_view name) {
  Registry& registry = GetRegistry();
  absl::MutexLock lock(&registry.mutex);
  auto it = registry.indices.find(name);
  if (it == registry.indices.end()) {
    CHECK(registry.count < kMaxProfilePoints)
        << "More than " << kMaxProfilePoints << " profile points";
    Registered& added = registry.points[registry.count];
    added.kind = kind;
    added.name = name;
    it = registry.indices.emplace(added.name, registry.count).first;
    ++registry.count;
  }
  const Registered& registered = registry.points[it->second];
  CHECK(registered.kind == kind)
      << "Profile point \"" << name << "\" was registered with another kind";
  kind_ = kind;
  name_ = registered.name;
  index_ = it->second;
}

std::optional<ProfilePoint> ProfilePoint::Find(std::string_view name) {
  Registry& registry = GetRegistry();
  absl::MutexLock lock(&registry.mutex);
  auto it = registry.indices.find(name);
  if (it == registry.indices.end()) {
    return std::nullopt;
  }
  const Registered& registered = registry.points[it->second];
  return ProfilePoint(registered.kind, registered.name, it->second);
}

int ProfilePoint::GetRegisteredCount() {
  Registry& registry = GetRegistry();
  absl::MutexLock lock(&registry.mutex);
  return registry.count;
}

std::vector<ProfilePoint> ProfilePoint::GetRegisteredPoints() {
  Registry& registry = GetRegistry();
  absl::MutexLock lock(&registry.mutex);
  std::vector<ProfilePoint> points;
  points.reserve(registry.count);
  for (int i = 0; i < registry.count; ++i) {
    const Registered& registered = registry.points[i];
    points.push_back(ProfilePoint(registered.kind, registered.name, i));
  }
  return points;
}

void ProfilePoint::Count(int64_t count) const {
  DCHECK(kind_ == Kind::kCounter) << name_ << " is not a counter";
  Profiler* const profiler = Profiler::s_current;
  if (profiler != nullptr) {
    profiler->AddCount(index_, count);
  }
}

void ProfilePoint::SetValue(int64_t value) const {
  DCHECK(kind_ == Kind::kValue) << name_ << " is not a value";
  Profiler* const profiler = Profiler::s_current;
  if (profiler != nullptr) {
    profiler->SetValue(index_, value);
  }
}

}  // namespace gb
