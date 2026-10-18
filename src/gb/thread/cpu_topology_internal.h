// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_CPU_TOPOLOGY_INTERNAL_H_
#define GB_THREAD_CPU_TOPOLOGY_INTERNAL_H_

#include "gb/thread/cpu_topology.h"

namespace gb::internal {

//==============================================================================
// Parts of GetCpuTopology() (see cpu_topology.h) shared by every platform. This
// is private to gb_thread and its tests.
//==============================================================================

// Returns a topology with a class 0 core for each hardware thread and no
// caches, for when the current platform cannot tell which hardware threads
// share a core. Each core is pinned to its hardware thread, if
// GetHardwareThreadAffinities() has one for each.
CpuTopology MakeHardwareThreadTopology();

}  // namespace gb::internal

#endif  // GB_THREAD_CPU_TOPOLOGY_INTERNAL_H_
