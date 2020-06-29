#ifndef GBITS_RESOURCE_RESOURCE_SET_H_
#define GBITS_RESOURCE_RESOURCE_SET_H_

#include <string_view>
#include <tuple>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "gbits/resource/resource.h"

namespace gb {

// A resource set manages shared ownership over a set of render resources.
//
// Resources can be added or removed from the set as desired. As long as this
// resource set exists, its referenced resources will not be deleted by their
// associated resource manager. Only resources from the same system can be
// stored in the same set.
//
// This class is thread-compatible, although multiple ResourcePtr and
// ResourceSet instances referring to the same resources are thread-safe
// relative to each other. Further, querying the set from multiple threads is
// safe as it is progammatically guaranteed that there are no races with
// modification functions. For instance, if a resource set is only modified at
// game load time, it may be safely queried from multiple threads during
// gameplay.
class ResourceSet final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  explicit ResourceSet() = default;
  ResourceSet(const ResourceSet&);
  ResourceSet(ResourceSet&&);
  ResourceSet& operator=(const ResourceSet&);
  ResourceSet& operator=(ResourceSet&&);
  ~ResourceSet();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns true if the resource set is empty.
  bool IsEmpty() const { return resources_.empty(); }

  // Returns the resource system common to all resources in this set.
  //
  // Only resources from the same system can be stored in the same set. If there
  // are no resources in the set, this will return nullptr.
  ResourceSystem* GetSystem() const { return system_; }

  //----------------------------------------------------------------------------
  // Resource accessors
  //----------------------------------------------------------------------------

  // Retrieves the requested resource by ID, or null if it does not exist in the
  // set.
  template <typename Type>
  Type* Get(ResourceId id) const;

  // Retrieves the requested resource by name, or null if it does not exist in
  // the set.
  template <typename Type>
  Type* Get(std::string_view name) const;

  // Adds the specified resource to the set, if it is not already.
  //
  // If the resource has discoverable resource dependencies, those are also
  // added to the set if add_dependencies is true (strongly recommended).
  // Returns false if the resource or any of its dependencies (if
  // add_dependencies is true) were not added to the set.
  bool Add(Resource* resource, bool add_dependencies = true);

  // Removes the specified resource from the set, if it is not already.
  //
  // If the resource has discoverable resource dependencies, those are also
  // removed from the set if remove_dependencies is true and there are no other
  // resources in the set that depend on them. This is strongly recommended,
  // however it may be quite an expensive operation depending on the number of
  // resources in the set.
  //
  // Returns true if the resource did not exist in the set or was successfully
  // removed from the set. A resource may fail to be removed from the set due to
  // other resources in the set referring to it.
  template <typename Type>
  bool Remove(ResourceId id, bool remove_dependencies = true);

  template <typename Type>
  bool Remove(std::string_view name, bool remove_dependencies = true);

  bool Remove(Resource* resource, bool remove_dependencies = true);

 private:
  ResourceId GetResourceIdFromName(TypeKey* type, std::string_view name) const;

  bool DoAdd(Resource* resource, bool add_dependencies);
  bool AddDependencies(Resource* resource);

  bool DoRemove(TypeKey* type, ResourceId id, bool remove_dependencies);
  bool RemoveResourceOnly(Resource* resource);
  bool RemoveWithDependencies(Resource* resource);
  void AddAllDependencies(absl::flat_hash_set<Resource*>* all_dependencies,
                          Resource* resource);

  ResourceSystem* system_ = nullptr;
  absl::flat_hash_map<ResourceKey, Resource*> resources_;
};

template <typename Type>
Type* ResourceSet::Get(ResourceId id) const {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");
  auto it = resources_.find({gb::TypeKey::Get<Type>(), id});
  return (it != resources_.end() ? static_cast<Type*>(it->second) : nullptr);
}

template <typename Type>
Type* ResourceSet::Get(std::string_view name) const {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");
  auto type = gb::TypeKey::Get<Type>();
  auto it = resources_.find({type, GetResourceIdFromName(type, name)});
  return (it != resources_.end() ? static_cast<Type*>(it->second) : nullptr);
}

inline bool ResourceSet::Add(Resource* resource, bool add_dependencies) {
  if (resource == nullptr) {
    return false;
  }
  return DoAdd(resource, add_dependencies);
}

template <typename Type>
bool ResourceSet::Remove(ResourceId id, bool remove_dependencies) {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");
  return DoRemove(gb::TypeKey::Get<Type>(), id, remove_dependencies);
}

template <typename Type>
bool ResourceSet::Remove(std::string_view name, bool remove_dependencies) {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");
  auto type = gb::TypeKey::Get<Type>();
  return DoRemove(type, GetResourceIdFromName(type, name), remove_dependencies);
}

inline bool ResourceSet::Remove(Resource* resource, bool remove_dependencies) {
  if (resource == nullptr) {
    return true;
  }
  return DoRemove(resource->GetResourceType(), resource->GetResourceId(),
                  remove_dependencies);
}

}  // namespace gb

#endif  // GBITS_RESOURCE_RESOURCE_SET_H_
