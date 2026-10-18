// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_THREAD_H_
#define GB_THREAD_THREAD_H_

#include <cstdint>
#include <string_view>

#include "absl/types/span.h"
#include "gb/thread/thread_types.h"

namespace gb {

//==============================================================================
// This file defines a low level platform independent interface to threads,
// beyond what is available from the standard library. It provides functionality
// for specifying hardware thread affinity, specifying thread names, and
// measuring how long a thread has run for.
//==============================================================================

// Signature for the main function of a thread.
using ThreadMain = void (*)(void* user_data);

// Sets whether the thread module has verbose logging.
//
// This only has an effect if GB_BUILD_ENABLE_THREAD_LOGGING is defined to be
// non-zero (by default, this is true in debug builds). Otherwise, verbose
// logging cannot be enabled.
void SetThreadVerboseLogging(bool enabled);

// Returns the maximum number of threads that can be running simultaneously in
// this process on different hardware threads.
//
// Depending on the operating system and environment, this may be less than the
// total number of hardware threads on the machine, if the running process does
// not have access to them.
//
// If the maximum concurrency cannot be determined for the current platform,
// this returns 1.
//
// This function is thread-safe.
int GetMaxConcurrency();

// Returns the hardware individual thread affinities available for threads.
//
// This may contain less affinities than GetMaxConcurrency would indicate. If
// hardware thread pinning is not supported by the current platform, this will
// be empty. Valid affinities are non-zero. No other assumption should be made
// on affinity values. Affinities may or may not be a mask (depending on the
// platform and number of hardware threads available).
//
// This function is thread-safe.
absl::Span<const uint64_t> GetHardwareThreadAffinities();

// Creates a new thread and starts it running.
//
// 'affinity' determines the requested hardware affinity for the thread. If the
// affinity is zero, then no hardware affinity is defined. Otherwise, the thread
// will be pinned to that hardware thread affinity, if possible (if not, the
// thread may be unpinned -- call GetThreadAffinity to query this).
//
// 'stack_size' determines the minimum size of the stack for the thread. If
// this is zero (or if setting the stack size is not supported by the running
// platform), then stack size will be the default stack size for threads.
//
// 'thread_main' is the function executed for each thread, and it is passed the
// provided 'user_data'. When 'thread_main' exits, the corresponding thread will
// exit.
//
// Every call to CreateThread should be paired with either a JoinThread or
// DetachThread to ensure thread resources are reclaimed.
//
// This function is thread-safe.
Thread CreateThread(uint64_t affinity, uint32_t stack_size, void* user_data,
                    ThreadMain thread_main);

// Joins a potentially running thread to the current thread, ending the thread
// and cleaning up any thread resources.
//
// This blocks the current thread until the specified thread completes. If the
// thread handle is already completed, this returns immediately. After
// JoinThread returns, the thread handle is cleaned up and invalid.
//
// This function is thread-compatible with other functions that operate on the
// same thread, and thread-safe otherwise.
void JoinThread(Thread thread);

// Detaches a potentially running thread, ensuring the thread resources are
// cleaned up once the thread is no longer running.
//
// After DetachThread returns, the thread handle should generally not be used
// except within its own thread, as it will automatically be deleted when the
// thread exits.
//
// This function is thread-compatible with other functions that operate on the
// same thread, and thread-safe otherwise.
void DetachThread(Thread thread);

// Returns the affinity for the thread, from CreateThread() or a later
// SetThisThreadAffinity() on the thread.
//
// If the thread does not have an assigned hardware thread affinity this will
// return zero.
//
// This function is thread-compatible with JoinThread and DetachThread on the
// same thread, and thread-safe otherwise.
uint64_t GetThreadAffinity(Thread thread);

// Pins the calling thread to `affinity`, which is one from
// GetHardwareThreadAffinities() or GetCpuTopology() (see cpu_topology.h).
//
// This works on any thread, including the program's main thread, which
// CreateThread() did not create. It returns false, leaving the thread as it
// was, if `affinity` is zero or the thread cannot be pinned to it, which is
// always the case if the current platform cannot pin threads.
//
// This function is thread-safe.
bool SetThisThreadAffinity(uint64_t affinity);

// Gets or sets the name of a thread.
//
// The maximum name length is platform specific. Any excess bytes will be
// trimmed.
//
// It is valid to call these on a null thread. If this is done, Get will return
// "null" and Set will be a noop.
//
// These functions are thread-compatible with other functions that operate on
// the same thread, and thread-safe otherwise.
std::string_view GetThreadName(Thread thread);
void SetThreadName(Thread thread, std::string_view name);

// Returns the thread currently running, or null if this thread was not created
// by CreateThread. On a fiber, this is the thread running the fiber now, which
// may change whenever the fiber switches away and back.
//
// This function is thread-safe.
Thread GetThisThread();

// Returns the optional thread features the current platform supports.
//
// This function is thread-safe.
ThreadCapabilities GetThreadCapabilities();

// Returns how many cycles the calling thread has run for.
//
// This always returns zero unless GetThreadCapabilities() includes
// ThreadCapability::kCycles.
//
// This counts only the time the thread spent running, not the time it waited
// or was switched out, and includes everything run on the thread (not just
// code from Game Bits). The difference between two calls on the same thread is
// how much work the thread did in between.
//
// On modern processors, cycles are usually in the same units as the
// processor's timestamp counter, so a difference converts to time with
// GetCpuTicksPerSecond() (see cpu_ticks.h). This may not be true on older
// processors or in virtual machines.
//
// Any thread may call this, including threads not created by CreateThread.
//
// This function is thread-safe.
int64_t GetThisThreadCycles();

// Returns the class of the core the calling thread is running on, where a
// higher class is faster. Classes count up from 0 for the slowest kind of core,
// so on a CPU with one kind of core (or a platform that cannot tell them apart)
// every core is class 0 (see CpuCore in cpu_topology.h).
//
// The thread may move to another core as soon as this returns. The first call
// finds every core's class, and later calls are fast.
//
// This function is thread-safe.
int GetThisThreadCoreClass();

// Returns the number of threads still active.
//
// A thread is considered no longer active if JoinThread or DetachThread have
// been called on it AND it is no longer running. This only returns the count of
// threads created via CreateThread.
//
// This function is thread-safe.
int GetActiveThreadCount();

}  // namespace gb

#endif  // GB_THREAD_THREAD_H_
