#ifndef GBITS_BASE_CONTEXT_TYPE_H_
#define GBITS_BASE_CONTEXT_TYPE_H_

#include <any>
#include <type_traits>

#include "absl/synchronization/mutex.h"

namespace gb {

// Forward declarations.
class Context;
class ContextType;

// Defines a unique key for a type used in a context.
//
// This class is thread-safe.
class ContextKey {
 public:
  ContextKey(const ContextKey&) = delete;
  ContextKey& operator=(const ContextKey&) = delete;

  // Returns the ContextKey for the specified type.
  template <typename Type>
  static ContextKey* Get();

  // Returns the placeholder type associated with this key. The full ContextType
  // is not available, as a key may represent types that are only partially
  // defined (forward declared).
  virtual ContextType* GetPlaceholderType() = 0;

  // Returns the equivalent of the std::type_info::name() function, if the type
  // is a complete type. Otherwise, this will return an empty string.
  virtual const char* GetTypeName() = 0;

  // Explicitly overrides the default type name with the name provided. 'name'
  // must be a pointer that remains valid for as long as it is the type name for
  // this key.
  virtual void SetTypeName(const char* name) = 0;

 protected:
  ContextKey() = default;
  virtual ~ContextKey() = default;

 private:
  template <typename Type>
  class Impl;
};

// A ContextType defines all the operations necessary for using a type with a
// Context. It is an opaque type used only by the Context class.
//
// This class is thread-safe.
class ContextType {
 public:
  ContextType(const ContextType&) = delete;
  ContextType& operator=(const ContextType&) = delete;

  // Returns a ContextType which implements all functions supported for the
  // underlying type.
  template <typename Type>
  static ContextType* Get();

  // Returns a ContextType which does nothing for any of the functions. This is
  // used to represent values that are simply held by pointer (SetPtr/GetPtr
  // functions).
  template <typename Type>
  static ContextType* GetPlaceholder();

  // Returns the associated context key for this type.
  virtual ContextKey* Key() = 0;

  // Returns the name of the type as defined by either a GB_CONTEXT macro or
  // by calling ContextType::Get<Type>(). If neither of these have happened,
  // this will return an empty string.
  const char* GetTypeName() { return Key()->GetTypeName(); }

  // Explicitly overrides the default type name with the name provided. 'name'
  // must be a pointer that remains valid for as long as it is the type name for
  // this key.
  void SetTypeName(const char* name) { Key()->SetTypeName(name); }

 protected:
  friend class Context;

  ContextType() = default;
  virtual ~ContextType() = default;

  // Destroys the value.
  virtual void Destroy(void* value) = 0;

  // Creates a copy of the provided "any" value, iff it has a copy constructor.
  // If it does not, then this will return nullptr.
  virtual void* Clone(const std::any& value) = 0;

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

 private:
  template <typename Type>
  class Impl;

  template <typename Type>
  class PlaceholderImpl;
};

namespace internal {

template <typename Type>
const char* ContextTypeName(Type* stub, const char* new_name = nullptr,
                            bool force_new_name = false) {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for Context. It is likely const, a function "
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
class ContextKey::Impl : public ContextKey {
 public:
  explicit Impl() = default;
  ~Impl() override = default;

  ContextType* GetPlaceholderType() override {
    return ContextType::GetPlaceholder<Type>();
  }

  const char* GetTypeName() override {
    return internal::ContextTypeName<Type>(nullptr);
  }

  void SetTypeName(const char* name) override {
    internal::ContextTypeName<Type>(nullptr, name, true);
  }
};

template <typename Type>
ContextKey* ContextKey::Get() {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for Context. It is likely const, a function "
                "reference (instead of a pointer), or an array.");
  static Impl<Type> s_context_key;
  return &s_context_key;
}

template <typename Type>
class ContextType::Impl : public ContextType {
 public:
  Impl() { internal::ContextTypeName<Type>(nullptr, typeid(Type).name()); }
  ~Impl() override = default;

  ContextKey* Key() override { return ContextKey::Get<Type>(); }

 protected:
  void* Clone(const std::any& value) override { return DoCreate<Type>(value); }
  void Destroy(void* value) override { delete static_cast<Type*>(value); }
};

template <typename Type>
ContextType* ContextType::Get() {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for Context. It is likely const, a function "
                "reference (instead of a pointer), or an array.");
  static Impl<Type> s_context_type;
  return &s_context_type;
}

template <typename Type>
class ContextType::PlaceholderImpl : public ContextType {
 public:
  PlaceholderImpl() = default;
  ~PlaceholderImpl() override = default;

  ContextKey* Key() override { return ContextKey::Get<Type>(); }
  void* Clone(const std::any& value) override { return nullptr; }
  void Destroy(void* value) override {}
};

template <typename Type>
ContextType* ContextType::GetPlaceholder() {
  static_assert(std::is_same_v<Type, std::decay_t<Type>>,
                "Invalid type for Context. It is likely const, a function "
                "reference (instead of a pointer), or an array.");
  static PlaceholderImpl<Type> s_context_type;
  return &s_context_type;
}

}  // namespace gb

#endif  // GBITS_BASE_CONTEXT_TYPE_H_
