// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_TYPES_H_
#define GB_THREAD_TYPES_H_

namespace gb {

#ifndef GB_BUILD_ENABLE_FIBER_LOGGING
#ifdef NDEBUG
#define GB_BUILD_ENABLE_FIBER_LOGGING 0
#else  // NDEBUG
#define GB_BUILD_ENABLE_FIBER_LOGGING 1
#endif  // NDEBUG
#endif  // GB_BUILD_ENABLE_FIBER_LOGGING

}  // namespace gb

#endif  // GB_THREAD_TYPES_H_
