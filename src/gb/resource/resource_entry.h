// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_RESOURCE_ENTRY_H_
#define GB_RESOURCE_RESOURCE_ENTRY_H_

#include "gb/resource/resource_types.h"

namespace gb {

// A resource entry represents a unique entry within a specific ResourceSystem.
// This must be passed to all newly created resources to associate them with the
// ResourceSystem they are allocated to.
class ResourceEntry {
 public:
  // Constructs an invalid resource entry. This is only useful as a placeholder
  // to receive a valid entry via move assignment.
  ResourceEntry() = default;

  // Constructor called by the ResourceSystem when allocating a resource in the
  // system.
  ResourceEntry(ResourceInternal, ResourceSystem* system, TypeKey* type,
                ResourceId id)
      : system_(system), type_(type), id_(id) {}

  // Resource entries are unique and cannot be copied, only moved.
  ResourceEntry(const ResourceEntry&) = delete;
  ResourceEntry& operator=(const ResourceEntry&) = delete;

  // Move construction and assignment.
  ResourceEntry(ResourceEntry&& other)
      : system_(std::exchange(other.system_, nullptr)),
        type_(std::exchange(other.type_, nullptr)),
        id_(std::exchange(other.id_, 0)) {}
  ResourceEntry& operator=(ResourceEntry&& other) {
    if (&other != this) {
      system_ = std::exchange(other.system_, nullptr);
      type_ = std::exchange(other.type_, nullptr);
      id_ = std::exchange(other.id_, 0);
    }
    return *this;
  }

  // Removes the resource entry, allowing it to be allocated again.
  ~ResourceEntry() {
    if (system_ != nullptr) {
      Free();
    }
  }

  // Returns true if the resource entry is valid.
  operator bool() const { return system_ != nullptr; }

  // Attributes
  ResourceSystem* GetSystem() const { return system_; }
  TypeKey* GetType() const { return type_; }
  ResourceId GetId() const { return id_; }
  std::string_view GetName() const;

 private:
  void Free();

  ResourceSystem* system_ = nullptr;
  TypeKey* type_ = nullptr;
  ResourceId id_ = 0;
};

}  // namespace gb

#endif  // GB_RESOURCE_RESOURCE_ENTRY_H_
