// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/resource/resource_system.h"

#include <random>
#include <tuple>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gb/resource/resource_manager.h"

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
  Resources resources;
  {
    absl::WriterMutexLock lock(&mutex_);
    types.swap(types_);
    resources.swap(resources_);
  }

  // Disconnect all the resource managers
  for (const auto& [type, type_info] : types) {
    type_info.manager->SetSystem({}, nullptr, {});
  }

  // Now attempt to delete the resources.
  for (const auto& [key, resource] : resources) {
    if (resource != nullptr && !resource->MaybeDelete({})) {
      LOG(ERROR) << "Resource " << std::get<TypeKey*>(key)->GetTypeName() << "("
                 << std::get<ResourceId>(key)
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

  // If any of the types are already registered, or their names are used by
  // other types, then fail registration for all types.
  absl::flat_hash_set<std::string_view> new_type_names;
  for (TypeKey* type : types) {
    if (types_.contains(type)) {
      LOG(ERROR) << "Resource system already contains manager for type "
                 << type->GetTypeName();
      return false;
    }
    std::string_view type_name = type->GetTypeName();
    if (!type_name.empty() && (type_names_.contains(type_name) ||
                               !new_type_names.insert(type_name).second)) {
      LOG(ERROR) << "Resource system already contains a type named "
                 << type_name;
      return false;
    }
  }

  manager->SetSystem({}, this, {types.begin(), types.end()});
  for (TypeKey* type : types) {
    auto& type_info = types_[type];
    type_info.manager = manager;
    type_info.loader = manager->GetLoader({}, type);
    type_info.release_handler = manager->GetReleaseHandler({}, type);

    std::string_view type_name = type->GetTypeName();
    if (!type_name.empty()) {
      type_names_.try_emplace(type_name, type);
    }
  }
  return true;
}

Resource* ResourceSystem::DoFind(TypeKey* type, ResourceId id) {
  auto it = resources_.find({type, id});
  if (it == resources_.end() || it->second == nullptr ||
      it->second->IsDeleting({})) {
    return nullptr;
  }
  return it->second;
}

Resource* ResourceSystem::DoGet(TypeKey* type, ResourceId id) {
  Resource* resource = DoFind(type, id);
  if (resource == nullptr || !resource->IsVisible({})) {
    return nullptr;
  }
  return resource;
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
  for (Resource* dependency : dependencies) {
    set->Add(dependency);
  }
}

bool ResourceSystem::ReserveResourceName(ResourceInternal, TypeKey* type,
                                         ResourceId id, const std::string& name,
                                         bool* added) {
  *added = false;
  absl::WriterMutexLock lock(&mutex_);
  auto type_it = types_.find(type);
  if (type_it == types_.end()) {
    return false;
  }
  auto [name_it, name_added] = type_it->second.name_to_id.try_emplace(name, id);
  *added = name_added;
  return name_it->second == id;
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
  if (name_it == type_it->second.name_to_id.end() || name_it->second != id) {
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
  if (name_it == type_it->second.name_to_id.end() || name_it->second != id) {
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
                                             std::string_view name,
                                             ValidatedContext context) {
  // TODO: This method can fail erroneously if there are simultaneous load
  //       requests on different threads for the same resource, or if the
  //       resource is deleted on a separate thread immediately after the name
  //       lookup. This should be vanishingly rare in practice.

  // Look up the resource first, to see if it is already loaded.
  {
    absl::ReaderMutexLock lock(&mutex_);
    ResourceId id = DoGetResourceIdFromName(type, name);
    if (id != 0) {
      return DoFind(type, id);
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

  ResourcePtr<Resource> resource = (*loader)(context.GetContext(), type, name);

  {
    absl::WriterMutexLock lock(&mutex_);
    auto it = types_.find(type);
    CHECK(it != types_.end())
        << "Resource manager deleted during resource load";
    if (resource == nullptr) {
      it->second.name_to_id.erase(name_string);
      return nullptr;
    }

    // The loader may return a resource that already has another name, which
    // it no longer has.
    auto id = resource->GetResourceId();
    it->second.name_to_id[name_string] = id;
    std::string& id_name = it->second.id_to_name[id];
    if (!id_name.empty() && id_name != name_string) {
      it->second.name_to_id.erase(id_name);
    }
    id_name = std::move(name_string);
  }

  return resource;
}

void ResourceSystem::RemoveManager(ResourceInternal, ResourceManager* manager) {
  std::vector<Resource*> resources;
  {
    absl::WriterMutexLock lock(&mutex_);
    resources.reserve(resources_.size());
    absl::flat_hash_set<TypeKey*> types;
    for (const auto& [type, type_info] : types_) {
      if (type_info.manager == manager) {
        types.insert(type);
      }
    }
    for (const auto& [key, resource] : resources_) {
      if (types.contains(std::get<TypeKey*>(key)) && resource != nullptr) {
        resources.push_back(resource);
      }
    }
    for (auto type : types) {
      types_.erase(type);
    }
    absl::erase_if(type_names_, [&types](const auto& type_name) {
      return types.contains(type_name.second);
    });
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
  resources_[{resource->GetResourceType(), resource->GetResourceId()}] =
      resource;
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
