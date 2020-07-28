#ifndef GB_RESOURCE_RESOURCE_H_
#define GB_RESOURCE_RESOURCE_H_

#include <atomic>
#include <string>

#include "absl/container/inlined_vector.h"
#include "absl/synchronization/mutex.h"
#include "gb/base/flags.h"
#include "gb/resource/resource_entry.h"
#include "gb/resource/resource_types.h"
#include "glog/logging.h"

namespace gb {

// Type used when tracing resource dependencies.
//
// This is tuned so that heap allocations are not required for resources that
// have a low number of dependencies.
using ResourceDependencyList = absl::InlinedVector<Resource*, 16>;

// Resource flags control the behavior of a resource type. They are set as part
// of the resource construction.
enum class ResourceFlag {
  // When the resource is referenced for the first time (by a ResourcePtr or
  // ResourceSet), it will automatically become visible within the
  // ResourceSystem (see Resource::SetResourceVisible).
  kAutoVisible,

  // When the last reference to a resource is removed (not held by any
  // ResourcePtr or ResourceSet), it will automatically trigger the release
  // behavior defined in the ResourceManager (the default behavior being to
  // delete the resource).
  kAutoRelease,
};
using ResourceFlags = Flags<ResourceFlag>;
inline constexpr ResourceFlags kDefaultResourceFlags = {
    ResourceFlag::kAutoVisible,
    ResourceFlag::kAutoRelease,
};

// This class represents a shared resource for a game.
//
// Resources are implicitly owned by a ResourceManager (each ResourceManager
// handles one or more derived types of Resource). Every resource is keyed by
// its type (derived class) and its ID.
//
// Resource instances are intended to be passed by raw pointer for efficiency,
// which requires callers ensure that they are not referencing any dangling
// pointers. Callers must ensure this by holding all resources in either a
// ResourceSet or a ResourcePtr. Storing in either of these has a resource
// management cost (reference counting and potential synchronization with the
// owning resource manager), so it is recommended for ensuring ownership and
// during ownership transfer only. If a weak reference to a resource is desired,
// store/pass by ResourceId instead, and retrieve an owned reference on demand
// by calling ResourceSystem::Get.
//
// Resources must never be deleted explicitly, instead relying on the resource
// manager to delete any unreferenced resources. This further means that all
// references to a resource instance must be released during the lifetime of its
// resource manager and the resource system it is a part of. In practical terms,
// this means there cannot be any ResourceSet or ResourcePtr instances
// referencing a resource when its manager or the resource system is destructed.
//
// A typical use case is for a game level (or other logical chunk of game
// content) to hold all dependent resources in a single ResourceSet.
// Inter-resource dependencies and execution dependencies may then use raw
// pointers leveraging higher level logic to ensure the resource set is valid.
//
// Game editors on the other hand may wish to use ResourcePtr to allow for more
// fine grained ownership semantics that it manages itself (with the caveat that
// circular dependencies must be avoided).
//
// When used as described above, Resource itself is thread-safe, but derived
// types may have weaker thread guarantees. However, ALL thread safety
// guarantees are off the table if a raw Resource pointer to an unreferenced
// resource is used after it is added to the resource system (resources are
// added to the system the first time they are referenced).
class Resource {
 public:
  // Resources cannot be copied, as their lifecycle is tied to a
  // ResourceManager.
  Resource(const Resource&) = delete;
  Resource(Resource&&) = delete;
  Resource& operator=(const Resource&) = delete;
  Resource& operator=(Resource&&) = delete;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the resource system for this resource.
  ResourceSystem* GetResourceSystem() const { return entry_.GetSystem(); }

  // Returns what resource type this is.
  gb::TypeKey* GetResourceType() const { return entry_.GetType(); }

  // Returns the unique ID for this resource.
  ResourceId GetResourceId() const { return entry_.GetId(); }

  // Returns true if the resource is currently referenced by any ResourceSets or
  // ResourcePtrs.
  //
  // This is of course transient information in the case of multiple threads
  // using the resource, so caution is necessary in using this result.
  bool IsResourceReferenced() const { return ref_count_ > 1; }

  // Returns the resource name associated with this resource, if there is one.
  //
  // Resource names are the name used to load the resource. Resources can be
  // anonymous (no name), but if the name is set, then it will be unique within
  // its resource type.
  //
  // Resource IDs should be preferred as a programmatic key to another resource
  // when a pointer is not possible or appropriate. For instance, when a weak
  // reference to a resource is desired, or when serializing resources to/from
  // storage.
  std::string_view GetResourceName() const { return entry_.GetName(); }

  // Set the resource visibility in the ResourceSystem.
  //
  // Resource visibility determines whether a resource may be looked up by ID or
  // name via ResourceManager::Get. The default behavior is that a resource
  // becomes visible the first time it is referenced (held by a ResourcePtr or
  // ResourceSet). Specific resource types may leave resources hidden by
  // default, or their managers may make the resource visible prior to it being
  // explicitiy referenced.
  //
  // Note that visibility has no effect on the ResourceManager::Load
  // operation, which may retrieve an existing resource if it was already loaded
  // under the specified name.
  void SetResourceVisible(bool visible = true);

  // Return any immediate resource dependencies of this resource.
  //
  // Derived classes should override this if a resource depends on other
  // resources, and return those resources. This should only return the
  // immediate dependencies, and not indirect dependencies. It is valid for the
  // implicit dependency graph to be circular.
  //
  // This method must NOT remove, clear, or otherwise modifiy the passed in
  // dependency list, as this method may be used to accumulaate dependencies.
  virtual void GetResourceDependencies(
      ResourceDependencyList* dependencies) const {}

  //----------------------------------------------------------------------------
  // Internal methods
  //----------------------------------------------------------------------------

  bool MaybeDelete(ResourceInternal);
  void AddRef(ResourceInternal);
  void RemoveRef(ResourceInternal);
  bool IsDeleting(ResourceInternal);

 protected:
  // The resource entry passed to Resource binds this resource to a specific
  // resource system and resource manager.
  //
  // If ResourceFlag::kAutoVisible is set (the default), then the resource will
  // automatically become visible in the ResourceSystem as soon as the resource
  // is FIRST referenced (held in a ResourcePtr or ResourceSet).
  //
  // If ResourceFlag::kAutoRelease is set (the default), then any release
  // callback in the manager will be called when the last reference goes away
  // (the default release behavior is to delete the resource).
  //
  // Specific resource types and their managers may choose to override the
  // default behavior.
  explicit Resource(ResourceEntry entry,
                    ResourceFlags flags = kDefaultResourceFlags);

  // Derived resource types should also have protected or private destructors.
  // The only valid way to delete a resource is by calling
  // Delete (if it is not referenced and was never visible in the resource
  // system), or via ResourceManager::MaybeDeleteResource (at any time).
  virtual ~Resource();

  // Explicitly delete the resource.
  //
  // This may be called only if there are no references to the resource, and it
  // is has never been visible in the resource system. This is intended to be
  // used during resource construction, where the validity of the resource may
  // not be known until later (for instance when creating a compound resource,
  // or if initialization is multi-phased).
  //
  // These are hard preconditions, which must be enforced programmatically. If
  // they are not met, this will crash the program.
  void Delete();

 private:
  enum class State {
    kNew,
    kActive,
    kReleasing,
    kDeleting,
  };

  void DoAutoVisible();
  void Release();

  const ResourceEntry entry_;
  const ResourceFlags flags_;
  std::atomic<int> ref_count_ = 1;
  absl::Mutex mutex_;
  State state_ ABSL_GUARDED_BY(mutex_) = State::kNew;
};

inline void Resource::AddRef(ResourceInternal) {
  if (flags_.IsSet(ResourceFlag::kAutoVisible) && ref_count_ == 1) {
    DoAutoVisible();
  } else {
    ++ref_count_;
  }
}

inline void Resource::RemoveRef(ResourceInternal) {
  if (flags_.IsSet(ResourceFlag::kAutoRelease) && ref_count_ == 2) {
    Release();
  } else {
    --ref_count_;
  }
}

inline bool Resource::IsDeleting(ResourceInternal) { return ref_count_ == 0; }

}  // namespace gb

#endif  // GB_RESOURCE_RESOURCE_H_
