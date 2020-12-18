// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_JOB_FIBER_H_
#define GB_JOB_JOB_FIBER_H_

#include <string_view>
#include <vector>

#include "gb/job/job_types.h"

namespace gb {

//==============================================================================
// This file defines a low level platform independent interface to user-space
// fibers for platforms that support it. In most cases, it is easier to use the
// higher level FiberJobSystem interface (which is implemented in terms of this
// API).
//==============================================================================

// A JobFiber is a handle to a platform-specific fiber. Null indicates an
// invalid/non-existant fiber.
struct JobFiberType;
using JobFiber = JobFiberType*;

// Signature for the main function of a fiber.
using JobFiberMain = void (*)(void* user_data);

// Returns true if the running platform supports fibers.
//
// This function is thread-safe.
bool SupportsJobFibers();

// Sets whether the job fiber module has verbose logging.
//
// This only has an effect if GB_BUILD_ENABLE_JOB_LOGGING is defined to be
// non-zero (by default, this is true in debug builds). Otherwise, verbose
// logging cannot be enabled.
void SetJobFiberVerboseLogging(bool enabled);

// Returns the maximum number of threads that can be running simultaneously.
//
// This function is thread-safe.
int GetMaxConcurrency();

// Creates a set of fibers, each running on a separate thread.
//
// 'thread_count' determines the number of threads to create as follows (where
// max_concurrency is the number of available hardware threads for the running
// process):
// - Positive value: Creates thread_count threads/fibers
// - Zero: Creates max_concurrency threads/fibers.
// - Negative: Creates max(max_concurrency - thread_count, 1) threads/fibers.
// If 'pin_threads' is true AND the number of threads created is less than or
// equal to max_concurrency, then each thread will be bound to a specific core.
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
// the requested number of fibers (potentially empty). DeleteJobFiber must be
// called to delete each fiber when it is not running.
//
// This function is thread-safe.
std::vector<JobFiber> CreateJobFiberThreads(int thread_count, bool pin_threads,
                                            size_t stack_size, void* user_data,
                                            JobFiberMain fiber_main);

// Creates a suspended fiber.
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
// Returns the successfully created fiber, or null if fibers are not supported
// or creation fails. Call SwitchToJobFiber to start the fiber running. Call
// DeleteJobFiber to delete the fiber when it is not running.
//
// This function is thread-safe.
JobFiber CreateJobFiber(size_t stack_size, void* user_data,
                        JobFiberMain fiber_main);

// Deletes the specified fiber.
//
// Fibers that are running must not be deleted. Attempting to delete an invalid
// fiber or one that is running is undefined behavior (and will likely crash).
//
// This function is thread-safe (thread-compatible with other functions on the
// same fiber).
void DeleteJobFiber(JobFiber fiber);

// Switches the currently running fiber to the specified fiber.
//
// This returns true if the fiber was switched to. SwitchToJobFiber will fail if
// the calling thread is not a fiber, or if the fiber being switched to is
// already running. Attempting to switch to an invalid and/or deleted fiber is
// undefined behavior (and will likely crash).
//
// This function is thread-safe (thread-compatible with DeleteJobFiber or itself
// on the same fiber).
bool SwitchToJobFiber(JobFiber fiber);

// Returns the fiber currently running, or null if this thread is not a running
// a fiber.
//
// This function is thread-safe.
JobFiber GetThisJobFiber();

// Returns true if the fiber is currently running.
//
// This function itself is thread-safe (thread-compatible with DeleteJobFiber or
// SwitchToJobFiber on/from the same fiber). However, as it is inherently stale
// information, it should only be used as a hint without additional external
// synchronization guarantees provided by the caller.
bool IsJobFiberRunning(JobFiber fiber);

// Returns the number of fibers currently executing in a thread.
//
// This function is thread-safe.
int GetRunningJobFiberCount();

// Gets or sets the name of a job fiber.
//
// The maximum name length is 128 bytes (including the null terminator). Any
// excess bytes will be trimmed.
//
// It is valid to call these on a null fiber. If this is done, Get will return
// "null" and Set will be a noop.
//
// These function is thread-safe (thread-compatible with DeleteJobFiber or
// Get/SetJobFiberName on the same thread).
std::string_view GetJobFiberName(JobFiber fiber);
void SetJobFiberName(JobFiber fiber, std::string_view name);

}  // namespace gb

#endif  // GB_JOB_JOB_FIBER_H_
