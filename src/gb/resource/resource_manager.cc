// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/resource/resource_manager.h"

#include "absl/log/log.h"
#include "gb/resource/resource.h"
#include "gb/resource/resource_system.h"

namespace gb {

ResourceManager::ResourceManager() {}

ResourceManager::~ResourceManager() {
  if (system_ != nullptr) {
    system_->RemoveManager({}, this);
  }
}

void ResourceManager::InitGenericLoader(GenericLoader callback) {
  if (system_ != nullptr) {
    LOG(ERROR)
        << "Generic loader cannot be set after ResourceManager is registered.";
    return;
  }
  if (generic_loader_ != nullptr) {
    LOG(ERROR) << "Generic loader already set.";
    return;
  }
  generic_loader_ = std::move(callback);
}

void ResourceManager::DoInitLoader(TypeKey* type, GenericLoader callback) {
  if (system_ != nullptr) {
    LOG(ERROR) << "Type-specific loader cannot be set after ResourceManager is "
                  "registered.";
    return;
  }
  auto& callbacks = typed_callbacks_[type];
  if (callbacks.loader) {
    LOG(ERROR) << "Type-specific loader already set for type "
               << type->GetTypeName();
    return;
  }
  callbacks.loader = std::move(callback);
}

void ResourceManager::InitGenericReleaseHandler(
    GenericReleaseHandler callback) {
  if (system_ != nullptr) {
    LOG(ERROR) << "Generic release handler cannot be set after ResourceManager "
                  "is registered.";
    return;
  }
  if (generic_release_handler_ != nullptr) {
    LOG(ERROR) << "Generic release handler already set.";
    return;
  }
  generic_release_handler_ = std::move(callback);
}

void ResourceManager::DoInitReleaseHandler(TypeKey* type,
                                           GenericReleaseHandler callback) {
  if (system_ != nullptr) {
    LOG(ERROR) << "Type-specific release handler cannot be set after "
                  "ResourceManager is registered.";
    return;
  }
  auto& callbacks = typed_callbacks_[type];
  if (callbacks.release_handler) {
    LOG(ERROR) << "Type-specific release handler already set for type "
               << type->GetTypeName();
    return;
  }
  callbacks.release_handler = std::move(callback);
}

bool ResourceManager::MaybeDeleteResource(Resource* resource) {
  if (resource == nullptr) {
    return true;
  }
  if (resource->GetResourceSystem() != system_) {
    LOG(ERROR) << "Cannot delete resource "
               << resource->GetResourceType()->GetTypeName() << "("
               << resource->GetResourceId()
               << ") because it is not in the manager's system.";
    return false;
  }
  if (!types_.contains(resource->GetResourceType())) {
    LOG(ERROR) << "Cannot delete resource "
               << resource->GetResourceType()->GetTypeName() << "("
               << resource->GetResourceId()
               << ") because it was created using a different manager.";
    return false;
  }
  return resource->MaybeDelete({});
}

void ResourceManager::SetSystem(ResourceInternal, ResourceSystem* system,
                                absl::flat_hash_set<TypeKey*> types) {
  system_ = system;
  types_ = std::move(types);
}

ResourceManager::GenericLoader* ResourceManager::GetLoader(ResourceInternal,
                                                           TypeKey* type) {
  auto it = typed_callbacks_.find(type);
  if (it != typed_callbacks_.end()) {
    if (it->second.loader) {
      return &it->second.loader;
    }
  }
  if (!generic_loader_) {
    generic_loader_ = [](Context*, TypeKey*, std::string_view) -> Resource* {
      return nullptr;
    };
  }
  return &generic_loader_;
}

ResourceManager::GenericReleaseHandler* ResourceManager::GetReleaseHandler(
    ResourceInternal, TypeKey* type) {
  auto it = typed_callbacks_.find(type);
  if (it != typed_callbacks_.end()) {
    if (it->second.release_handler) {
      return &it->second.release_handler;
    }
  }
  if (!generic_release_handler_) {
    generic_release_handler_ = [this](Resource* resource) {
      MaybeDeleteResource(resource);
    };
  }
  return &generic_release_handler_;
}

ResourceNameReservation ResourceManager::DoReserveResourceName(
    TypeKey* type, ResourceId id, std::string_view name) {
  if (!types_.contains(type)) {
    LOG(ERROR) << "Cannot reserve resource name for type "
               << type->GetTypeName()
               << " as this ResourceManager was not registered with it.";
    return {};
  }
  std::string resource_name(name.data(), name.size());
  if (!system_->ReserveResourceName({}, type, id, resource_name)) {
    return {};
  }
  return ResourceNameReservation({}, system_, type, id, resource_name);
}

ResourceEntry ResourceManager::DoNewResourceEntry(TypeKey* type,
                                                  ResourceId id) {
  if (!types_.contains(type)) {
    LOG(ERROR) << "Cannot create resource entry for type "
               << type->GetTypeName()
               << " as this ResourceManager was not registered with it.";
    return {};
  }
  return system_->NewResourceEntry({}, type, id);
}

}  // namespace gb
