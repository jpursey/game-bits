#ifndef GB_BASE_TYPE_INFO_H_
#define GB_BASE_TYPE_INFO_H_

#include <any>
#include <type_traits>

#include "absl/synchronization/mutex.h"

namespace gb {

// Forward declarations.
class TypeInfo;

// Defines a unique key for a type.
//
// A TypeKey can always be retrieved, even if the type is not fully defined (for
// example, forward declared). TypeKeys are not supported for CV qualified
// types.
//
// This class is thread-safe.
class TypeKey {
 public:
  TypeKey(const TypeKey&) = delete;
  TypeKey& operator=(const TypeKey&) = delete;

  // Returns the TypeKey for the specified type.
  template <typename Type>
  static TypeKey* Get();

  // Returns the placeholder type associated with this key. The full TypeInfo
  // is not available, as a key may represent types that are only partially
  // defined (forward declared).
  virtual TypeInfo* GetPlaceholderType() = 0;

  // Returns the equivalent of the std::type_info::name() function by default,
  // if the type is a complete type. Otherwise, this will return an empty
  // string. SetTypeName may be called to explicitly set this name.
  virtual const char* GetTypeName() = 0;

  // Explicitly overrides the default type name with the name provided. 'name'
  // must be a pointer that remains valid for as long as it is the type name for
  // this key.
  virtual void SetTypeName(const char* name) = 0;

 protected:
  TypeKey() = default;
  virtual ~TypeKey() = default;

 private:
  template <typename Type>
  class Impl;
};

// A TypeInfo defines operations for using a type anonymously.
//
// TypeInfo is not supported for CV qualified types.
//
// This class is thread-safe.
class TypeInfo {
 public:
  TypeInfo(const TypeInfo&) = delete;
  TypeInfo& operator=(const TypeInfo&) = delete;

  // Returns a TypeInfo which implements all functions supported for the
  // underlying type.
  template <typename Type>
  static TypeInfo* Get();

  // Returns a TypeInfo which does nothing for any of the functions. This is
  // used to represent values that are simply held by pointer (SetPtr/GetPtr
  // functions).
  template <typename Type>
  static TypeInfo* GetPlaceholder();

  // Returns the associated type key for this type.
  virtual TypeKey* Key() = 0;

  // Returns the equivalent of the std::type_info::name() function by default,
  // if the type is a complete type. Otherwise, this will return an empty
  // string. SetTypeName may be called to explicitly set this name.
  const char* GetTypeName() { return Key()->GetTypeName(); }

  // Explicitly overrides the default type name with the name provided. 'name'
  // must be a pointer that remains valid for as long as it is the type name for
  // this key.
  void SetTypeName(const char* name) { Key()->SetTypeName(name); }

  // Returns true if the type has a public destructor.
  virtual bool CanDestroy() = 0;

  // Destroys the value, iff it has a public destructor. If it does not, then
  // this will do nothing.
  virtual void Destroy(void* value) = 0;

  // Returns true if the type has a public copy constructor.
  virtual bool CanClone() = 0;

  // Creates a copy of the provided value, iff it has a public copy constructor.
  // If it does not, then this will return nullptr.
  virtual void* Clone(const std::any& value) = 0;
  virtual void* Clone(const void* value) = 0;

 protected:
  TypeInfo() = default;
  virtual ~TypeInfo() = default;

  template <typename Type,
            std::enable_if_t<std::is_destructible<Type>::value, int> = 0>
  static void DoDestroy(void* value) {
    delete static_cast<Type*>(value);
  }
  template <typename Type,
            std::enable_if_t<std::negation<std::is_destructible<Type>>::value,
                             int> = 0>
  static void DoDestroy(void* value) {}

  template <typename Type,
            std::enable_if_t<std::is_copy_constructible<Type>::value, int> = 0>
  static void* DoCreate(const std::any& any_value) {
    if (!any_value.has_value()) {
      return nullptr;
    }
    const Type* typed_value = std::any_cast<Type>(&any_value);
    if (typed_value == nullptr) {
      return nullptr;
    }
    return new Type(*typed_value);
  }
  template <
      typename Type,
      std::enable_if_t<std::negation<std::is_copy_constructible<Type>>::value,
                       int> = 0>
  static void* DoCreate(const std::any& any_value) {
    return nullptr;
  }

  template <typename Type,
            std::enable_if_t<std::is_copy_constructible<Type>::value, int> = 0>
  static void* DoCreate(const void* value) {
    if (value == nullptr) {
      return nullptr;
    }
    const Type* typed_value = static_cast<const Type*>(value);
    if (typed_value == nullptr) {
      return nullptr;
    }
    return new Type(*typed_value);
  }
  template <
      typename Type,
      std::enable_if_t<std::negation<std::is_copy_constructible<Type>>::value,
                       int> = 0>
  static void* DoCreate(const void* value) {
    return nullptr;
  }

 private:
  template <typename Type>
  class Impl;

  template <typename Type>
  class PlaceholderImpl;
};

namespace internal {

template <typename Type>
const char* TypeInfoName(Type* stub, const char* new_name = nullptr,
                         bool force_new_name = false) {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for TypeInfo. It is likely const, a function "
                "reference (instead of a pointer), or an array.");
  ABSL_CONST_INIT static absl::Mutex mutex(absl::kConstInit);
  absl::MutexLock lock(&mutex);

  // Stub is typed to force a new instance of the functon per type, to prevent
  // linker optimizations that could collapse the functions. Note: This is pure
  // paranoia, and may not be necessary per-standard.
  static std::pair<Type*, const char*> info = {stub, new_name};
  if (info.second == nullptr || force_new_name) {
    info.second = new_name;
  }
  return (info.second != nullptr ? info.second : "");
}

}  // namespace internal

template <typename Type>
class TypeKey::Impl : public TypeKey {
 public:
  explicit Impl() = default;
  ~Impl() override = default;

  TypeInfo* GetPlaceholderType() override {
    return TypeInfo::GetPlaceholder<Type>();
  }

  const char* GetTypeName() override {
    return internal::TypeInfoName<Type>(nullptr);
  }

  void SetTypeName(const char* name) override {
    internal::TypeInfoName<Type>(nullptr, name, true);
  }
};

template <typename Type>
TypeKey* TypeKey::Get() {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for TypeInfo. It is likely const, a function "
                "reference (instead of a pointer), or an array.");
  static Impl<Type> s_type_key;
  return &s_type_key;
}

template <typename Type>
class TypeInfo::Impl : public TypeInfo {
 public:
  Impl() { internal::TypeInfoName<Type>(nullptr, typeid(Type).name()); }
  ~Impl() override = default;

  TypeKey* Key() override { return TypeKey::Get<Type>(); }
  bool CanDestroy() override { return std::is_destructible<Type>::value; }
  void Destroy(void* value) override { DoDestroy<Type>(value); }
  bool CanClone() override { return std::is_copy_constructible<Type>::value; }
  void* Clone(const std::any& value) override { return DoCreate<Type>(value); }
  void* Clone(const void* value) override { return DoCreate<Type>(value); }
};

template <typename Type>
TypeInfo* TypeInfo::Get() {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for TypeInfo. It is likely const, a function "
                "reference (instead of a pointer), or an array.");
  static Impl<Type> s_type;
  return &s_type;
}

template <typename Type>
class TypeInfo::PlaceholderImpl : public TypeInfo {
 public:
  PlaceholderImpl() = default;
  ~PlaceholderImpl() override = default;

  TypeKey* Key() override { return TypeKey::Get<Type>(); }
  bool CanDestroy() override { return false; }
  void Destroy(void* value) override {}
  bool CanClone() override { return false; }
  void* Clone(const std::any& value) override { return nullptr; }
  void* Clone(const void* value) override { return nullptr; }
};

template <typename Type>
TypeInfo* TypeInfo::GetPlaceholder() {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for TypeInfo. It is likely const, a function "
                "reference (instead of a pointer), or an array.");
  static PlaceholderImpl<Type> s_type;
  return &s_type;
}

}  // namespace gb

#endif  // GB_BASE_TYPE_INFO_H_
