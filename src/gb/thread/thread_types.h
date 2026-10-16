// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_THREAD_TYPES_H_
#define GB_THREAD_THREAD_TYPES_H_

#include "gb/base/flags.h"

namespace gb {

#ifndef GB_BUILD_ENABLE_THREAD_LOGGING
#ifdef NDEBUG
#define GB_BUILD_ENABLE_THREAD_LOGGING 0
#else  // NDEBUG
#define GB_BUILD_ENABLE_THREAD_LOGGING 1
#endif  // NDEBUG
#endif  // GB_BUILD_ENABLE_THREAD_LOGGING

// A Fiber is a handle to a platform-specific fiber. Null indicates an
// invalid/non-existant fiber.
struct FiberType;
using Fiber = FiberType*;

// A Thread is a handle to a platform-specific thread. Null indicates an
// invalid/non-existant thread.
struct ThreadType;
using Thread = ThreadType*;

// Defines which optional thread features the current platform supports.
enum class ThreadCapability {
  kCycles,  // GetThisThreadCycles() counts the cycles a thread has run for.
};
using ThreadCapabilities = Flags<ThreadCapability>;

}  // namespace gb

#endif  // GB_THREAD_THREAD_TYPES_H_
