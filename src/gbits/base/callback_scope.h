#ifndef GBITS_BASE_CALLBACK_SCOPE_H_
#define GBITS_BASE_CALLBACK_SCOPE_H_

#include <memory>

#include "absl/synchronization/mutex.h"
#include "gbits/base/callback.h"

namespace gb {

// A callback scope specifies a bounded lifetime for a callback to exist.
//
// All callbacks constructed through the CallbackScope will automatically become
// no-op callbacks when the CallbackScope is deleted. This allows safely
// generating callbacks to external whose lifetime is unknown or otherwise
// independent. A common use case for a CallbackScope is to safely wrap
// callbacks that bind to "this".
//
// This class is thread-compatible. However, callbacks generated by this scope
// can be called safely from any thread, as long as the callbacks themselves are
// thread-safe. If a scoped callback is executing, then CallbackScope will block
// in its destructor until the callback completes. As such, callbacks *must not*
// delete the underlying scope as a side-effect.
class CallbackScope final {
 public:
  CallbackScope() = default;
  CallbackScope(const CallbackScope&) = delete;
  CallbackScope& operator=(const CallbackScope&) = delete;

  // Callback destructor.
  //
  // This will block until there are no scoped callbacks executing.
  ~CallbackScope() {
    absl::WriterMutexLock lock(&alive_->mutex);
    alive_->alive = false;
  }

  // Creates a new void-return callback bound to this scope.
  //
  // When the scope is deleted, then the returned callback will do nothing.
  // Please see the class overview for thread safety details.
  template <typename Callable,
            typename = std::enable_if_t<std::is_same<
                void, typename Callback<Callable>::ReturnType>::value>>
  Callback<Callable> New(Callback<Callable> callback) {
    if (callback == nullptr) {
      return callback;
    }
    return Factory<Callable>::NewVoid(alive_, std::move(callback));
  }

  // Creates a new value-returning callback bound to this scope.
  //
  // When the scope is deleted, then the returned callback will return a default
  // value (which is the default-constructed return type, if not specified).
  // Please see the class overview for thread safety details.
  template <typename Callable,
            typename = std::enable_if_t<!std::is_same<
                void, typename Callback<Callable>::ReturnType>::value>>
  Callback<Callable> New(
      Callback<Callable> callback,
      typename Callback<Callable>::ReturnType default_value = {}) {
    if (callback == nullptr) {
      return callback;
    }
    return Factory<Callable>::NewDefault(
        alive_, std::move(callback), std::move(default_value));
  }

 private:
  struct Alive {
    absl::Mutex mutex;
    bool alive ABSL_GUARDED_BY(mutex) = true;
  };
  std::shared_ptr<Alive> alive_ = std::make_shared<Alive>();

  template <typename Callable>
  class Factory;

  template <typename Return, typename... Args>
  class Factory<Return(Args...)> {
   public:
    static Callback<void(Args...)> NewVoid(std::shared_ptr<Alive> alive,
                                           Callback<Return(Args...)> callback) {
      return [alive = std::move(alive),
              callback = std::move(callback)](Args&&... args) {
        absl::ReaderMutexLock lock(&alive->mutex);
        if (alive->alive) {
          callback(std::forward<Args>(args)...);
        }
      };
    }
    template <
        typename DefaultType,
        typename = std::enable_if_t<!std::is_same<void, DefaultType>::value>>
    static Callback<Return(Args...)> NewDefault(
        std::shared_ptr<Alive> alive, Callback<Return(Args...)> callback,
        DefaultType default_value) {
      return [alive = std::move(alive), callback = std::move(callback),
              default_value = std::move(default_value)](Args&&... args) {
        absl::ReaderMutexLock lock(&alive->mutex);
        if (alive->alive) {
          return callback(std::forward<Args>(args)...);
        }
        return default_value;
      };
    }
  };
};

}  // namespace gb

#endif  // GBITS_BASE_CALLBACK_SCOPE_H_