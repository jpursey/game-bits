// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_FUNCTION_HOOK_H_
#define GB_BASE_FUNCTION_HOOK_H_

#include <optional>
#include <type_traits>
#include <utility>

#include "absl/log/check.h"

namespace gb {

namespace internal {
template <auto kPointer, typename Hook, typename Function>
class FunctionHookImpl;
}  // namespace internal

//==============================================================================
// FunctionHook
//==============================================================================

// Hooks a global function pointer, as used by C APIs whose functions are loaded
// at runtime (such as a host application's API table, or Vulkan's).
//
// While a FunctionHook exists, the pointer at `kPointer` points to a function
// of the same signature, which calls `hook().Call(original, args...)` and
// returns its result. `original` is the pointer's value when the hook was
// installed. The hook may call it, or stand in for it entirely.
//
// A null pointer (a function that was never loaded) is left null by default, so
// code that checks whether the function exists still finds it missing, and
// every function in an API table can be hooked safely. A Hook that stands in
// for missing functions (such as a fake of the API) opts in to hooking null
// pointers by declaring:
//
//   static constexpr bool kHookNotLoaded = true;
//
// Its Call is then passed a null `original` for a function that wasn't loaded.
//
// Hook must have a Call member function (static or not) that accepts the
// original function pointer followed by the function's arguments. It is
// usually a template, so one Hook type can hook functions of any signature:
//
//   class LogHook {
//    public:
//     explicit LogHook(std::string_view name) : name_(name) {}
//
//     template <typename Function, typename... Args>
//     auto Call(Function original, Args&&... args) {
//       LOG(INFO) << name_;
//       return original(std::forward<Args>(args)...);
//     }
//
//    private:
//     std::string_view name_;
//   };
//
//   FunctionHook<&vkCreateBuffer, LogHook> hook("vkCreateBuffer");
//
// Constructing a FunctionHook installs it, forwarding its arguments to Hook's
// constructor (even when the pointer is left null), and destroying it restores
// the pointer to `original`. Neither is synchronized with calls through the
// pointer, so hooks must only be installed and uninstalled while no call
// through the pointer is in progress.
//
// Hooks stack: a hook installed over another calls it as its original. They
// must be destroyed in the reverse order they were installed, and only one
// FunctionHook with the same pointer and Hook type can exist at a time.
//
// Hooks are not supported for variadic functions (such as `printf`).
template <auto kPointer, typename Hook>
using FunctionHook =
    internal::FunctionHookImpl<kPointer, Hook,
                               std::remove_pointer_t<decltype(kPointer)>>;

//==============================================================================
// Implementation
//==============================================================================

namespace internal {

template <auto kPointer, typename Hook, typename Return, typename... Args>
class FunctionHookImpl<kPointer, Hook, Return (*)(Args...)> final {
 public:
  using Function = Return (*)(Args...);

  template <typename... HookArgs>
  explicit FunctionHookImpl(HookArgs&&... hook_args) {
    CHECK(!s_installed.has_value())
        << "Only one FunctionHook with the same pointer and Hook may exist";
    s_installed.emplace(*kPointer, std::forward<HookArgs>(hook_args)...);
    *kPointer = InstalledPointer();
  }
  FunctionHookImpl(const FunctionHookImpl&) = delete;
  FunctionHookImpl& operator=(const FunctionHookImpl&) = delete;
  ~FunctionHookImpl() {
    CHECK(*kPointer == InstalledPointer())
        << "FunctionHooks must be destroyed in the reverse order they were "
           "installed";
    *kPointer = s_installed->original;
    s_installed.reset();
  }

  // Returns the hook that calls through the pointer are passed to.
  Hook& hook() { return s_installed->hook; }

 private:
  // The installed state is static, rather than in the FunctionHook, so the
  // wrapper reaches it at a fixed address instead of through a pointer.
  struct Installed {
    template <typename... HookArgs>
    explicit Installed(Function function, HookArgs&&... hook_args)
        : original(function), hook(std::forward<HookArgs>(hook_args)...) {}

    const Function original;
    Hook hook;
  };

  // True if Hook declares a `kHookNotLoaded` that is true.
  static constexpr bool kHookNotLoaded =
      requires { requires Hook::kHookNotLoaded; };

  // Returns the value the pointer holds while this hook is installed.
  static Function InstalledPointer() {
    if (s_installed->original == nullptr && !kHookNotLoaded) {
      return nullptr;
    }
    return &Wrapper;
  }

  static Return Wrapper(Args... args) {
    return s_installed->hook.Call(s_installed->original,
                                  std::forward<Args>(args)...);
  }

  static inline std::optional<Installed> s_installed;
};

}  // namespace internal
}  // namespace gb

#endif  // GB_BASE_FUNCTION_HOOK_H_
