#include "gbits/resource/resource_set.h"

#include "gbits/resource/resource_system.h"

namespace gb {

ResourceSet::ResourceSet(const ResourceSet& other)
    : system_(other.system_), resources_(other.resources_) {
  for (const auto& it : resources_) {
    it.second->AddRef({});
  }
}

ResourceSet::ResourceSet(ResourceSet&& other)
    : system_(std::exchange(other.system_, nullptr)),
      resources_(std::move(other.resources_)) {}

ResourceSet& ResourceSet::operator=(const ResourceSet& other) {
  if (&other == this) {
    return *this;
  }

  absl::flat_hash_map<ResourceKey, Resource*> old_resources;
  resources_.swap(old_resources);

  resources_ = other.resources_;
  for (const auto& it : resources_) {
    it.second->AddRef({});
  }

  for (const auto& it : old_resources) {
    it.second->RemoveRef({});
  }

  system_ = other.system_;
  return *this;
}

ResourceSet& ResourceSet::operator=(ResourceSet&& other) {
  if (&other == this) {
    return *this;
  }

  absl::flat_hash_map<ResourceKey, Resource*> old_resources;
  resources_.swap(old_resources);
  for (const auto& it : old_resources) {
    it.second->RemoveRef({});
  }

  system_ = std::exchange(other.system_, nullptr);
  resources_ = std::move(other.resources_);
  return *this;
}

ResourceSet::~ResourceSet() {
  for (const auto& it : resources_) {
    it.second->RemoveRef({});
  }
}

ResourceId ResourceSet::GetResourceIdFromName(TypeKey* type,
                                              std::string_view name) const {
  if (system_ == nullptr) {
    return 0;
  }
  return system_->GetResourceIdFromName({}, type, name);
}

bool ResourceSet::DoAdd(Resource* resource, bool add_dependencies) {
  if (system_ == nullptr) {
    system_ = resource->GetResourceSystem();
  } else if (system_ != resource->GetResourceSystem()) {
    LOG(ERROR) << "Cannot add resource from different resource system to set.";
    return false;
  }
  auto [it, added] = resources_.try_emplace(
      {resource->GetResourceType(), resource->GetResourceId()}, resource);
  if (added) {
    resource->AddRef({});
    if (add_dependencies) {
      return AddDependencies(resource);
    }
  }
  return true;
}

bool ResourceSet::AddDependencies(Resource* resource) {
  ResourceDependencyList dependencies;
  resource->GetResourceDependencies(&dependencies);
  bool success = true;
  for (auto* resource : dependencies) {
    if (resource != nullptr) {
      success = (DoAdd(resource, true) && success);
    }
  }
  return success;
}

bool ResourceSet::DoRemove(TypeKey* type, ResourceId id,
                           bool remove_dependencies) {
  auto it = resources_.find({type, id});
  if (it == resources_.end()) {
    return true;
  }
  if (remove_dependencies) {
    return RemoveWithDependencies(it->second);
  } else {
    return RemoveResourceOnly(it->second);
  }
}

bool ResourceSet::RemoveResourceOnly(Resource* resource) {
  // If any other resource in the set depends on this resource, then we cannot
  // remove it.
  auto end_it = resources_.end();
  auto resource_it = end_it;
  for (auto it = resources_.begin(); it != end_it; ++it) {
    if (it->second == resource) {
      resource_it = it;
      continue;
    }
    ResourceDependencyList dependencies;
    it->second->GetResourceDependencies(&dependencies);
    for (Resource* dependent : dependencies) {
      if (dependent == resource) {
        return false;
      }
    }
  }

  // Remove the resource, it it was in the set.
  if (resource_it != end_it) {
    resources_.erase(resource_it);
    resource->RemoveRef({});
  }
  if (resources_.empty()) {
    system_ = nullptr;
  }
  return true;
}

bool ResourceSet::RemoveWithDependencies(Resource* resource) {
  // First determine the transitive closure of resource dependencies for this
  // resource. These are the potential resources than may be removed, if there
  // are no other resources in set that are NOT in the transitive closure but
  // refer to one or more of these dependencies.
  absl::flat_hash_set<Resource*> to_remove;
  to_remove.insert(resource);
  AddAllDependencies(&to_remove, resource);

  // Now check whether any resources outside to_remove refer to any resources
  // inside to_remove.
  absl::flat_hash_set<Resource*> keep_resources;
  for (const auto& it : resources_) {
    if (to_remove.find(it.second) != to_remove.end()) {
      continue;
    }
    ResourceDependencyList dependencies;
    it.second->GetResourceDependencies(&dependencies);
    for (Resource* dependent : dependencies) {
      if (to_remove.find(dependent) != to_remove.end()) {
        keep_resources.insert(dependent);
      }
    }
  }
  if (!keep_resources.empty()) {
    // Determine the transitive closure of all resources that need to be kept,
    // then remove them from to_remove.
    absl::flat_hash_set<Resource*> all_keep_resources = keep_resources;
    for (Resource* keep : keep_resources) {
      AddAllDependencies(&all_keep_resources, keep);
    }
    for (Resource* keep : all_keep_resources) {
      to_remove.erase(keep);
    }
  }

  // Finally remove and dereference any remaining resources in to_remove.
  // We do this in two loops so that the set is never referencing a resource
  // that may be deleted.
  for (Resource* remove_resource : to_remove) {
    resources_.erase(
        {remove_resource->GetResourceType(), remove_resource->GetResourceId()});
  }
  for (Resource* remove_resource : to_remove) {
    remove_resource->RemoveRef({});
  }
  if (resources_.empty()) {
    system_ = nullptr;
  }
  return !to_remove.empty();
}

void ResourceSet::AddAllDependencies(
    absl::flat_hash_set<Resource*>* all_dependencies, Resource* resource) {
  ResourceDependencyList dependencies;
  resource->GetResourceDependencies(&dependencies);
  for (Resource* resource : dependencies) {
    if (!resources_.contains(
            {resource->GetResourceType(), resource->GetResourceId()})) {
      continue;
    }
    if (all_dependencies->insert(resource).second) {
      AddAllDependencies(all_dependencies, resource);
    }
  }
}

}  // namespace gb
