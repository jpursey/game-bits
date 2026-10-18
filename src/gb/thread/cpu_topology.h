// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_CPU_TOPOLOGY_H_
#define GB_THREAD_CPU_TOPOLOGY_H_

#include <cstdint>
#include <vector>

#include "gb/thread/thread_types.h"

namespace gb {

//==============================================================================
// This file describes the cores a process can run threads on: which hardware
// threads share a core, which cores are faster, and which share a cache.
//==============================================================================

// A cache shared by one or more cores.
struct CpuCache {
  int level = 0;
  int64_t size_bytes = 0;
};

// A core, with its hardware threads: the threads it can run at the same time,
// sharing the core's execution units. Many cores have two.
struct CpuCore {
  // The class of the core, where a higher class is faster. Classes count up
  // from 0 for the slowest kind of core, so on a CPU with one kind of core (or
  // a platform that cannot tell them apart) every core is class 0. This is what
  // GetThisThreadCoreClass() returns on this core.
  int core_class = 0;

  // An affinity for CreateThread() covering all of the core's hardware threads,
  // and one for each of them, in processor order. These are zero if the
  // current platform cannot pin threads (see GetHardwareThreadAffinities()).
  uint64_t affinity = 0;
  std::vector<uint64_t> hardware_threads;

  // The core's level 2 (L2) and last level caches, as indices in
  // CpuTopology::caches, or -1 if unknown. Cores with the same index share the
  // cache. On a CPU without a level 3 cache, both are the L2 cache.
  int l2_cache = -1;
  int last_level_cache = -1;
};

// The cores a process can run threads on, in processor order, and the caches
// they share.
struct CpuTopology {
  std::vector<CpuCore> cores;
  std::vector<CpuCache> caches;
};

// Returns the cores this process can run threads on.
//
// This covers the hardware threads in the process's affinity, as
// GetHardwareThreadAffinities() does, so it has every one of them, each in
// exactly one core. If the current platform cannot tell which hardware threads
// share a core, every hardware thread is a class 0 core of its own, and there
// are no caches. There is always at least one core.
//
// The topology is read at the first call, so a program that restricts its
// process affinity must do so before then.
//
// This function is thread-safe.
const CpuTopology& GetCpuTopology();

}  // namespace gb

#endif  // GB_THREAD_CPU_TOPOLOGY_H_
