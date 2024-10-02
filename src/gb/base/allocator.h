// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_ALLOCATOR_H_
#define GB_BASE_ALLOCATOR_H_

#include <memory>
#include <type_traits>

#include "absl/synchronization/mutex.h"

namespace gb {

// Interface to a generic allocator.
//
// A full set of concrete allocators are defined in the separate gb/alloc
// library. Thread-safety is dependent on the concrete implementation. Most game
// bits allocators come in both a thread-compatible and thread-safe variant,
// with the thread-safe variant having a "Ts" prefix.
class Allocator {
 public:
  virtual ~Allocator() = default;

  // Helper methods to new/delete classes using this allocator.
  template <typename Type, typename... Args>
  Type* New(Args... args) {
    return new (Alloc(sizeof(Type), alignof(Type)))
        Type(std::forward<Args>(args)...);
  }

  template <typename Type>
  void Delete(Type* object) {
    if (object != nullptr) {
      object->~Type();
      Free(object);
    }
  }

  // Helper methods to new/delete arrays using this allocator.
  template <typename Type, typename... Args>
  Type* NewArray(int count, Args... args) {
    Type* objects =
        static_cast<Type*>(Alloc(sizeof(Type) * count, alignof(Type)));
    for (int i = 0; i < count; ++i) {
      new (objects + i) Type(std::forward<Args>(args)...);
    }
    return objects;
  }
  template <typename Type>
  void DeleteArray(Type* objects, int count) {
    if (objects != nullptr) {
      for (int i = 0; i < count; ++i) {
        objects[i].~Type();
      }
      Free(objects);
    }
  }

  // Allocates memory from the allocator of the specified alignment.
  //
  // The specified alignment 'align' is the number of bytes the allocation must
  // be aligned to. This must be zero or a power of 2. If zero is specified, the
  // default heap alignment for the platform is implied (same as what malloc
  // would align to). On success, the returned pointer is guaranteed to meet the
  // requested alignment.
  //
  // On success, this will return a non-null pointer to the data. Allocating
  // zero bytes is undefined behavior.
  virtual void* Alloc(size_t size, size_t align) = 0;
  void* Alloc(size_t size) { return Alloc(size, 0); }

  // Frees a pointer allocated by "Alloc" from this allocator.
  //
  // Calling Free on a nullptr is valid, and is a noop. Calling Free on an
  // allocation from a different allocator is undefined behavior (and will
  // likely crash).
  virtual void Free(void* ptr) = 0;
};

// This class implements a thread-safe wrapper allocator for a thread-compatible
// allocator.
template <typename BaseAllocator>
class TsAllocator : public BaseAllocator {
 public:
  static_assert(std::is_base_of<Allocator, BaseAllocator>::value,
                "BaseAllocator must be an Allocator");

  template <typename... Args>
  explicit TsAllocator(Args... args)
      : BaseAllocator(std::forward<Args>(args)...) {}
  ~TsAllocator() override = default;

  void* Alloc(size_t size, size_t align) override {
    absl::MutexLock lock(&mutex_);
    return BaseAllocator::Alloc(size, align);
  }

  void Free(void* ptr) override {
    absl::MutexLock lock(&mutex_);
    BaseAllocator::Free(ptr);
  }

 private:
  absl::Mutex mutex_;
};

// Returns the global system allocator.
//
// This allocator allocates off the standard heap. The system allocator is
// thread-safe.
Allocator* GetSystemAllocator() noexcept;

// Sets the default allocator.
//
// This must be set before GetDefaultAllocator is called, otherwise this will
// have no effect. This function is thread-safe.
void SetDefaultAllocator(Allocator* allocator) noexcept;

// Gets the default allocator.
//
// If the default allocator was previously set by SetDefaultAllocator, this will
// return that allocator. If SetDefaultAllocator was not called before the first
// invocation of this function, this will return the standard system allocator
// (the same as GetSystemAllocator() returns).
//
// This function is thread-safe.
Allocator* GetDefaultAllocator() noexcept;

// Defines allocator traits for use with StdGlobalAllocator which uses the
// allocator set via SetDefaultAllocator.
struct DefaultGlobalAllocatorTraits {
  static Allocator* Get() noexcept { return GetDefaultAllocator(); }
};

// Defines allocator traits for use with StdGlobalAllocator which uses a default
// constructed (and never destructed) allocator of Type.
template <typename Type>
struct GlobalAllocatorTraits {
  static Allocator* Get() noexcept {
    alignas(Type) static char instance_memory[sizeof(Type)];
    static Type* instance = new (instance_memory) Type();
    return instance;
  }
};

// Defines a C++ compliant standard allocator that accesses a specified global
// allocator.
//
// By default, this will return an allocator implemented via the default
// allocator. However, it can be specialized to be any allocator by defining a
// traits struct with a single static method Get() which returns the desired
// global allocator. See StdDefaultAllocatorTraits for an example.
template <typename Type, typename Traits = DefaultGlobalAllocatorTraits>
class StdGlobalAllocator {
 public:
  using value_type = Type;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using is_always_equal = std::true_type;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::false_type;
  using propagate_on_container_swap = std::false_type;

  StdGlobalAllocator() noexcept = default;
  StdGlobalAllocator(const StdGlobalAllocator&) noexcept = default;
  StdGlobalAllocator(StdGlobalAllocator&&) noexcept = default;
  template <typename OtherType>
  StdGlobalAllocator(const StdGlobalAllocator<OtherType, Traits>&) noexcept {}
  template <typename OtherType>
  StdGlobalAllocator(StdGlobalAllocator<OtherType, Traits>&&) noexcept {}
  StdGlobalAllocator<Type, Traits>& operator=(
      const StdGlobalAllocator&) noexcept = default;
  StdGlobalAllocator<Type, Traits>& operator=(StdGlobalAllocator&&) noexcept =
      default;
  ~StdGlobalAllocator() noexcept = default;

  Type* allocate(size_type count) {
    return Traits::Get()->Alloc(count * sizeof(Type), alignof(Type));
  }
  void deallocate(Type* ptr, size_type /*count*/) {
    return Traits::Get()->Free(ptr);
  }

  bool operator==(const StdGlobalAllocator<Type, Traits>& other) {
    return true;
  }
  bool operator!=(const StdGlobalAllocator<Type, Traits>& other) {
    return false;
  }
};

// Defines a C++ compliant standard allocator that allocates from a provided
// allocator instance.
//
// By default, this will initialize to use the allocator returned from
// GetDefaultAllocator(). However, it may also be explicitly set to any
// allocator at runtime.
template <typename Type>
class StdAllocator {
 public:
  using value_type = Type;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using is_always_equal = std::false_type;
  using propagate_on_container_copy_assignment = std::false_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  StdAllocator() noexcept : allocator_(GetDefaultAllocator()) {}
  StdAllocator(Allocator* allocator) noexcept : allocator_(allocator) {}
  StdAllocator(const StdAllocator& other) noexcept
      : allocator_(other.allocator_) {}
  StdAllocator(StdAllocator&& other) noexcept : allocator_(other.allocator_) {}
  template <typename OtherType>
  StdAllocator(const StdAllocator<OtherType>& other) noexcept
      : allocator_(other.allocator_) {}
  template <typename OtherType>
  StdAllocator(StdAllocator<OtherType>&& other) noexcept
      : allocator_(other.GetAllocator()) {}
  StdAllocator<Type>& operator=(const StdAllocator&) noexcept = default;
  StdAllocator<Type>& operator=(StdAllocator&&) noexcept = default;
  ~StdAllocator() noexcept = default;

  Type* allocate(size_type count) {
    return allocator_->Alloc(count * sizeof(Type), alignof(Type));
  }
  void deallocate(Type* ptr, size_type /*count*/) {
    return allocator_->Free(ptr);
  }

  bool operator==(const StdAllocator<Type>& other) {
    return allocator_ == other.allocator_;
  }
  bool operator!=(const StdAllocator<Type>& other) {
    return allocator_ != other.allocator_;
  }

 private:
  Allocator* allocator_;
};

}  // namespace gb

#endif  // GB_BASE_ALLOCATOR_H_
