// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_RESOURCE_NAME_RESERVATION_H_
#define GB_RESOURCE_RESOURCE_NAME_RESERVATION_H_

#include "gb/resource/resource_types.h"

namespace gb {

class ResourceNameReservation {
 public:
  // Constructs an invalid resource entry. This is only useful as a placeholder
  // to receive a valid entry via move assignment.
  ResourceNameReservation() = default;

  // Constructor called by the ResourceManager when reserving the name.
  ResourceNameReservation(ResourceInternal, ResourceSystem* system,
                          TypeKey* type, ResourceId id, const std::string& name)
      : system_(system), type_(type), id_(id), name_(name) {}

  // Resource name reservations are unique and cannot be copied, only moved.
  ResourceNameReservation(const ResourceNameReservation&) = delete;
  ResourceNameReservation& operator=(const ResourceNameReservation&) = delete;

  // Move construction and assignment.
  ResourceNameReservation(ResourceNameReservation&& other)
      : system_(std::exchange(other.system_, nullptr)),
        type_(std::exchange(other.type_, nullptr)),
        id_(std::exchange(other.id_, 0)),
        name_(std::exchange(other.name_, {})) {}
  ResourceNameReservation& operator=(ResourceNameReservation&& other) {
    if (&other != this) {
      system_ = std::exchange(other.system_, nullptr);
      type_ = std::exchange(other.type_, nullptr);
      id_ = std::exchange(other.id_, 0);
      name_ = std::exchange(other.name_, {});
    }
    return *this;
  }

  // Removes the resource name reservation.
  ~ResourceNameReservation() {
    if (system_ != nullptr) {
      Free();
    }
  }

  // Returns true if the resource entry is valid.
  bool IsValid() const { return system_ != nullptr; }
  operator bool() const { return IsValid(); }

  // Attributes
  ResourceSystem* GetSystem() const { return system_; }
  TypeKey* GetType() const { return type_; }
  ResourceId GetId() const { return id_; }
  std::string_view GetName() const { return name_; }

  // Applies the resource reservation to the resource.
  //
  // This completes and clears the reservation.
  void Apply();

 private:
  void Free();

  ResourceSystem* system_ = nullptr;
  TypeKey* type_ = nullptr;
  ResourceId id_ = 0;
  std::string name_;
};

}  // namespace gb

#endif  // GB_RESOURCE_RESOURCE_NAME_RESERVATION_H_
