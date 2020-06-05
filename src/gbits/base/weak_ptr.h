#ifndef GBITS_BASE_WEAK_PTR_H
#define GBITS_BASE_WEAK_PTR_H

#include <memory>

#include "absl/synchronization/mutex.h"
#include "glog/logging.h"

namespace gb {

namespace internal {

// Internal structure used by Weak* classes.
class WeakPtrData {
 public:
  explicit WeakPtrData(void* ptr) : ptr_(ptr) {}

  void* Get() { return ptr_; }
  void Clear();
  void ReaderLock();
  void ReaderUnlock();

 private:
  absl::Mutex mutex_;
  std::atomic<bool> clear_pending_ = false;
  int count_ = 0;  // Number of WeakLocks active.
  void* ptr_ = nullptr;
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
// does not require shared ownership of the underlying instance. Instead, when
// the WeakPtr is tied to a WeakScope class which controls the value of the
// WeakPtr instance. To access the underlying pointer, it must be locked (via
// constructing a WeakLock<Type>), at which point the associated Scope is
// blocks if it attempts to invalidate the pointer (setting it to nullptr) until
// the lock is released. See WeakScope for examples.
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

  explicit WeakPtr(std::shared_ptr<internal::WeakPtrData> data) : data_(data) {}

  std::shared_ptr<internal::WeakPtrData> data_;
};

// This class locks a WeakPtr<Type> providing a stable pointer to the underlying
// instance.
//
// While any WeakLock instance exists, the underlying WeakScope will be
// block if it attempts to invaldate the pointer (setting it to null).
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
  // This prevents the underlying instance pointer from changing until the
  // WeakLock is destructed.
  explicit WeakLock(const WeakPtr<Type>* ptr);

  // Construct a WeakLock from nullptr.
  explicit WeakLock(nullptr_t) {}

  // Default construction.
  WeakLock() = default;

  // WeakLock supports all standard copy and assignment methods.
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
      : data_(std::make_shared<internal::WeakPtrData>(ptr)){};

  // WeakScope is a move-only class.
  WeakScope(const WeakScope&) = delete;
  WeakScope& operator=(const WeakScope&) = delete;
  WeakScope(WeakScope&&) = default;
  WeakScope& operator=(WeakScope&&) = default;

  // Destructor. InvalidateWeakPtrs() must be called before destruction.
  virtual ~WeakScope();

  // Invalidates all WeakPtrs retrieved from this scope (sets their pointer to
  // nullptr).
  //
  // As soon as this call begins execution, all WeakPtrs and new WeakLocks will
  // be null. The call then blocks until all previously existing WeakLocks
  // are destructed.
  void InvalidateWeakPtrs();

  // Returns a WeakPtr to the instance managed by this scope.
  //
  // If InvalidateWeakPtrs was already called (or the scope was constructed with
  // nullptr), this will return a null WeakPtr.
  template <typename OtherType,
            typename =
                std::enable_if_t<std::is_convertible<Type*, OtherType*>::value>>
  WeakPtr<OtherType> GetWeakPtr() {
    return WeakPtr<OtherType>(data_);
  }

 private:
  std::shared_ptr<internal::WeakPtrData> data_;
};

//==============================================================================
// Implementation

inline void internal::WeakPtrData::Clear() {
  clear_pending_ = true;
  absl::MutexLock lock(&mutex_);
  mutex_.Await(absl::Condition(
      +[](int* count) { return *count == 0; }, &count_));
  ptr_ = nullptr;
  clear_pending_ = false;
}

inline void internal::WeakPtrData::ReaderLock() {
  absl::MutexLock lock(&mutex_);
  mutex_.Await(absl::Condition(
      +[](std::atomic<bool>* clear_pending) { return !*clear_pending; },
      &clear_pending_));
  ++count_;
}

inline void internal::WeakPtrData::ReaderUnlock() {
  absl::MutexLock lock(&mutex_);
  --count_;
}

template <typename Type>
template <typename Scope>
inline WeakPtr<Type>::WeakPtr(Scope* scope) {
  if (scope != nullptr) {
    *this = scope->GetWeakPtr<Type>();
  }
}

template <typename Type>
WeakLock<Type> WeakPtr<Type>::Lock() const {
  return WeakLock<Type>(this);
}

template <typename Type>
WeakLock<Type>::WeakLock(const WeakPtr<Type>* ptr) {
  if (ptr != nullptr && ptr->data_ != nullptr) {
    data_ = ptr->data_;
    data_->ReaderLock();
    ptr_ = static_cast<Type*>(data_->Get());
  }
}

template <typename Type>
WeakLock<Type>::WeakLock(const WeakLock& other)
    : data_(other.data_), ptr_(other.ptr_) {
  if (data_ != nullptr) {
    data_->ReaderLock();
  }
}

template <typename Type>
WeakLock<Type>::WeakLock(WeakLock&& other)
    : data_(std::exhange(other.data_, nullptr)),
      ptr_(std::exhange(other.ptr_, nullptr)) {}

template <typename Type>
WeakLock<Type>& WeakLock<Type>::operator=(const WeakLock& other) {
  if (&other == this) {
    return *this;
  }
  if (data_ != nullptr) {
    data_->ReaderUnlock();
  }
  data_ = other.data_;
  ptr_ = other.ptr_;
  if (data_ != nullptr) {
    data_->ReaderLock();
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
WeakScope<Type>::~WeakScope() {
  CHECK_EQ(data_->Get(), nullptr)
      << "Invalidate() must be called prior to WeakScope destruction.";
}

template <typename Type>
void WeakScope<Type>::InvalidateWeakPtrs() {
  data_->Clear();
}

}  // namespace gb

#endif  // GBITS_BASE_WEAK_PTR_H
