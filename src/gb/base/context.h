#ifndef GB_BASE_CONTEXT_H_
#define GB_BASE_CONTEXT_H_

#include <string_view>
#include <tuple>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "gb/base/type_info.h"
#include "gb/base/weak_ptr.h"

namespace gb {

// This class contains a set of values keyed by type and an optional name.
//
// Only one anonymous value (no name) of each type may be stored, and only one
// value of each name (regardless of the type) may be stored. The values stored
// in the context are never const.
//
// This class is thread-safe. However, there is no implied thread-safety
// guarantees for methods called on contained values. These methods are always
// called outside of any synchronization primitive to allow further calls into
// the Context. For instance, it is up to the caller to ensure that a specific
// value is not destructed while simultaneously being assigned from a separate
// thread. For this reason (and others) it is recommended that complex objects
// are stored by pointer instead of by value.
class Context final : public WeakScope<Context> {
 public:
  Context() : WeakScope<Context>(this) {}
  Context(const Context&) = delete;
  Context(Context&&);
  Context& operator=(const Context&) = delete;
  Context& operator=(Context&&);
  ~Context() {
    InvalidateWeakPtrs();
    Reset();
  }

  // Sets a parent context for this context.
  //
  // A parent context is used when a lookup in this context fails, at which
  // point it is looked up in the parent's context (which may fall back on its
  // parent context, etc). Any Set* methods on this context will hide any
  // corresponding value in the parent context, but will not modify it.
  // Similarly, clearing a value in this context will simply unhide the
  // corresponding value in the parent context.
  void SetParent(WeakPtr<const Context> parent) {
    absl::WriterMutexLock lock(&mutex_);
    parent_ = parent;
  }

  // Returns the parent context for this context.
  WeakPtr<const Context> GetParent() const {
    absl::ReaderMutexLock lock(&mutex_);
    return parent_;
  }

  // Returns true if there are no values stored in the context.
  bool Empty() const {
    absl::ReaderMutexLock lock(&mutex_);
    return values_.empty();
  }

  // Resets the context to be empty. All owned values are destructed.
  void Reset();

  // Constructs a new value of the specified type with the provided constructor
  // parameters.
  //
  // Any existing owned value will be destructed and replaced with the new
  // value.
  template <typename Type, class... Args>
  void SetNew(Args&&... args) {
    SetImpl({}, TypeInfo::Get<Type>(), new Type(std::forward<Args>(args)...),
            true);
  }
  template <typename Type, class... Args>
  void SetNamedNew(std::string_view name, Args&&... args) {
    SetImpl(name, TypeInfo::Get<Type>(), new Type(std::forward<Args>(args)...),
            true);
  }

  // Sets the value of the specified Type with the context taking ownership.
  //
  // Any existing owned value will be destructed and replaced with the new
  // value. If the new value is the same as the old value, then no destruction
  // will not occur -- only the ownership may change (from unowned to owned). If
  // a null is passed in, this is equivalent to calling Clear<Type> with no
  // value being stored.
  //
  // Unlike SetNew and SetValue, SetOwned does not require explicit Type
  // specification as it is unambiguous.
  template <typename Type>
  void SetOwned(std::unique_ptr<Type> value) {
    SetImpl({}, TypeInfo::Get<Type>(), value.release(), true);
  }
  template <typename Type>
  void SetOwned(std::string_view name, std::unique_ptr<Type> value) {
    SetImpl(name, TypeInfo::Get<Type>(), value.release(), true);
  }

  // Sets the value of the specified Type without the context taking ownership.
  //
  // Any existing owned value will be destructed and replaced with the new
  // value. If the new value is the same as the old value, then no destruction
  // will not occur -- only the ownership may change (from owned to unowned). If
  // a null is passed in, this is equivalent to calling Clear<Type> with no
  // value being stored.
  //
  // Unlike SetNew and SetValue, SetPtr does not require explicit Type
  // specification as it is unambiguous.
  template <typename Type>
  void SetPtr(Type* value) {
    SetImpl({}, TypeInfo::GetPlaceholder<Type>(), value, false);
  }
  template <typename Type>
  void SetPtr(std::string_view name, Type* value) {
    SetImpl(name, TypeInfo::GetPlaceholder<Type>(), value, false);
  }

  // Updates the value of the specified Type in the context with the new value.
  //
  // The Type must support copy or move construction. If the Type supports copy
  // or move assignment and there is an existing value in the context, then the
  // corresponding supported assignment will take place. If assignment is not
  // supported but there is an existing owned value, the old value will be
  // destructed and a new value will be copy/move constructed from the provided
  // value.
  //
  // Unlike SetPtr and SetOwned, SetValue always requires explicit Type
  // specification, as implicit specification can easily cause ambiguity and
  // hard to track bugs. This also allows assignment support for other types
  // (assigning a const char* to a std::string, for instance).
  template <
      typename Type, typename OtherType,
      std::enable_if_t<std::is_assignable<std::decay_t<Type>, OtherType>::value,
                       int> = 0>
  void SetValue(OtherType&& value) {
    mutex_.ReaderLock();
    Type* old_value = Lookup<Type>();
    mutex_.ReaderUnlock();
    if (old_value != nullptr) {
      *old_value = std::forward<OtherType>(value);
    } else {
      SetImpl({}, TypeInfo::Get<Type>(),
              new Type(std::forward<OtherType>(value)), true);
    }
  }
  template <
      typename Type, typename OtherType,
      std::enable_if_t<std::is_assignable<std::decay_t<Type>, OtherType>::value,
                       int> = 0>
  void SetValue(std::string_view name, OtherType&& value) {
    mutex_.ReaderLock();
    Type* old_value = Lookup<Type>(name);
    mutex_.ReaderUnlock();
    if (old_value != nullptr) {
      *old_value = std::forward<OtherType>(value);
    } else {
      SetImpl(name, TypeInfo::Get<Type>(),
              new Type(std::forward<OtherType>(value)), true);
    }
  }
  template <typename Type, typename OtherType,
            std::enable_if_t<std::negation<std::is_assignable<
                                 std::decay_t<Type>, OtherType>>::value,
                             int> = 0>
  void SetValue(OtherType&& value) {
    SetImpl({}, TypeInfo::Get<Type>(), new Type(std::forward<OtherType>(value)),
            true);
  }
  template <typename Type, typename OtherType,
            std::enable_if_t<std::negation<std::is_assignable<
                                 std::decay_t<Type>, OtherType>>::value,
                             int> = 0>
  void SetValue(std::string_view name, OtherType&& value) {
    SetImpl(name, TypeInfo::Get<Type>(),
            new Type(std::forward<OtherType>(value)), true);
  }

  // Sets a value based on an std::any and pre-determined TypeInfo.
  //
  // If the std::any does not have a value, or it is not of the exact same type
  // as the TypeInfo represents, then this set the value to null (equivalent
  // to calling Clear<Type>()).
  void SetAny(TypeInfo* type, const std::any& value) {
    return SetAny({}, type, value);
  }
  void SetAny(std::string_view name, TypeInfo* type, const std::any& value) {
    if (type != nullptr) {
      SetImpl(name, type, type->Clone(value), true);
    }
  }

  // Returns a pointer to the stored value of the specified Type if there is
  // one, or null otherwise.
  template <typename Type>
  Type* GetPtr(std::string_view name = {}) const {
    return GetPtrImpl<Type>(name);
  }

  // Returns the stored value of the specified Type if there is one, or a
  // default constructed value if there isn't. The Type must support copy
  // construction.
  template <typename Type>
  Type GetValue(std::string_view name = {}) const {
    Type* value = GetPtrImpl<Type>(name);
    if (value != nullptr) {
      return *value;
    }
    return Type{};
  }

  // Returns the stored value of the specified Type if there is one, or the
  // specified default value if there isn't. The Type must support copy
  // construction.
  template <typename Type, typename DefaultType>
  Type GetValueOrDefault(DefaultType&& default_value) const {
    Type* value = GetPtrImpl<Type>();
    if (value != nullptr) {
      return *value;
    }
    return std::forward<DefaultType>(default_value);
  }
  template <typename Type, typename DefaultType>
  Type GetValueOrDefault(std::string_view name,
                         DefaultType&& default_value) const {
    Type* value = GetPtrImpl<Type>(name);
    if (value != nullptr) {
      return *value;
    }
    return std::forward<DefaultType>(default_value);
  }

  // Returns true if a value of the specified Type exists in the context.
  //
  // This is equivalent to (GetPtr<Type>() != nullptr)
  template <typename Type>
  bool Exists(std::string_view name = {}) const {
    return Exists(name, TypeKey::Get<Type>());
  }
  bool Exists(TypeKey* key) const { return Exists({}, key); }
  bool Exists(std::string_view name, TypeKey* key) const {
    WeakLock<const Context> parent;
    {
      absl::ReaderMutexLock lock(&mutex_);
      if (values_.find({name, key}) != values_.end()) {
        return true;
      }
      parent = parent_.Lock();
    }
    return parent != nullptr && parent->Exists(name, key);
  }

  // Returns true if a value of the specified name exists in the context.
  //
  // If the name is empty, this always returns false.
  bool NameExists(std::string_view name) const {
    WeakLock<const Context> parent;
    {
      absl::ReaderMutexLock lock(&mutex_);
      if (names_.find(name) != names_.end()) {
        return true;
      }
      parent = parent_.Lock();
    }
    return parent != nullptr && parent->NameExists(name);
  }

  // Returns true if a value of the specified Type exists in the context AND it
  // is owned by the context.
  template <typename Type>
  bool Owned(std::string_view name = {}) const {
    absl::ReaderMutexLock lock(&mutex_);
    auto it = values_.find({name, TypeKey::Get<Type>()});
    return it != values_.end() && it->second.owned;
  }

  // Releases ownership of the value of the specified Type to the caller.
  //
  // If the value does not exist or is not owned (Owned<Type>() would return
  // false), then this will return null.
  template <typename Type>
  std::unique_ptr<Type> Release(std::string_view name = {}) {
    absl::WriterMutexLock lock(&mutex_);
    return std::unique_ptr<Type>(
        static_cast<Type*>(ReleaseImpl(name, TypeInfo::Get<Type>())));
  }

  // Clears any value of the specified Type from the context (if one existed).
  //
  // If the value exists and is owned, it will be destructed.
  template <typename Type>
  void Clear(std::string_view name = {}) {
    return Clear(name, TypeKey::Get<Type>());
  }
  void Clear(TypeKey* key) { return Clear({}, key); }
  void Clear(std::string_view name, TypeKey* key) {
    SetImpl(name, key->GetPlaceholderType(), nullptr, false);
  }

  // Clears any value of the specified name from the context (if one existed).
  //
  // If the name is empty, this has no effect.
  void ClearName(std::string_view name) {
    TypeInfo* type = nullptr;

    mutex_.ReaderLock();
    auto name_it = names_.find(name);
    if (name_it != names_.end()) {
      type = name_it->second;
    }
    mutex_.ReaderUnlock();

    if (type != nullptr) {
      SetImpl(name, type, nullptr, false);
    }
  }

 private:
  struct Value {
    Value() = default;
    TypeInfo* type = nullptr;
    char* name = nullptr;
    void* value = nullptr;
    bool owned = false;
  };
  using Values =
      absl::flat_hash_map<std::tuple<std::string_view, TypeKey*>, Value>;
  using Names = absl::flat_hash_map<std::string_view, TypeInfo*>;

  template <typename Type>
  Type* Lookup(std::string_view name = {}) const
      ABSL_SHARED_LOCKS_REQUIRED(mutex_) {
    auto it = values_.find({name, TypeKey::Get<Type>()});
    return (it != values_.end() ? static_cast<Type*>(it->second.value)
                                : nullptr);
  }

  template <typename Type>
  Type* GetPtrImpl(std::string_view name = {}) const
      ABSL_LOCKS_EXCLUDED(mutex_) {
    Type* result = nullptr;
    WeakLock<const Context> parent;
    {
      absl::ReaderMutexLock lock(&mutex_);
      result = Lookup<Type>(name);
      if (result != nullptr) {
        return result;
      }
      parent = parent_.Lock();
    }
    return (parent != nullptr ? parent->GetPtrImpl<Type>(name) : nullptr);
  }
  void SetImpl(std::string_view name, TypeInfo* type, void* new_value,
               bool owned) ABSL_LOCKS_EXCLUDED(mutex_);
  void* ReleaseImpl(std::string_view name, TypeInfo* type)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  WeakPtr<const Context> parent_ ABSL_GUARDED_BY(mutex_);
  Values values_ ABSL_GUARDED_BY(mutex_);
  Names names_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace gb

#endif  // GB_BASE_CONTEXT_H_
