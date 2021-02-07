// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_CALLBACK_H_
#define GB_BASE_CALLBACK_H_

#include <memory>

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++20-extensions"
#endif

namespace gb {

// Non-specialized template declaration for Callback. Only the following
// specialization is supported. See below.
template <typename>
class Callback;

// Defines a callback to any callable type.
//
// This class serves a similar purpose to std::function, except that it does not
// require anything of the underlying callable type. This allows it to capture a
// wider range of callbacks, including lambdas that are not copyable (for
// instance, if a std::unique_ptr is in the capture list of a lambda). Its only
// restriction is that it is itself not copyable.
//
// Whenever possible, Callback is preferred over std::function as it can receive
// anything that can be called. It also supports both owned and unowned
// references to callables, and has minimal overhead for plain function
// pointers (one extra call).
//
// This class is thread-compatible.
template <typename Return, typename... Args>
class Callback<Return(Args...)> final {
 public:
  using ReturnType = Return;

  // Constructs a null callback.
  Callback() = default;
  Callback(std::nullptr_t) {}

  // Copy construction is not supported.
  Callback(const Callback&) = delete;

  // Move constructor.
  Callback(Callback&& other)
      : callback_(std::exchange(other.callback_, nullptr)),
        call_callback_(std::exchange(other.call_callback_, nullptr)),
        delete_callback_(std::exchange(other.delete_callback_, nullptr)) {}

  // Construct from any callable type that supports copy or move construction,
  // or implicit cast to a function pointer.
  template <typename Callable>
  Callback(Callable&& callable) {
    using CallableType = typename std::decay<Callable>::type;
    static_assert(std::is_invocable<CallableType, Args...>::value,
                  "One or more arguments do not match Callback");
    static_assert(!std::is_invocable<CallableType, Args...>::value ||
                      std::is_invocable_r<Return, CallableType, Args...>::value,
                  "Return type does not match callback");
    using TagType = typename std::conditional<
        std::is_convertible<Callable, Return (*)(Args...)>::value,
        FunctionPtrTag,
        typename std::conditional<
            std::is_lvalue_reference<Callable>::value, LValueTag,
            typename std::conditional<std::is_const<Callable>::value, ConstTag,
                                      RValueTag>::type>::type>::type;
    Init(std::forward<Callable>(callable), TagType{});
  }

  // Construct from a std::unique_ptr to callable type. This is used for types
  // that do not support move construction, but ownership should be passed.
  template <typename CallableType, typename Deleter>
  Callback(std::unique_ptr<CallableType, Deleter> callable) {
    // Supporting custom deleters generically would require additional overhead
    // in Callback to store the deleter value. This is a rare use case for
    // callbacks, so the current decision is to not support custom deleters.
    static_assert(
        std::is_same<Deleter, std::default_delete<CallableType>>::value,
        "Custom deleters are not supported in Callback.");
    static_assert(std::is_invocable<CallableType, Args...>::value,
                  "One or more arguments do not match Callback");
    static_assert(!std::is_invocable<CallableType, Args...>::value ||
                      std::is_invocable_r<Return, CallableType, Args...>::value,
                  "Return type does not match callback");
    callback_ = callable.release();
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*static_cast<CallableType*>(callable))(
          std::forward<Args>(args)...);
    };
    delete_callback_ = [](void* callable) {
      delete static_cast<CallableType*>(callable);
    };
  }

  // Construct from a pointer to a callable type. This is used for simple
  // function or functor pointers. No ownership is passed, so it is the
  // responsibility of the caller to ensure the pointer remains valid for the
  // duration of time this callback is referring to it.
  template <typename CallableType>
  Callback(CallableType* callable) {
    static_assert(std::is_invocable<CallableType, Args...>::value,
                  "One or more arguments do not match Callback");
    static_assert(!std::is_invocable<CallableType, Args...>::value ||
                      std::is_invocable_r<Return, CallableType, Args...>::value,
                  "Return type does not match callback");
    callback_ = callable;
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*reinterpret_cast<CallableType*>(callable))(
          std::forward<Args>(args)...);
    };
    delete_callback_ = nullptr;
  }

  // Construct from a function pointer.
  //
  // Note: This is here explicitly to allow lambdas with no capture to match
  // this overload instead of the generic one which will result in a
  // needless allocation. Non-template overloads are always chosen over template
  // overloads for resolution purposes.
  Callback(Return (*callable)(Args...)) {
    callback_ = reinterpret_cast<void*>(callable);
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*reinterpret_cast<Return (*)(Args...)>(callable))(
          std::forward<Args>(args)...);
    };
    delete_callback_ = nullptr;
  }

  // Copy assigment is not supported.
  Callback& operator=(const Callback&) = delete;

  // Move assignment.
  Callback& operator=(Callback&& other) {
    if (&other == this) {
      return *this;
    }
    if (delete_callback_ != nullptr) {
      delete_callback_(callback_);
    }
    callback_ = std::exchange(other.callback_, nullptr);
    call_callback_ = std::exchange(other.call_callback_, nullptr);
    delete_callback_ = std::exchange(other.delete_callback_, nullptr);
    return *this;
  }

  // Sets the callback to null.
  Callback& operator=(std::nullptr_t) {
    if (delete_callback_ != nullptr) {
      delete_callback_(callback_);
    }
    callback_ = nullptr;
    call_callback_ = nullptr;
    delete_callback_ = nullptr;
    return *this;
  }

  // Destructs the callback.
  ~Callback() {
    if (delete_callback_ != nullptr) {
      delete_callback_(callback_);
    }
  }

  // Returns true if the callback is callable.
  operator bool() const { return callback_ != nullptr; }

  // Calls the underlying callback.
  Return operator()(Args... args) const {
    return call_callback_(callback_, std::forward<Args>(args)...);
  }

 private:
  using CallCallback = Return (*)(void*, Args&&...);
  using DeleteCallback = void (*)(void*);
  struct FunctionPtrTag {};
  struct RValueTag {};
  struct LValueTag {};
  struct ConstTag {};

  template <typename Callable>
  void Init(Callable&& callable, FunctionPtrTag) {
    using FunctionPtr = Return (*)(Args...);
    callback_ = reinterpret_cast<void*>(static_cast<FunctionPtr>(callable));
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*reinterpret_cast<FunctionPtr>(callable))(
          std::forward<Args>(args)...);
    };
    delete_callback_ = nullptr;
  }
  template <typename Callable>
  void Init(Callable&& callable, RValueTag) {
    using CallableType = typename std::decay<Callable>::type;
    callback_ = new CallableType(std::move(callable));
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*reinterpret_cast<CallableType*>(callable))(
          std::forward<Args>(args)...);
    };
    delete_callback_ = [](void* callable) {
      delete static_cast<CallableType*>(callable);
    };
  }
  template <typename Callable>
  void Init(Callable&& callable, LValueTag) {
    using CallableType = typename std::decay<Callable>::type;
    callback_ = new CallableType(callable);
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*reinterpret_cast<CallableType*>(callable))(
          std::forward<Args>(args)...);
    };
    delete_callback_ = [](void* callable) {
      delete static_cast<CallableType*>(callable);
    };
  }
  template <typename Callable>
  void Init(Callable&& callable, ConstTag) {
    static_assert(
        false, "Passed in callback is a const reference that cannot be copied");
  }

  void* callback_ = nullptr;
  CallCallback call_callback_ = nullptr;
  DeleteCallback delete_callback_ = nullptr;
};

template <typename Callable>
inline bool operator==(const Callback<Callable>& callback, std::nullptr_t) {
  return !callback;
}

template <typename Callable>
inline bool operator==(std::nullptr_t, const Callback<Callable>& callback) {
  return !callback;
}

template <typename Callable>
inline bool operator!=(const Callback<Callable>& callback, std::nullptr_t) {
  return callback;
}

template <typename Callable>
inline bool operator!=(std::nullptr_t, const Callback<Callable>& callback) {
  return callback;
}

}  // namespace gb

#ifdef __clang__
#pragma GCC diagnostic pop
#endif

#endif  // GB_BASE_CALLBACK_H_
