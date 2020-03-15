#ifndef GBITS_BASE_CALLBACK_H_
#define GBITS_BASE_CALLBACK_H_

#include <functional>
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
  template <typename Callable, typename = std::enable_if_t<!std::is_same<
                                   std::decay<Callable>, Callback>::value>>
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
    callback_ = callable.release();
    call_callback_ = [](void* callable, Args&&... args) -> Return {
      return (*static_cast<CallableType*>(callable))(
          std::forward<Args>(args)...);
    };
    delete_callback_ = [](void* callable) {
      Deleter()(static_cast<CallableType*>(callable));
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
  Return operator()(Args&&... args) const {
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