// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_RESSOURCE_SYSTEM_H_
#define GB_RESOURCE_RESSOURCE_SYSTEM_H_

#include <memory>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "gb/base/callback.h"
#include "gb/base/type_info.h"
#include "gb/base/validated_context.h"
#include "gb/resource/resource_ptr.h"
#include "gb/resource/resource_set.h"
#include "gb/resource/resource_types.h"

namespace gb {

// This class manages a cache of shared game resources and corresponding
// resource managers.
//
// In order to be used, one or more resource managers must be registered with
// the resource system declaring the resource types they support. Any resources
// created or loaded using the resource manager will automatically be cached in
// the resource system and may be retrieved via the ResourceSystem::Get method.
// Resources may be retrieved using their resource ID (preferred) or resource
// name (if there is one).
//
// If a resource manager supports it, resources can also be loaded directly via
// the resource system. Often, resource names are path names to a file on disk
// or in an archive, but they may be anything that the underlying resource
// manager supports.
//
// When the resource system is deleted, all resource managers still registered
// will be unregistered automatically, and all resource they reference will be
// deleted. The ResourceSystem MUST outlive any ResourceSet or ResourcePtr that
// refers to a resource within this manager. Otherwise, any change to those
// classes will crash, and the resources themselves will be leaked.
//
// This class is thread-safe except as noted.
class ResourceSystem final {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // OPTIONAL: Specifies resource set that should be used to collect all
  // resources (including dependent resources) during a load. If not specified,
  // this is set automatically by the Load calls that take a resource set as a
  // parameter.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSet, kInOptional,
                               ResourceSet);
  static GB_CONTEXT_CONSTRAINT(kConstraintOutResourceSet, kOutOptional,
                               ResourceSet);

  using LoadContract =
      ContextContract<kConstraintResourceSet, kConstraintOutResourceSet>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  static std::unique_ptr<ResourceSystem> Create();
  ResourceSystem(const ResourceSystem&) = delete;
  ResourceSystem& operator=(const ResourceSystem&) = delete;
  ~ResourceSystem();

  //----------------------------------------------------------------------------
  // Initialization
  //----------------------------------------------------------------------------

  // Registers a resource manager which controls creation and deletion of one or
  // more types of resources.
  //
  // All specified resource types should have type names specified:
  //   TypeKey::Get<Example>()->SetTypeName("Example");
  // Each type registered with a resource system must have a unique name. This
  // is used to support generic resource type serialization (see
  // ResourceFileReader and ResourceFileWriter).
  //
  // A single manager can handle multiple resource types, but a type may only be
  // registered against one manager. This returns true if the manager could be
  // registered for ALL specified resource types. On failure, the resource
  // manager will not be registered for any resource types.
  template <typename... Type>
  bool Register(ResourceManager* manager);

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Retrieves the type for a registered resource type.
  //
  // The type name matches the type name set on the type key when it was
  // registered with the resource system.
  TypeKey* GetResourceType(std::string_view type_name);

  // Retrieves the requested resource by ID, or null if it does not exist or is
  // not loaded.
  template <typename Type>
  ResourcePtr<Type> Get(ResourceId id);

  template <typename Type>
  Type* Get(ResourceSet* set, ResourceId id, bool get_dependencies = true);

  // Retrieves the requested resource by name, or null if it does not exist or
  // is not loaded.
  template <typename Type>
  ResourcePtr<Type> Get(std::string_view name);

  template <typename Type>
  Type* Get(ResourceSet* set, std::string_view name,
            bool get_dependencies = true);

  // Loads the requested resource by name, returning null it could not be
  // loaded. If the resource is already loaded, the existing resource will be
  // returned (a new copy will not be loaded).
  //
  // Load is thread-safe as long as the underlying manager for the type supports
  // thread-safe loading.
  template <typename Type>
  ResourcePtr<Type> Load(std::string_view name,
                         LoadContract contract = Context{});

  template <typename Type>
  Type* Load(ResourceSet* set, std::string_view name,
             LoadContract contract = Context{});

  Resource* Load(ResourceSet* set, TypeKey* type, std::string_view name,
                 LoadContract contract = Context{});

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  void RemoveManager(ResourceInternal, ResourceManager* manager);
  void AddResource(ResourceInternal, Resource* resource);
  void RemoveResource(ResourceInternal, TypeKey* type, ResourceId id);
  void ReleaseResource(ResourceInternal, Resource* resource);
  void SetResourceVisible(ResourceInternal, Resource* resource, bool visibile);
  ResourceEntry NewResourceEntry(ResourceInternal, TypeKey* type,
                                 ResourceId id);
  std::string_view GetResourceName(ResourceInternal, TypeKey* type,
                                   ResourceId id);
  ResourceId GetResourceIdFromName(ResourceInternal, TypeKey* type,
                                   std::string_view name);
  void ResourceLock(ResourceInternal) ABSL_EXCLUSIVE_LOCK_FUNCTION(mutex_);
  void ResourceUnlock(ResourceInternal) ABSL_UNLOCK_FUNCTION(mutex_);
  Resource* Find(ResourceInternal, TypeKey* type, ResourceId id);
  bool ReserveResourceName(ResourceInternal, TypeKey* type, ResourceId id,
                           const std::string& name);
  void ReleaseResourceName(ResourceInternal, TypeKey* type, ResourceId id,
                           const std::string& name);
  void ApplyResourceName(ResourceInternal, TypeKey* type, ResourceId id,
                         const std::string& name);

 private:
  using Loader = Callback<Resource*(Context*, TypeKey*, std::string_view)>;
  using ReleaseHandler = Callback<void(Resource*)>;

  struct ResourceTypeInfo {
    ResourceTypeInfo() = default;

    ResourceManager* manager = nullptr;
    Loader* loader;
    ReleaseHandler* release_handler;

    absl::flat_hash_map<std::string, ResourceId> name_to_id;
    absl::flat_hash_map<ResourceId, std::string> id_to_name;
  };

  struct ResourceInfo {
    ResourceInfo() = default;
    Resource* resource = nullptr;
    bool visible = false;
  };

  ResourceSystem();

  bool DoRegister(std::initializer_list<TypeKey*> types,
                  ResourceManager* manager) ABSL_LOCKS_EXCLUDED(mutex_);
  ResourceId DoGetResourceIdFromName(TypeKey* type, std::string_view name)
      ABSL_SHARED_LOCKS_REQUIRED(mutex_);
  Resource* DoGet(TypeKey* type, ResourceId id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  Resource* DoGet(ResourceSet* set, TypeKey* type, ResourceId id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  void DoAddDependencies(ResourceSet* set, Resource* resource)
      ABSL_LOCKS_EXCLUDED(mutex_);
  ResourcePtr<Resource> DoLoad(TypeKey* type, std::string_view name,
                               ValidatedContext context)
      ABSL_LOCKS_EXCLUDED(mutex_);

  absl::Mutex mutex_;
  absl::flat_hash_map<std::string, TypeKey*> type_names_;
  absl::flat_hash_map<TypeKey*, ResourceTypeInfo> types_
      ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<ResourceKey, ResourceInfo> resources_
      ABSL_GUARDED_BY(mutex_);
  uint64_t next_resource_id_ ABSL_GUARDED_BY(mutex_);
};

template <typename... Type>
inline bool ResourceSystem::Register(ResourceManager* manager) {
  return DoRegister({TypeKey::Get<Type>()...}, manager);
}

inline TypeKey* ResourceSystem::GetResourceType(std::string_view type_name) {
  absl::MutexLock lock(&mutex_);
  auto it = type_names_.find(type_name);
  return it != type_names_.end() ? it->second : nullptr;
}

template <typename Type>
ResourcePtr<Type> ResourceSystem::Get(ResourceId id) {
  absl::MutexLock lock(&mutex_);
  Resource* resource = DoGet(TypeKey::Get<Type>(), id);
  return static_cast<Type*>(resource);
}

template <typename Type>
Type* ResourceSystem::Get(ResourceSet* set, ResourceId id,
                          bool get_dependencies) {
  mutex_.Lock();
  Type* resource = static_cast<Type*>(DoGet(set, TypeKey::Get<Type>(), id));
  mutex_.Unlock();
  if (get_dependencies) {
    DoAddDependencies(set, resource);
  }
  return resource;
}

template <typename Type>
ResourcePtr<Type> ResourceSystem::Get(std::string_view name) {
  absl::MutexLock lock(&mutex_);
  TypeKey* type = TypeKey::Get<Type>();
  ResourceId id = DoGetResourceIdFromName(type, name);
  Resource* resource = DoGet(TypeKey::Get<Type>(), id);
  return static_cast<Type*>(resource);
}

template <typename Type>
Type* ResourceSystem::Get(ResourceSet* set, std::string_view name,
                          bool get_dependencies) {
  mutex_.Lock();
  TypeKey* type = TypeKey::Get<Type>();
  ResourceId id = DoGetResourceIdFromName(type, name);
  Type* resource = static_cast<Type*>(DoGet(set, type, id));
  mutex_.Unlock();
  if (get_dependencies) {
    DoAddDependencies(set, resource);
  }
  return resource;
}

template <typename Type>
ResourcePtr<Type> ResourceSystem::Load(std::string_view name,
                                       LoadContract contract) {
  ResourcePtr<Resource> resource =
      DoLoad(TypeKey::Get<Type>(), name, std::move(contract));
  return ResourcePtr<Type>(static_cast<Type*>(resource.Get()));
}

inline Resource* ResourceSystem::Load(ResourceSet* set, TypeKey* type,
                                      std::string_view name,
                                      LoadContract contract) {
  if (set == nullptr) {
    return nullptr;
  }
  ValidatedContext context = std::move(contract);
  if (!context.Exists<ResourceSet>()) {
    context.SetPtr<ResourceSet>(set);
  }
  ResourcePtr<Resource> resource = DoLoad(type, name, std::move(context));
  if (resource != nullptr) {
    set->Add(resource.Get());
  }
  return resource.Get();
}

template <typename Type>
inline Type* ResourceSystem::Load(ResourceSet* set, std::string_view name,
                                  LoadContract contract) {
  return static_cast<Type*>(
      Load(set, TypeKey::Get<Type>(), name, std::move(contract)));
}

inline ResourceId ResourceSystem::GetResourceIdFromName(ResourceInternal,
                                                        TypeKey* type,
                                                        std::string_view name) {
  absl::MutexLock lock(&mutex_);
  return DoGetResourceIdFromName(type, name);
}

inline void ResourceSystem::ResourceLock(ResourceInternal) {
  mutex_.WriterLock();
}

inline void ResourceSystem::ResourceUnlock(ResourceInternal) {
  mutex_.WriterUnlock();
}

inline Resource* ResourceSystem::Find(ResourceInternal, TypeKey* type,
                                      ResourceId id) {
  absl::MutexLock lock(&mutex_);
  return DoGet(type, id);
}

}  // namespace gb

#endif  // GB_RESOURCE_RESSOURCE_SYSTEM_H_
