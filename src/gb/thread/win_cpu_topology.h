// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_WIN_CPU_TOPOLOGY_H_
#define GB_THREAD_WIN_CPU_TOPOLOGY_H_

#include <cstddef>
#include <cstdint>

#include "absl/types/span.h"
#include "gb/thread/cpu_topology.h"
#include "gb/thread/thread_types.h"

namespace gb::internal {

//==============================================================================
// How GetCpuTopology() (see cpu_topology.h) reads the topology on Windows. This
// is private to gb_thread and its tests.
//==============================================================================

// Parses the buffer GetLogicalProcessorInformationEx() fills, keeping only the
// hardware threads in processor group `group` that are in `mask`. The buffer
// holds records of different sizes one after another, each starting with its
// relationship and its size.
//
// Cores come from RelationProcessorCore records, and caches from data and
// unified caches at level 2 and above in RelationCache records. Records of any
// other relationship are skipped. A core or cache with none of its hardware
// threads in `mask` is left out.
CpuTopology ParseCpuTopology(absl::Span<const std::byte> buffer, int group,
                             uint64_t mask);

}  // namespace gb::internal

#endif  // GB_THREAD_WIN_CPU_TOPOLOGY_H_
