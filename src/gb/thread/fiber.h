// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_FIBER_H_
#define GB_THREAD_FIBER_H_

#include <string_view>
#include <vector>

#include "gb/base/flags.h"
#include "gb/thread/thread_types.h"

namespace gb {

//==============================================================================
// This file defines a low level platform independent interface to user-space
// fibers for platforms that support it.
//==============================================================================

// Signature for the main function of a fiber.
using FiberMain = void (*)(void* user_data);

// This class represents an initial pairing of a fiber and the thread it begain
// running on.
//
// This is returned by CreateFiberThreads.
struct FiberThread {
  FiberThread() {}
  FiberThread(Fiber in_fiber, Thread in_thread)
      : fiber(in_fiber), thread(in_thread) {}

  Fiber fiber = nullptr;
  Thread thread = nullptr;
};

// Options when creating fiber threads.
enum class FiberOption {
  // Fibers created as part of a thread should be pinned to a core. This has no
  // effect when creating a suspended fiber (CreateFiber).
  kPinThreads,

  // When a fiber with this option becomes active, it will set the thread's name
  // to its own name.
  kSetThreadName,
};
using FiberOptions = Flags<FiberOption>;

// Returns true if the running platform supports fibers.
//
// This function is thread-safe.
bool SupportsFibers();

// Sets whether the fiber module has verbose logging.
//
// This only has an effect if GB_BUILD_ENABLE_THREAD_LOGGING is defined to be
// non-zero (by default, this is true in debug builds). Otherwise, verbose
// logging cannot be enabled.
void SetFiberVerboseLogging(bool enabled);

// Creates a set of fibers, each running on a separate thread.
//
// 'thread_count' determines the number of threads to create as follows (where
// max_concurrency is the number of available hardware threads for the running
// process):
//   - Positive value: Creates thread_count threads/fibers
//   - Zero: Creates max_concurrency threads/fibers.
//   - Negative: Creates max(max_concurrency - thread_count, 1) threads/fibers.
// If 'FiberOption::kPinThreads' is set AND the number of threads created is
// less than or equal to max_concurrency, then each thread will be bound to a
// specific core.
//
// 'stack_size' determines the minimum size of the stack for every thread. If
// this is zero (or if setting the stack size is not supported by the running
// platform), then stack size will be the default stack size for threads.
//
// 'fiber_main' is the function executed for each fiber, and it is passed the
// provided 'user_data'. When 'fiber_main' exits, the corresponding thread will
// exit. This does not delete the fiber, and it must still be deleted with
// DeleteFiber.
//
// Returns the successfully created set of fibers, each running on their own
// thread. If fibers are not supported or creation fails this may be less that
// the requested number of fibers (potentially empty). Both the threads and
// fibers returned must be managed by the caller. DeleteFiber must be called to
// delete each fiber when it is not running in a thread, and either DetachThread
// or JoinThread must eventually be called on each thread.
//
// This function is thread-safe.
std::vector<FiberThread> CreateFiberThreads(int thread_count,
                                            FiberOptions options,
                                            uint32_t stack_size,
                                            void* user_data,
                                            FiberMain fiber_main);

// Creates a suspended fiber.
//
// 'stack_size' determines the minimum size of the stack for the fiber. If
// this is zero (or if setting the stack size is not supported by the running
// platform), then stack size will be the default stack size for threads.
//
// 'fiber_main' is the function executed for each fiber, and it is passed the
// provided 'user_data'. When 'fiber_main' exits, the corresponding thread will
// exit. This does not delete the fiber, and it must still be deleted with
// DeleteFiber.
//
// Returns the successfully created fiber, or null if fibers are not supported
// or creation fails. Call SwitchToFiber to start the fiber running. Call
// DeleteFiber to delete the fiber when it is not running.
//
// This function is thread-safe.
Fiber CreateFiber(FiberOptions options, uint32_t stack_size, void* user_data,
                  FiberMain fiber_main);

// Deletes the specified fiber.
//
// Fibers that are running must not be deleted. Attempting to delete an invalid
// fiber or one that is running is undefined behavior (and will likely crash).
//
// This function is thread-safe (thread-compatible with other functions on the
// same fiber).
void DeleteFiber(Fiber fiber);

// Switches the currently running fiber to the specified fiber.
//
// This returns true if the fiber was switched to. SwitchToFiber will fail if
// the calling thread is not a fiber, or if the fiber being switched to is
// already running. Attempting to switch to an invalid and/or deleted fiber is
// undefined behavior (and will likely crash).
//
// This function is thread-safe (thread-compatible with DeleteFiber or itself
// on the same fiber).
bool SwitchToFiber(Fiber fiber);

// Gets or sets the name of a fiber.
//
// The maximum name length is 128 bytes (including the null terminator). Any
// excess bytes will be trimmed.
//
// It is valid to call these on a null fiber. If this is done, Get will return
// "null" and Set will be a noop.
//
// These functions are thread-safe (thread-compatible with DeleteFiber or
// Get/SetFiberName on the same thread).
std::string_view GetFiberName(Fiber fiber);
void SetFiberName(Fiber fiber, std::string_view name);

// Gets or sets arbitrary data on a fiber.
//
// This does not take ownership or do anything other than store the pointer. It
// is the callers responsibility to ensure any pointed-to data is correctly
// cleaned up.
//
// It is valid to call these on a null fiber. If this is done, Get will return
// null and Set will be a noop.
//
// These functions are thread-safe (thread-compatible with DeleteFiber).
void* GetFiberData(Fiber fiber);
void SetFiberData(Fiber fiber, void* data);

// Atomically sets the fiber data and returns the previously set data.
//
// This does not take ownership or do anything other than store the pointer. It
// is the callers responsibility to ensure any pointed-to data is correctly
// cleaned up.
//
// Like Get and Set, it is valid to call on a null fiber. If this is done it
// will do nothing and return null.
//
// This function is thread-safe (thread-compatible with DeleteFiber).
void* SwapFiberData(Fiber fiber, void* data);

// Returns the fiber currently running, or null if this thread is not a running
// a fiber.
//
// This function is thread-safe.
Fiber GetThisFiber();

// Returns true if the fiber is currently running.
//
// This function itself is thread-safe (thread-compatible with DeleteFiber or
// SwitchToFiber on/from the same fiber). However, as it is inherently stale
// information, it should only be used as a hint without additional external
// synchronization guarantees provided by the caller.
bool IsFiberRunning(Fiber fiber);

// Returns the number of fibers currently executing in a thread.
//
// This function is thread-safe.
int GetRunningFiberCount();

}  // namespace gb

#endif  // GB_THREAD_FIBER_H_
