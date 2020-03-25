#ifndef GBITS_BASE_CALLBACK_H_
#define GBITS_BASE_CALLBACK_H_

#include <memory>

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

  // Construct from any callable type that supports move construction.
  template <typename Callable, typename = std::enable_if_t<!std::is_convertible<
                                   Callable, Return (*)(Args...)>::value>>
  Callback(Callable&& callable) {
    using CallableType = typename std::decay<Callable>::type;
    callback_ = new CallableType(std::move(callable));
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*static_cast<CallableType*>(callable))(
          std::forward<Args>(args)...);
    };
    delete_callback_ = [](void* callable) {
      delete static_cast<CallableType*>(callable);
    };
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
    callback_ = callable;
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*static_cast<CallableType*>(callable))(
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
    callback_ = callable;
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*static_cast<Return (*)(Args...)>(callable))(
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

  // Explicit construction from a function pointer.
  //
  // Note: This is here explicitly, as for some reason Visual Studio 2017 does
  // not find the conversion from a no-capture lambda to a Callback, even though
  // it will for explicit construction.
  Callback& operator=(Return (*callable)(Args...)) {
    return operator=(Callback(callable));
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

#endif  // GBITS_BASE_CALLBACK_H_
