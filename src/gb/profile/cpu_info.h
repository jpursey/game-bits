// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PROFILE_CPU_INFO_H_
#define GB_PROFILE_CPU_INFO_H_

namespace gb::internal {

// Returns the class of the core the calling thread is running on, where a
// higher class is faster. Every core is class 0 on a CPU with one kind of core.
// The thread may move to another core as soon as this returns.
int GetCoreClass();

}  // namespace gb::internal

#endif  // GB_PROFILE_CPU_INFO_H_
