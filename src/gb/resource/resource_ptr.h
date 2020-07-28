// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_RESOURCE_PTR_H_
#define GB_RESOURCE_RESOURCE_PTR_H_

#include <utility>

#include "gb/resource/resource.h"

namespace gb {

// Helper for ResourcePtr
class ResourcePtrBase {
 protected:
  ResourcePtrBase() = default;
  ResourcePtrBase(Resource* resource);
  ResourcePtrBase(const ResourcePtrBase& other);
  ResourcePtrBase(ResourcePtrBase&& other);
  ResourcePtrBase& operator=(const ResourcePtrBase& other) = delete;
  ResourcePtrBase& operator=(ResourcePtrBase&& other) = delete;
  ~ResourcePtrBase();

  void DoMove(ResourcePtrBase&& other);
  void DoReset(Resource* resource);

  Resource* resource_ = nullptr;
};

inline ResourcePtrBase::ResourcePtrBase(Resource* resource)
    : resource_(resource) {
  if (resource_ != nullptr) {
    resource_->AddRef({});
  }
}

inline ResourcePtrBase::ResourcePtrBase(const ResourcePtrBase& other)
    : resource_(other.resource_) {
  if (resource_ != nullptr) {
    resource_->AddRef({});
  }
}

inline ResourcePtrBase::ResourcePtrBase(ResourcePtrBase&& other)
    : resource_(std::exchange(other.resource_, nullptr)) {}

inline ResourcePtrBase::~ResourcePtrBase() {
  if (resource_ != nullptr) {
    resource_->RemoveRef({});
  }
}

inline void ResourcePtrBase::DoMove(ResourcePtrBase&& other) {
  if (&other != this) {
    if (resource_ != nullptr) {
      resource_->RemoveRef({});
    }
    resource_ = std::exchange(other.resource_, nullptr);
  }
}

inline void ResourcePtrBase::DoReset(Resource* resource) {
  if (resource_ == resource) {
    return;
  }
  if (resource_ != nullptr) {
    resource_->RemoveRef({});
  }
  resource_ = resource;
  if (resource_ != nullptr) {
    resource_->AddRef({});
  }
}

// This smart pointer class manages shared ownership to a single resource.
//
// As long as this smart pointer exists, its referenced resource will not be
// deleted by its associated resource manager.
//
// This class is thread-compatible, although multiple ResourcePtr and
// ResourceSet instances referring to the same resource are thread-safe relative
// to each other.
template <typename Type>
class ResourcePtr final : public ResourcePtrBase {
  static_assert(std::is_base_of_v<Resource, Type>, "Type is not a resource");

 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  ResourcePtr() = default;
  ResourcePtr(nullptr_t) {}
  ResourcePtr(Type* resource) : ResourcePtrBase(resource) {}
  ResourcePtr(const ResourcePtr<Type>& other) : ResourcePtrBase(other) {}
  template <typename OtherType>
  ResourcePtr(const ResourcePtr<OtherType>& other) : ResourcePtrBase(other) {
    static_assert(std::is_base_of_v<Type, OtherType>,
                  "Invalid resource type for assignment");
  }
  ResourcePtr(ResourcePtr<Type>&& other) : ResourcePtrBase(std::move(other)) {}
  template <typename OtherType>
  ResourcePtr(ResourcePtr<OtherType>&& other)
      : ResourcePtrBase(std::move(other)) {
    static_assert(std::is_base_of_v<Type, OtherType>,
                  "Invalid resource type for assignment");
  }
  template <typename OtherType>
  ResourcePtr<Type>& operator=(OtherType* other) {
    static_assert(std::is_base_of_v<Type, OtherType>,
                  "Invalid resource type for assignment");
    DoReset(other);
    return *this;
  }
  ResourcePtr<Type>& operator=(const ResourcePtr<Type>& other) {
    DoReset(other.resource_);
    return *this;
  }
  template <typename OtherType>
  ResourcePtr<Type>& operator=(const ResourcePtr<OtherType>& other) {
    static_assert(std::is_base_of_v<Type, OtherType>,
                  "Invalid resource type for assignment");
    DoReset(other.Get());
    return *this;
  }
  ResourcePtr<Type>& operator=(ResourcePtr<Type>&& other) {
    DoMove(std::move(other));
    return *this;
  }
  template <typename OtherType>
  ResourcePtr<Type>& operator=(ResourcePtr<OtherType>&& other) {
    static_assert(std::is_base_of_v<Type, OtherType>,
                  "Invalid resource type for assignment");
    DoMove(std::move(other));
    return *this;
  }
  ~ResourcePtr() = default;

  //----------------------------------------------------------------------------
  // Smart pointer accessors
  //----------------------------------------------------------------------------

  Type* Get() const { return static_cast<Type*>(resource_); }
  Type& operator*() const { return *static_cast<Type*>(resource_); }
  Type* operator->() const { return static_cast<Type*>(resource_); }
  operator bool() const { return resource_ != nullptr; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  void Reset(Type* resource = nullptr) { DoReset(resource); }
};

template <typename Type>
inline bool operator==(const ResourcePtr<Type>& ptr, nullptr_t) {
  return !ptr;
}

template <typename Type>
inline bool operator==(nullptr_t, const ResourcePtr<Type>& ptr) {
  return !ptr;
}

template <typename Type>
inline bool operator!=(const ResourcePtr<Type>& ptr, nullptr_t) {
  return ptr;
}

template <typename Type>
inline bool operator!=(nullptr_t, const ResourcePtr<Type>& ptr) {
  return ptr;
}

}  // namespace gb

#endif  // GB_RESOURCE_RESOURCE_PTR_H_
