// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/resource/resource_system.h"

#include <random>

#include "absl/time/clock.h"
#include "gb/resource/resource.h"
#include "gb/resource/resource_manager.h"
#include "glog/logging.h"

namespace gb {

std::unique_ptr<ResourceSystem> ResourceSystem::Create() {
  return absl::WrapUnique(new ResourceSystem);
}

ResourceSystem::ResourceSystem() {
  // To minimize the chance of any resource collisions across runs, the initial
  // resource ID is based on both time and randomness.
  absl::WriterMutexLock lock(&mutex_);
  uint64_t random_part = std::random_device()();
  uint64_t time_part = absl::ToUnixSeconds(absl::Now());
  next_resource_id_ = ((time_part << 32) | ((random_part & 0xFFFF) << 16)) + 1;
}

ResourceSystem::~ResourceSystem() {
  absl::flat_hash_map<TypeKey*, ResourceTypeInfo> types;
  absl::flat_hash_map<ResourceKey, ResourceInfo> resources;
  {
    absl::WriterMutexLock lock(&mutex_);
    types.swap(types_);
    resources.swap(resources_);
  }

  // Disconnect all the resource managers
  for (auto it : types) {
    it.second.manager->SetSystem({}, nullptr, {});
  }

  // Now attempt to delete the resources.
  for (auto it : resources) {
    if (it.second.resource != nullptr && !it.second.resource->MaybeDelete({})) {
      LOG(ERROR) << "Resource " << std::get<TypeKey*>(it.first)->GetTypeName()
                 << "(" << std::get<ResourceId>(it.first)
                 << ") still referenced in ResourceSystem destructor.";
    }
  }
}

bool ResourceSystem::DoRegister(std::initializer_list<TypeKey*> types,
                                ResourceManager* manager) {
  if (manager == nullptr || types.size() == 0 ||
      manager->GetSystem() != nullptr) {
    return false;
  }

  absl::WriterMutexLock lock(&mutex_);

  // If any of the types are already registered, then fail registration for all
  // types.
  for (TypeKey* type : types) {
    if (types_.contains(type)) {
      LOG(ERROR) << "Resource system already contains manager for type "
                 << type->GetTypeName();
      return false;
    }
  }

  manager->SetSystem({}, this, {types.begin(), types.end()});
  for (TypeKey* type : types) {
    auto& type_info = types_[type];
    type_info.manager = manager;
    type_info.loader = manager->GetLoader({}, type);
    type_info.release_handler = manager->GetReleaseHandler({}, type);

    std::string type_name = type->GetTypeName();
    if (!type_name.empty()) {
      type_names_[type_name] = type;
    }
  }
  return true;
}

Resource* ResourceSystem::DoGet(TypeKey* type, ResourceId id) {
  auto it = resources_.find({type, id});
  if (it == resources_.end() || !it->second.visible ||
      it->second.resource->IsDeleting({})) {
    return nullptr;
  }
  return it->second.resource;
}

Resource* ResourceSystem::DoGet(ResourceSet* set, TypeKey* key, ResourceId id) {
  if (set == nullptr) {
    return nullptr;
  }
  Resource* resource = DoGet(key, id);
  if (!set->Add(resource, false)) {
    return nullptr;
  }
  return resource;
}

void ResourceSystem::DoAddDependencies(ResourceSet* set, Resource* resource) {
  if (set == nullptr || resource == nullptr) {
    return;
  }
  ResourceDependencyList dependencies;
  resource->GetResourceDependencies(&dependencies);
  for (Resource* resource : dependencies) {
    set->Add(resource);
  }
}

bool ResourceSystem::ReserveResourceName(ResourceInternal, TypeKey* type,
                                         ResourceId id,
                                         const std::string& name) {
  absl::WriterMutexLock lock(&mutex_);
  auto type_it = types_.find(type);
  if (type_it == types_.end()) {
    return false;
  }
  auto name_it = type_it->second.name_to_id.find(name);
  if (name_it != type_it->second.name_to_id.end()) {
    return name_it->second == id;
  }
  type_it->second.name_to_id[name] = id;
  return true;
}

void ResourceSystem::ReleaseResourceName(ResourceInternal, TypeKey* type,
                                         ResourceId id,
                                         const std::string& name) {
  absl::WriterMutexLock lock(&mutex_);
  auto type_it = types_.find(type);
  if (type_it == types_.end()) {
    return;
  }
  auto name_it = type_it->second.name_to_id.find(name);
  if (name_it == type_it->second.name_to_id.end()) {
    LOG(ERROR) << "Name reservation removed unexpectedly. ID=" << id
               << ", Name=\"" << name << "\"";
    return;
  }
  auto id_it = type_it->second.id_to_name.find(id);
  if (id_it != type_it->second.id_to_name.end() && id_it->second == name) {
    return;
  }
  type_it->second.name_to_id.erase(name_it);
}

void ResourceSystem::ApplyResourceName(ResourceInternal, TypeKey* type,
                                       ResourceId id, const std::string& name) {
  absl::WriterMutexLock lock(&mutex_);
  auto type_it = types_.find(type);
  if (type_it == types_.end()) {
    return;
  }
  auto name_it = type_it->second.name_to_id.find(name);
  if (name_it == type_it->second.name_to_id.end()) {
    LOG(ERROR) << "Name reservation removed unexpectedly. ID=" << id
               << ", Name=\"" << name << "\"";
    return;
  }
  auto id_it = type_it->second.id_to_name.find(id);
  if (id_it != type_it->second.id_to_name.end()) {
    if (id_it->second == name) {
      return;
    }
    type_it->second.name_to_id.erase(id_it->second);
    id_it->second = name;
    return;
  }
  type_it->second.id_to_name[id] = name;
}

ResourcePtr<Resource> ResourceSystem::DoLoad(TypeKey* type,
                                             std::string_view name) {
  // TODO: This method can fail erroneously if there are simultaneous load
  //       requests on different threads for the same resource, or if the
  //       resource is deleted on a separate thread immediately after the name
  //       lookup. This should be vanishingly rare in practice.

  // Look up the resource first, to see if it is already loaded.
  {
    absl::ReaderMutexLock lock(&mutex_);
    ResourceId id = DoGetResourceIdFromName(type, name);
    if (id != 0) {
      auto it = resources_.find({type, id});
      if (it == resources_.end() || it->second.resource == nullptr ||
          it->second.resource->IsDeleting({})) {
        return nullptr;
      }
      return it->second.resource;
    }
  }

  std::string name_string(name.data(), name.size());
  Loader* loader = nullptr;
  {
    absl::WriterMutexLock lock(&mutex_);
    auto it = types_.find(type);
    if (it == types_.end()) {
      return nullptr;
    }
    if (it->second.name_to_id.contains(name)) {
      // This name is already reserved for a pending load.
      return nullptr;
    }
    // Zero is a marker for the reserved name.
    it->second.name_to_id[name_string] = 0;
    loader = it->second.loader;
  }

  ResourcePtr<Resource> resource = (*loader)(type, name);

  {
    absl::WriterMutexLock lock(&mutex_);
    auto it = types_.find(type);
    CHECK(it != types_.end())
        << "Resource manager deleted during resource load";
    if (resource == nullptr) {
      it->second.name_to_id.erase(name_string);
      return nullptr;
    }

    auto id = resource->GetResourceId();
    it->second.name_to_id[name_string] = id;
    it->second.id_to_name[id] = std::move(name_string);
  }

  return resource;
}

void ResourceSystem::RemoveManager(ResourceInternal, ResourceManager* manager) {
  std::vector<Resource*> resources;
  {
    absl::WriterMutexLock lock(&mutex_);
    resources.reserve(resources_.size());
    absl::flat_hash_set<TypeKey*> types;
    for (auto it : types_) {
      if (it.second.manager == manager) {
        types.insert(it.first);
      }
    }
    for (auto it : resources_) {
      if (types.contains(std::get<TypeKey*>(it.first)) &&
          it.second.resource != nullptr) {
        resources.push_back(it.second.resource);
      }
    }
    for (auto type : types) {
      types_.erase(type);
    }
  }

  // Only managers can delete a resource, and the manager is in its destructor
  // currently, so won't be deleting these resources out from under us.
  for (Resource* resource : resources) {
    if (!resource->MaybeDelete({})) {
      LOG(ERROR) << "Resource " << resource->GetResourceType()->GetTypeName()
                 << " " << resource->GetResourceId()
                 << " still referenced in ResourceManager destructor.";

      // Force the removal anyway, as the manager is gone.
      absl::WriterMutexLock lock(&mutex_);
      resources_.erase(
          {resource->GetResourceType(), resource->GetResourceId()});
    }
  }
}

void ResourceSystem::AddResource(ResourceInternal, Resource* resource) {
  absl::WriterMutexLock lock(&mutex_);
  resources_[{resource->GetResourceType(), resource->GetResourceId()}]
      .resource = resource;
}

void ResourceSystem::RemoveResource(ResourceInternal, TypeKey* type,
                                    ResourceId id) {
  absl::WriterMutexLock lock(&mutex_);
  if (resources_.erase({type, id}) == 0) {
    // This can happen legitimately during system destruction, as the resources
    // are removed explicitly.
    return;
  }
  auto it = types_.find(type);
  if (it == types_.end()) {
    // This can happen legitimately during manager destruction, as the managers
    // are removed first.
    return;
  }
  auto id_it = it->second.id_to_name.find(id);
  if (id_it != it->second.id_to_name.end()) {
    it->second.name_to_id.erase(id_it->second);
    it->second.id_to_name.erase(id_it);
  }
}

void ResourceSystem::ReleaseResource(ResourceInternal, Resource* resource) {
  ReleaseHandler* release_handler = nullptr;
  {
    absl::ReaderMutexLock lock(&mutex_);
    auto it = types_.find(resource->GetResourceType());
    if (it == types_.end()) {
      LOG(ERROR)
          << "Resource " << resource->GetResourceType()->GetTypeName() << " "
          << resource->GetResourceId()
          << " getting released after/during manager/system destruction.";
      return;
    }
    release_handler = it->second.release_handler;
  }
  (*release_handler)(resource);
}

void ResourceSystem::SetResourceVisible(ResourceInternal, Resource* resource,
                                        bool visible) {
  absl::WriterMutexLock lock(&mutex_);
  resources_[{resource->GetResourceType(), resource->GetResourceId()}].visible =
      visible;
}

ResourceEntry ResourceSystem::NewResourceEntry(ResourceInternal, TypeKey* type,
                                               ResourceId id) {
  absl::WriterMutexLock lock(&mutex_);
  if (id == 0) {
    do {
      id = next_resource_id_++;
    } while (!resources_.try_emplace({type, id}).second);
  } else if (!resources_.try_emplace({type, id}).second) {
    return {};
  }
  return ResourceEntry({}, this, type, id);
}

std::string_view ResourceSystem::GetResourceName(ResourceInternal,
                                                 TypeKey* type, ResourceId id) {
  absl::ReaderMutexLock lock(&mutex_);
  auto it = types_.find(type);
  if (it == types_.end()) {
    return {};
  }
  auto id_it = it->second.id_to_name.find(id);
  if (id_it == it->second.id_to_name.end()) {
    return {};
  }
  return id_it->second;
}

ResourceId ResourceSystem::DoGetResourceIdFromName(TypeKey* type,
                                                   std::string_view name) {
  auto it = types_.find(type);
  if (it == types_.end()) {
    return 0;
  }
  auto name_it = it->second.name_to_id.find(name);
  if (name_it == it->second.name_to_id.end()) {
    return 0;
  }
  return name_it->second;
}

}  // namespace gb
