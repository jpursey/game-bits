// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_JOB_TYPES_H_
#define GB_JOB_JOB_TYPES_H_

#include "gb/base/access_token.h"

namespace gb {

#ifndef GB_BUILD_ENABLE_JOB_LOGGING
#ifdef NDEBUG
#define GB_BUILD_ENABLE_JOB_LOGGING 0
#else  // NDEBUG
#define GB_BUILD_ENABLE_JOB_LOGGING 1
#endif  // NDEBUG
#endif  // GB_BUILD_ENABLE_JOB_LOGGING

class JobCounter;
class JobSystem;
class FiberJobSystem;

GB_DEFINE_ACCESS_TOKEN(JobInternal, class JobSystem, class FiberJobSystem);

}  // namespace gb

#endif  // GB_JOB_JOB_TYPES_H_
