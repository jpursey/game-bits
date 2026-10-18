// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_THREAD_THREAD_PLACEMENT_H_
#define GB_THREAD_THREAD_PLACEMENT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "gb/thread/cpu_topology.h"
#include "gb/thread/thread_types.h"

namespace gb {

//==============================================================================
// This file chooses where a set of threads run: which core, or which of a
// core's hardware threads, each is pinned to (see cpu_topology.h).
//==============================================================================

// How PlaceThreads() pins each thread.
enum class ThreadPinning {
  // Threads are not pinned, so the operating system may run them on any core,
  // including reserved ones. Threads are counted as for kHardwareThread.
  kNone,

  // Each thread is pinned to a hardware thread of its own, taking the first
  // hardware thread of every core before the second of any.
  kHardwareThread,

  // Each thread is pinned to a core of its own, and all of its hardware
  // threads.
  kCore,
};

// Options for PlaceThreads().
struct ThreadPlacementOptions {
  ThreadPinning pinning = ThreadPinning::kHardwareThread;

  // The number of cores, fastest first, to leave without threads, such as for
  // a program's main thread. At least one core is always left for threads.
  int reserved_cores = 0;

  // The number of threads, or zero for one on each core (with kCore) or
  // hardware thread that isn't reserved. If this is more than there are of
  // those, none of the threads are pinned.
  int thread_count = 0;
};

// Where one thread runs.
struct PlacedThread {
  // The thread's affinity for CreateThread(), or zero if it isn't pinned.
  uint64_t affinity = 0;

  // The index of the thread's core in the topology, or -1 if it isn't pinned.
  int core = -1;
};

// Where each of a set of threads runs.
struct ThreadPlacement {
  std::vector<PlacedThread> threads;

  // Cores kept free of these threads for other work, such as a program's main
  // thread, as indices in the topology.
  std::vector<int> reserved_cores;
};

// Places threads on the cores of `topology` (GetCpuTopology() if null), fastest
// cores first. Cores of the same class are taken in processor order, and the
// fastest are reserved (see ThreadPlacementOptions::reserved_cores).
//
// Every thread is pinned to a different core or hardware thread, unless there
// are more threads than there are of those to go around, or the topology
// cannot pin threads.
ThreadPlacement PlaceThreads(const ThreadPlacementOptions& options,
                             const CpuTopology* topology = nullptr);

// Returns a one line description of where the threads went, for logs, such
// as "30 threads: cores 1-7 (class 1, 2 threads each), cores 8-23 (class 0);
// reserved: core 0 (class 1)". The cores are those of `topology`
// (GetCpuTopology() if null).
std::string DescribeThreadPlacement(const ThreadPlacement& placement,
                                    const CpuTopology* topology = nullptr);

}  // namespace gb

#endif  // GB_THREAD_THREAD_PLACEMENT_H_
