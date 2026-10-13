// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_RESOURCE_NAME_RESERVATION_H_
#define GB_RESOURCE_RESOURCE_NAME_RESERVATION_H_

#include <string>
#include <string_view>
#include <utility>

#include "gb/base/type_info.h"
#include "gb/resource/resource_types.h"

namespace gb {

// A reservation of a resource name for a resource, which keeps any other
// resource of the same type from taking the name until it is applied or
// destroyed.
//
// Reserving a name that the resource already has or has reserved succeeds,
// but destroying that reservation leaves the name with the resource.
class ResourceNameReservation {
 public:
  // Constructs an invalid reservation. This is only useful as a placeholder to
  // receive a valid reservation via move assignment.
  ResourceNameReservation() = default;

  // Constructor called by the ResourceManager when reserving the name.
  // "owns_name" is true if the reservation added the name, so releases it.
  ResourceNameReservation(ResourceInternal, ResourceSystem* system,
                          TypeKey* type, ResourceId id, const std::string& name,
                          bool owns_name)
      : system_(system),
        type_(type),
        id_(id),
        name_(name),
        owns_name_(owns_name) {}

  // Resource name reservations are unique and cannot be copied, only moved.
  ResourceNameReservation(const ResourceNameReservation&) = delete;
  ResourceNameReservation& operator=(const ResourceNameReservation&) = delete;

  // Move construction and assignment.
  ResourceNameReservation(ResourceNameReservation&& other)
      : system_(std::exchange(other.system_, nullptr)),
        type_(std::exchange(other.type_, nullptr)),
        id_(std::exchange(other.id_, 0)),
        name_(std::exchange(other.name_, {})),
        owns_name_(std::exchange(other.owns_name_, false)) {}
  ResourceNameReservation& operator=(ResourceNameReservation&& other) {
    if (&other != this) {
      Free();
      system_ = std::exchange(other.system_, nullptr);
      type_ = std::exchange(other.type_, nullptr);
      id_ = std::exchange(other.id_, 0);
      name_ = std::exchange(other.name_, {});
      owns_name_ = std::exchange(other.owns_name_, false);
    }
    return *this;
  }

  // Removes the resource name reservation.
  ~ResourceNameReservation() { Free(); }

  // Returns true if the reservation is valid.
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
  bool owns_name_ = false;
};

}  // namespace gb

#endif  // GB_RESOURCE_RESOURCE_NAME_RESERVATION_H_
