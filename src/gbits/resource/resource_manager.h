#ifndef GBITS_RESOURCE_RESOURCE_MANAGER_H_
#define GBITS_RESOURCE_RESOURCE_MANAGER_H_

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "gbits/base/callback.h"
#include "gbits/resource/resource_ptr.h"
#include "gbits/resource/resource_types.h"

namespace gb {

// A resource manager controls the lifecycle for resources.
//
// Every resource requires a resource manager to be constructed and can only be
// deleted via its manager.
//
// Resource types that are set to auto-release, inform the ResourceManager when
// there are no more ResourceSet or ResourcePtr references to it. By default,
// this will result in the resource being deleted, but individual resource types
// may override this behavior.
//
// Resources can also be deleted by calling MaybeDeleteResource, which will
// delete the resource iff there are no existing references to it.
//
// A ResourceManager MUST outlive any ResourceSet or ResourcePtr that refers to
// a resource within this manager. Otherwise, any change to those classes will
// crash, and the resources themselves will be leaked.
//
// This class is thread-compatible to initialize, and thread-safe once it is
// registered with a ResourceSystem.
class ResourceManager final {
 public:
  using GenericLoader =
      Callback<Resource*(TypeKey* type, std::string_view name)>;
  using GenericReleaseHandler = Callback<void(Resource* resource)>;

  template <typename Type>
  using Loader = Callback<Type*(std::string_view name)>;
  template <typename Type>
  using ReleaseHandler = Callback<void(Type* resource)>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------
  ResourceManager();
  ResourceManager(const ResourceManager&) = delete;
  ResourceManager& operator=(const ResourceManager&) = delete;
  ~ResourceManager();

  //----------------------------------------------------------------------------
  // Initialization
  //----------------------------------------------------------------------------

  // Initializes a loader for the specified type.
  //
  // InitLoader must only be called before the manager is registered with a
  // ResourceSystem, and may only be called once for any given type. Additional
  // calls for a previously used type will log an error and be ignored.
  template <typename Type>
  void InitLoader(Loader<Type> callback);

  // Initializes a generic loader which will handle all resource load requests
  // that do not have type-specific loaders.
  //
  // InitGenericLoader must only be called before the manager is registered with
  // a ResourceSystem, and may only be called once. Additional calls will log an
  // error and be ignored.
  void InitGenericLoader(GenericLoader callback);

  // Intializes a handler which will be called when the last reference to a
  // resource of the specified type is reached.
  //
  // InitReleaseHandler must only be called before the manager is registered
  // with a ResourceSystem, and may only be called once for any given type.
  // Additional calls for a previously used type will log an error and be
  // ignored.
  template <typename Type>
  void InitReleaseHandler(ReleaseHandler<Type> callback);

  // Initializes a generic release handler, which will handle release behavior
  // for any resource types that do not have type-specific release handlers.
  //
  // If this is not set, the generic behavior is to call MaybeDeleteResource.
  //
  // InitGenericReleaseHandler must only be called before the manager is
  // registered with a ResourceSystem, and may only be called once. Additional
  // calls will log an error and be ignored.
  void InitGenericReleaseHandler(GenericReleaseHandler callback);

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  ResourceSystem* GetSystem() { return system_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Returns a new resource entry which may used to create a resource.
  //
  // This method will mint a unique resource ID, and allocate an entry of the
  // specified type bound to this manager. If this manager is not registered
  // against the specified type with a ResourceSystem, this will return an
  // invalid ResourceEntry.
  template <typename Type>
  ResourceEntry NewResourceEntry();

  // Returns a new resource entry with the explicitly specified ID.
  //
  // This method allocate an entry with the specified ID and type bound to this
  // manager. If this manager is not registered against the specified type with
  // a ResourceSystem, or the ID is zero or already in use, this will return an
  // invalid ResourceEntry.
  //
  // This method should only be used by managers if it conforms to one or both
  // of the following circumstances:
  // 1. The manager has taken on complete responsibility for minting unique
  //    resource IDs (the other NewResourceEntry() is never called).
  // 2. The manager is reconstructing a resource using its previously minted
  //    resource ID.
  template <typename Type>
  ResourceEntry NewResourceEntryWithId(ResourceId id);

  // Attempts to delete the resource.
  //
  // If the resource is currently referenced, this will do nothing and return
  // false. Otherwise the instance will be deleted, this will return true, and
  // any existing Resource pointers to this resource will be invalid.
  //
  // This is a relatively heavyweight operation, and so should generally only be
  // called when the chance of success is high.
  bool MaybeDeleteResource(Resource* resource);

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  void SetSystem(ResourceInternal, ResourceSystem* system,
                 absl::flat_hash_set<TypeKey*> types);
  GenericLoader* GetLoader(ResourceInternal, TypeKey* type);
  GenericReleaseHandler* GetReleaseHandler(ResourceInternal, TypeKey* type);

 private:
  struct Callbacks {
    GenericLoader loader;
    GenericReleaseHandler release_handler;
  };

  void DoInitLoader(TypeKey* type, GenericLoader callback);
  void DoInitReleaseHandler(TypeKey* type, GenericReleaseHandler callback);
  ResourceEntry DoNewResourceEntry(TypeKey* type, ResourceId id);

  ResourceSystem* system_ = nullptr;
  absl::flat_hash_set<TypeKey*> types_;
  absl::flat_hash_map<TypeKey*, Callbacks> typed_callbacks_;
  GenericLoader generic_loader_;
  GenericReleaseHandler generic_release_handler_;
};

template <typename Type>
void ResourceManager::InitLoader(
    Callback<Type*(std::string_view name)> callback) {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");
  DoInitLoader(TypeKey::Get<Type>(),
               [loader = std::move(callback)](
                   TypeKey* type, std::string_view name) -> Resource* {
                 return loader(name);
               });
}

template <typename Type>
void ResourceManager::InitReleaseHandler(
    Callback<void(Type* resource)> callback) {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");
  DoInitReleaseHandler(
      TypeKey::Get<Type>(),
      [release_handler = std::move(callback)](Resource* resource) {
        return release_handler(static_cast<Type*>(resource));
      });
}

template <typename Type>
ResourceEntry ResourceManager::NewResourceEntry() {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");
  return DoNewResourceEntry(TypeKey::Get<Type>(), 0);
}

template <typename Type>
ResourceEntry ResourceManager::NewResourceEntryWithId(ResourceId id) {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");
  if (id == 0) {
    return {};
  }
  return DoNewResourceEntry(TypeKey::Get<Type>(), id);
}

}  // namespace gb

#endif  // GBITS_RESOURCE_RESOURCE_MANAGER_H_
