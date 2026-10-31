// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_FIBER_H_
#define GB_THREAD_FIBER_H_

#include <cstdint>
#include <string_view>
#include <vector>

#include "absl/types/span.h"
#include "gb/base/flags.h"
#include "gb/thread/thread_placement.h"
#include "gb/thread/thread_types.h"

namespace gb {

//==============================================================================
// This file defines a low level platform independent interface to user-space
// fibers for platforms that support it.
//==============================================================================

// Signature for the main function of a fiber.
using FiberMain = void (*)(void* user_data);

// This class represents an initial pairing of a fiber and the thread it began
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

// Options when creating fibers.
enum class FiberOption {
  // When a fiber with this option becomes active, it will set the thread's name
  // to its own name.
  kSetThreadName,
};
using FiberOptions = Flags<FiberOption>;

// Running state for a fiber.
//
// Note that querying fiber state is inherently stale information, and should
// only be used as a hint, without some other form of external synchronization.
enum class FiberState {
  // The fiber is not currently running. It must be switched to via
  // SwitchToFiber to continue running. It may be deleted via DeleteFiber,
  // although doing so will not execute any cleanup code on the fiber's stack.
  kSuspended,

  // The fiber is currently running on a thread. It cannot be switched to or
  // deleted.
  kRunning,

  // The fiber has exited. It cannot be switched to, but it is safe to delete.
  kExited,
};

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
// 'threads' says where each thread to create runs, usually from PlaceThreads()
// (see thread_placement.h). Each thread is pinned to its affinity, as for
// CreateThread, or isn't pinned if that is zero. A default PlacedThread isn't
// pinned, so std::vector<PlacedThread>(count) creates 'count' threads that
// aren't pinned.
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
// thread, in the order of 'threads'. If fibers are not supported or creation
// fails, this stops at the first thread that fails, so it returns the fibers
// for the threads before it (potentially none). Both the threads and fibers
// returned must be managed by the caller. DeleteFiber must be called to delete
// each fiber when it is not running in a thread, and either DetachThread or
// JoinThread must eventually be called on each thread.
//
// This function is thread-safe.
std::vector<FiberThread> CreateFiberThreads(
    absl::Span<const PlacedThread> threads, FiberOptions options,
    uint32_t stack_size, void* user_data, FiberMain fiber_main);

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
// not currently suspended (not running and not exited). Attempting to switch to
// an invalid and/or deleted fiber is undefined behavior (and will likely
// crash).
//
// This function is thread-safe (thread-compatible with DeleteFiber or itself
// on the same fiber).
bool SwitchToFiber(Fiber fiber);

// Callbacks that every SwitchToFiber() calls, so a library can carry what
// belongs to a fiber, such as the profiler's open timers, from the thread it
// suspends on to the thread it resumes on.
struct FiberSwitchCallbacks {
  // Called on the fiber that is switching away, on its thread, just before it
  // suspends. Returns what is passed to on_resume when this fiber resumes. A
  // fiber that is deleted while suspended never resumes, so what this returns
  // mustn't need on_resume to free it.
  void* (*on_suspend)() = nullptr;

  // Called on the fiber that suspended, as soon as it resumes, on the thread
  // it resumed on, with what on_suspend returned.
  void (*on_resume)(void* state) = nullptr;
};

// Sets the callbacks every SwitchToFiber() calls from now on, or none if
// `callbacks` is null. Both callbacks must be set (CHECK), and `callbacks` must
// outlive every fiber switch. A fiber resumes with the callbacks it suspended
// with, even if they changed while it was suspended.
//
// A fiber's first run starts in its main, not in a switch, so it begins with
// no on_resume. A fiber that exits calls neither, nor does a switch that fails.
//
// A program has one set: setting others while some are set CHECK-fails, so
// they must be cleared first. Setting the same ones again does nothing.
//
// This function is thread-safe.
void SetFiberSwitchCallbacks(const FiberSwitchCallbacks* callbacks);

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

// Returns the current fiber running state.
//
// This function itself is thread-safe (thread-compatible with DeleteFiber or
// SwitchToFiber on/from the same fiber). However, as it is inherently stale
// information, it should only be used as a hint without additional external
// synchronization guarantees provided by the caller.
FiberState GetFiberState(Fiber fiber);

// Returns the number of fibers currently executing in a thread.
//
// This function is thread-safe.
int GetRunningFiberCount();

}  // namespace gb

#endif  // GB_THREAD_FIBER_H_
