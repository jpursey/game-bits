// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_WEAK_PTR_H
#define GB_BASE_WEAK_PTR_H

#include <memory>
#include <utility>

#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"

namespace gb {

namespace internal {

// Internal structure used by Weak* classes, shared by a WeakScope and every
// WeakPtr and WeakLock from it.
class WeakPtrData {
 public:
  explicit WeakPtrData(void* ptr) : ptr_(ptr) {}

  // Sets the pointer to null, then waits for all reader locks to be released.
  void Invalidate();

  // Takes a reader lock and returns the pointer, if it isn't null. Otherwise
  // returns null without taking a lock.
  void* ReaderLock();
  void ReaderUnlock();

 private:
  absl::Mutex mutex_;
  void* ptr_ ABSL_GUARDED_BY(mutex_);
  int count_ ABSL_GUARDED_BY(mutex_) = 0;  // Number of WeakLocks active.
};

}  // namespace internal

template <typename Type>
class WeakPtr;
template <typename Type>
class WeakLock;
template <typename Type>
class WeakScope;

// This class represents a weak reference to an instance of Type.
//
// This is roughly equivalent in functionality to std::weak_ptr, except that it
// does not require shared ownership of the underlying instance. Instead, the
// WeakPtr is tied to a WeakScope, which controls how long it stays valid. To
// access the underlying pointer, it must be locked (via constructing a
// WeakLock<Type>), which is null once the scope is invalidated. While a lock is
// held, invalidating the scope blocks until it is released. See WeakScope for
// examples.
//
// This class is thread-compatible. However, all WeakPtr, WeakLock, and
// WeakScope instances are thread-safe relative to each other. In other words,
// each independent instance of a WeakPtr, WeakLock, or WeakScope can be
// accessed or destructed safely from independent threads.
template <typename Type>
class WeakPtr {
 public:
  // Constructs a null WeakPtr.
  WeakPtr() = default;
  WeakPtr(nullptr_t) : WeakPtr() {}

  // Constructs a WeakPtr from a scope that can supply a WeakPtr of this Type.
  //
  // If "scope" is null, then the WeakPtr will also be null.
  template <typename Scope>
  WeakPtr(Scope* scope);
  template <typename Scope, typename Deleter>
  WeakPtr(const std::unique_ptr<Scope, Deleter>& scope)
      : WeakPtr(scope.get()) {}
  template <typename Scope>
  WeakPtr(const std::shared_ptr<Scope>& scope) : WeakPtr(scope.get()) {}

  // Returns a WeakLock to this WeakPtr.
  WeakLock<Type> Lock() const;

  // WeakPtr supports all standard copy and assignment methods.
  WeakPtr(const WeakPtr&) = default;
  WeakPtr(WeakPtr&&) = default;
  WeakPtr& operator=(const WeakPtr&) = default;
  WeakPtr& operator=(WeakPtr&&) = default;
  ~WeakPtr() = default;

 private:
  template <typename OtherType>
  friend class WeakScope;
  friend class WeakLock<Type>;

  explicit WeakPtr(std::shared_ptr<internal::WeakPtrData> data)
      : data_(std::move(data)) {}

  std::shared_ptr<internal::WeakPtrData> data_;
};

// This class locks a WeakPtr<Type> providing a stable pointer to the underlying
// instance.
//
// While any non-null WeakLock exists, invalidating its WeakScope blocks until
// the lock is released.
//
// This class is thread-compatible. However, all WeakPtr, WeakLock, and
// WeakScope instances are thread-safe relative to each other. In other words,
// each independent instance of a WeakPtr, WeakLock, or WeakScope can be
// accessed or destructed safely from independent threads.
template <typename Type>
class WeakLock final {
 public:
  // Constructs a WeakLock from a WeakPtr.
  //
  // The lock is null if the WeakPtr is null, or its scope has started being
  // invalidated. Otherwise, the scope can't finish being invalidated until the
  // WeakLock is destructed.
  explicit WeakLock(const WeakPtr<Type>* ptr);

  // Construct a WeakLock from nullptr.
  explicit WeakLock(nullptr_t) {}

  // Default construction.
  WeakLock() = default;

  // WeakLock supports all standard copy and assignment methods. A copy is a new
  // lock, so it is null if the scope has started being invalidated.
  WeakLock(const WeakLock& other);
  WeakLock(WeakLock&& other);
  WeakLock& operator=(const WeakLock& other);
  WeakLock& operator=(WeakLock&& other);
  ~WeakLock();

  // WeakLock supports standard smart-pointer operations.
  Type* Get() const { return ptr_; }
  Type& operator*() const { return *ptr_; }
  Type* operator->() const { return ptr_; }
  operator bool() const { return ptr_ != nullptr; }

 private:
  std::shared_ptr<internal::WeakPtrData> data_;
  Type* ptr_ = nullptr;
};

template <typename Type>
inline bool operator==(const WeakLock<Type>& lock, nullptr_t) {
  return !lock;
}

template <typename Type>
inline bool operator==(nullptr_t, const WeakLock<Type>& lock) {
  return !lock;
}

template <typename Type>
inline bool operator!=(const WeakLock<Type>& lock, nullptr_t) {
  return lock;
}

template <typename Type>
inline bool operator!=(nullptr_t, const WeakLock<Type>& lock) {
  return lock;
}

// This class controls the scope of when generated WeakPtr classes remain valid
// (not nullptr).
//
// A WeakScope is required to create a WeakPtr. All created WeakPtr instances
// will remain valid until Invalidate() is called on the scope. Invalidate()
// *must* be called before the scope is destructed. This is to help ensure
// proper implementation in the most common use-case which is a class inheriting
// from a WeakScope. In this case, it *must* call Invalidate in its destructor
// while it is still valid to access, otherwise the weak pointers may not be
// cleared soon enough.
//
// WeakScope can be used on its own to track an instance of any type, as long as
// the instance remains valid *longer* that the WeakScope. The most common use
// cases, however, is for a class to either inherit from a WeakScope to itself,
// or to have a WeakScope to itself as a member (when inheritance is undesirable
// or not an option).
//
// Inheritance pattern:
// -----------------------------------------------------------------------------
// class Class final : public WeakScope<Class> {
// public:
//   Class() : WeakScope<Class>(this) {
//     // ...
//   }
//   ~Class() override {
//     // Invalidate the scope *first* while the class is still valid.
//     InvalidateWeakPtrs();
//     // Destruction can now complete safely...
//   }
// };
// -----------------------------------------------------------------------------
//
// Aggregation pattern:
// -----------------------------------------------------------------------------
// class Class final {
// public:
//  Class() : weak_scope_(this) {}
//  ~Class() override {
//     // Invalidate the scope *first* while the class is still valid.
//     weak_scope_.InvalidateWeakPtrs();
//     // Destruction can now complete safely...
//   }
//   // Publicly explose the GetWeakPtr() function.
//   template <typename Type>
//   WeakPtr<Type> GetWeakPtr() {
//     return weak_scope_.GetWeakPtr<Type>();
//   }
// private:
//   WeakScope<Class> weak_scope_;
// };
// -----------------------------------------------------------------------------
//
// Note: Classes that use these self-scoping patterns should generally be
// "final". Otherwise derived class will need to explicitly call
// InvalidateWeakPtrs() in their destructor as well. However if necessary, this
// can be done as InvalidateWeakPtrs() may be called multiple times.
//
// This class is thread-compatible. However, all WeakPtr, WeakLock, and
// WeakScope instances are thread-safe relative to each other. In other words,
// each independent instance of a WeakPtr, WeakLock, or WeakScope can be
// accessed or destructed safely from independent threads.
template <typename Type>
class WeakScope {
 public:
  // Constructs a WeakScope from the specified pointer.
  //
  // The pointer must remain valid (to a valid instance or nullptr) until
  // InvalidateWeakPtrs() is called on this instance.
  explicit WeakScope(Type* ptr)
      : data_(std::make_shared<internal::WeakPtrData>(ptr)), ptr_(ptr) {};

  // WeakScope can't be copied or moved, as its WeakPtrs point to the instance
  // it was constructed with.
  WeakScope(const WeakScope&) = delete;
  WeakScope& operator=(const WeakScope&) = delete;

  // Destructor. InvalidateWeakPtrs() must be called before destruction.
  virtual ~WeakScope();

  // Invalidates all WeakPtrs retrieved from this scope.
  //
  // As soon as this call begins execution, all new WeakLocks will be null. The
  // call then blocks until all previously existing WeakLocks are destructed.
  void InvalidateWeakPtrs();

  // Returns a WeakPtr to the instance managed by this scope.
  //
  // OtherType may be Type or a base class of it, but the base class must start
  // at the same address as the instance, which this CHECKs. With single
  // inheritance it always does. With multiple inheritance, which base class
  // starts there depends on the compiler (MSVC puts one with virtual functions,
  // such as WeakScope, ahead of the others).
  //
  // If InvalidateWeakPtrs was already called (or the scope was constructed with
  // nullptr), locks of the returned WeakPtr are null.
  template <typename OtherType = const Type,
            typename = std::enable_if_t<
                std::is_convertible<const Type*, OtherType*>::value>>
  WeakPtr<OtherType> GetWeakPtr() const {
    return DoGetWeakPtr<OtherType>();
  }
  template <typename OtherType = Type,
            typename =
                std::enable_if_t<std::is_convertible<Type*, OtherType*>::value>>
  WeakPtr<OtherType> GetWeakPtr() {
    return DoGetWeakPtr<OtherType>();
  }

 private:
  template <typename OtherType>
  WeakPtr<OtherType> DoGetWeakPtr() const;

  const std::shared_ptr<internal::WeakPtrData> data_;
  Type* ptr_;  // Null once invalidated.
};

//==============================================================================
// Implementation

inline void internal::WeakPtrData::Invalidate() {
  absl::MutexLock lock(&mutex_);
  ptr_ = nullptr;
  mutex_.Await(
      absl::Condition(+[](int* count) { return *count == 0; }, &count_));
}

inline void* internal::WeakPtrData::ReaderLock() {
  absl::MutexLock lock(&mutex_);
  if (ptr_ != nullptr) {
    ++count_;
  }
  return ptr_;
}

inline void internal::WeakPtrData::ReaderUnlock() {
  absl::MutexLock lock(&mutex_);
  --count_;
}

template <typename Type>
template <typename Scope>
inline WeakPtr<Type>::WeakPtr(Scope* scope) {
  if (scope != nullptr) {
    *this = scope->template GetWeakPtr<Type>();
  }
}

template <typename Type>
WeakLock<Type> WeakPtr<Type>::Lock() const {
  return WeakLock<Type>(this);
}

template <typename Type>
WeakLock<Type>::WeakLock(const WeakPtr<Type>* ptr) {
  if (ptr == nullptr || ptr->data_ == nullptr) {
    return;
  }
  void* const locked_ptr = ptr->data_->ReaderLock();
  if (locked_ptr != nullptr) {
    data_ = ptr->data_;
    ptr_ = static_cast<Type*>(locked_ptr);
  }
}

template <typename Type>
WeakLock<Type>::WeakLock(const WeakLock& other) {
  if (other.data_ != nullptr && other.data_->ReaderLock() != nullptr) {
    data_ = other.data_;
    ptr_ = other.ptr_;
  }
}

template <typename Type>
WeakLock<Type>::WeakLock(WeakLock&& other)
    : data_(std::exchange(other.data_, nullptr)),
      ptr_(std::exchange(other.ptr_, nullptr)) {}

template <typename Type>
WeakLock<Type>& WeakLock<Type>::operator=(const WeakLock& other) {
  if (&other != this) {
    *this = WeakLock(other);
  }
  return *this;
}

template <typename Type>
WeakLock<Type>& WeakLock<Type>::operator=(WeakLock&& other) {
  if (&other == this) {
    return *this;
  }
  if (data_ != nullptr) {
    data_->ReaderUnlock();
  }
  data_ = std::exchange(other.data_, nullptr);
  ptr_ = std::exchange(other.ptr_, nullptr);
  return *this;
}

template <typename Type>
WeakLock<Type>::~WeakLock() {
  if (data_ != nullptr) {
    data_->ReaderUnlock();
  }
}

template <typename Type>
template <typename OtherType>
WeakPtr<OtherType> WeakScope<Type>::DoGetWeakPtr() const {
  // WeakPtrData holds the instance as a void*, so a WeakPtr can only point to
  // a type at the same address.
  OtherType* const other_ptr = ptr_;
  CHECK(static_cast<const void*>(other_ptr) == static_cast<const void*>(ptr_))
      << "WeakPtr type must start at the same address as the WeakScope's "
         "instance";
  return WeakPtr<OtherType>(data_);
}

template <typename Type>
WeakScope<Type>::~WeakScope() {
  CHECK(ptr_ == nullptr)
      << "Invalidate() must be called prior to WeakScope destruction.";
}

template <typename Type>
void WeakScope<Type>::InvalidateWeakPtrs() {
  data_->Invalidate();
  ptr_ = nullptr;
}

}  // namespace gb

#endif  // GB_BASE_WEAK_PTR_H
