# Core-aware job threads

`gb/thread` knows the hardware threads only as a flat list of affinities in
processor order, and `FiberJobSystem` pins job thread *i* to hardware thread
*i*. Neither knows which hardware threads share a core, or which cores are
faster. On an i9-14900KF, Windows numbers the 8 performance cores' SMT pairs
0-15 and the 16 efficiency cores 16-31, so 24 job threads (one per core) end up
two to each performance core, leaving 8 efficiency cores idle.

This adds a CPU topology to `gb/thread` (the cores, their hardware threads,
their class, and the caches they share), a placement of threads by core, and
options for `FiberJobSystem` to run one job thread per core, fastest first, to
leave the fastest cores free for a program's main loop, and to report where
each job thread went. When more than one job thread is idle, new work goes to
the one on the fastest core.

Measured on that CPU with Terra Diem's chunk generation and lighting (16,384
chunks of compute-bound jobs, 2026-10-15), with one per core set up by
restricting the process affinity before creating the job system:

| Job threads | Placement | Generation | Lighting |
|---|---|---|---|
| 24 | one per core | 0.38 s | 1.96 s |
| 24 | in processor order | 0.45 s | 2.33 s |
| 32 | every hardware thread | 0.35 s | 1.83 s |

Placement mattered more than count: processor order took 17-19% longer than
one per core, while using the SMT siblings as well gained only 7-9%. While
jobs contended for the heap, every hardware thread took about 30% longer than
one per core. A performance core did about 1.6 times the work of an efficiency
core.

## Behavior

- `GetCpuTopology()` returns the cores the process can run threads on: each
  core's class (higher is faster), an affinity for the whole core and one for
  each of its hardware threads, and its L2 and last level caches, by index, so
  cores that share a cache have the same index.
- `PlaceThreads()` picks an affinity for each of a number of threads from a
  topology: one hardware thread each, or one whole core each, fastest cores
  first, optionally leaving the fastest cores without threads.
  `DescribeThreadPlacement()` writes a placement as one line for logs.
- `SetThisThreadAffinity()` pins the calling thread, so a program can place its
  main thread, which `CreateThread()` didn't make.
- `CreateFiberThreads()` takes the affinity of each thread to create, rather
  than a count and `FiberOption::kPinThreads`, which is removed. This breaks
  callers, but no project calls it.
- `FiberJobSystem`:
  - A new `one_per_core` option counts job threads in cores, one per core, each
    pinned to all of its core's hardware threads. It is off by default.
  - A new `reserved_cores` option leaves that many of the fastest cores without
    job threads.
  - `GetPlacement()` reports where each job thread went.
  - By default it still runs one job thread per hardware thread, but pins them
    in a new order: fastest cores first, and the first hardware thread of every
    core before the second of any. Using every hardware thread, that is the
    same set as before. Using fewer, the threads spread across cores before
    sharing one, so 24 job threads on an i9-14900KF get one core each.
  - A job, or a waiting job that can resume, wakes the idle job thread that is
    first in placement order, so it starts on a performance core while one is
    idle, rather than on whichever thread Windows wakes.
- `GetThisThreadCoreClass()` behaves as before, reading its classes from the
  topology.

A program that wants one job thread per core, with its main thread on the
fastest core:

```
auto job_system = gb::FiberJobSystem::Create(
    gb::Context()
        .SetValue<bool>(gb::FiberJobSystem::kKeyOnePerCore, true)
        .SetValue<int>(gb::FiberJobSystem::kKeyReservedCores, 1));
const gb::CpuTopology& topology = gb::GetCpuTopology();
const gb::ThreadPlacement& placement = job_system->GetPlacement();
LOG(INFO) << "Job threads: " << gb::DescribeThreadPlacement(topology, placement);
gb::SetThisThreadAffinity(
    topology.cores[placement.reserved_cores[0]].affinity);
```

## Design

### Names

- `CpuTopology`, `CpuCore`, `CpuCache`, `GetCpuTopology()`: "CPU" as in
  `cpu_ticks.h`. Alternatives were `ProcessorTopology` and `HardwareTopology`.
  Distinct from `gb/profile`'s `ProfilerCpu`, which is a source of timestamps,
  not a description of the hardware.
- `core_class` matches `GetThisThreadCoreClass()`, and `hardware_threads`
  matches `GetHardwareThreadAffinities()`.
- `PlaceThreads()`, `ThreadPlacement`, `ThreadPlacementOptions`: "placement"
  is the backlog item's word. Alternatives were `AssignThreads()` and
  `ChooseAffinities()`.
- `one_per_core` (`kConstraintOnePerCore`): says both what is counted and the
  limit. Alternatives were `count_cores` and `per_core`.
- `reserved_cores` (`kConstraintReservedCores`): alternatives were
  `spare_cores` and `free_cores`, and "free" reads as releasing memory.
- `SetThisThreadAffinity()` follows `GetThisThread()` and
  `GetThisThreadCycles()`.

### CPU topology (gb/thread)

```
// A cache shared by one or more cores.
struct CpuCache {
  int level = 0;
  int64_t size_bytes = 0;
};

// A core, with its hardware threads (more than one with SMT).
struct CpuCore {
  // Higher is faster. Every core is class 0 on a CPU with one kind of core.
  int core_class = 0;

  // An affinity for CreateThread() covering all of the core's hardware
  // threads, and one for each of them in processor order. Zero where the
  // platform can't pin threads.
  uint64_t affinity = 0;
  std::vector<uint64_t> hardware_threads;

  // Indices in CpuTopology::caches, or -1 if unknown. Cores with the same
  // index share the cache.
  int l2_cache = -1;
  int last_level_cache = -1;
};

struct CpuTopology {
  std::vector<CpuCore> cores;  // In processor order.
  std::vector<CpuCache> caches;
};

// Returns the cores this process can run threads on. There is always at least
// one. It is read at the first call, so a program that restricts its process
// affinity must do so before then.
const CpuTopology& GetCpuTopology();
```

- `cpu_topology.h`, with `win_cpu_topology.cc` and `gen_cpu_topology.cc`
  chosen by the build.
- Windows reads `GetLogicalProcessorInformationEx(RelationAll)`: cores from
  `RelationProcessorCore` (`EfficiencyClass` and `GroupMask`), and data or
  unified caches at level 2 and above from `RelationCache`. Records are walked
  by their `Size`, and a cache's `GroupCount` of 0 (Windows 10) means one mask.
- Like `GetHardwareThreadAffinities()`, it covers only the processor group the
  process runs in, and only the hardware threads in the process's affinity
  mask. A core with none of its hardware threads left is dropped, and one with
  one of two SMT siblings left keeps just that one. If the process spans
  processor groups (Windows 11 on more than 64 hardware threads), or Windows'
  call fails, it falls back to the generic topology.
- The parsing is a function of the buffer, the group, and the mask, declared in
  `win_cpu_topology.h` so tests can call it with buffers of their own.
- The generic topology has one class-0 core for each of `GetMaxConcurrency()`,
  each with one hardware thread whose affinity is zero, and no caches.
- `GetThisThreadCoreClass()`'s table of classes by processor is built from the
  topology. Processors outside it (other processor groups) are class 0. Today
  it covers every group, but every CPU so far with more than one class of core
  has a single group, so nothing changes in practice.
- Performance: one system call and a parse at the first call, then a static
  read.

**Brittleness:** affinities stay opaque, so a caller can only use the values
the topology gives (whole cores, or single hardware threads), never combine
them. The topology is read once, as `GetHardwareThreadAffinities()` is, so a
later change to the process affinity isn't seen.

### Thread placement (gb/thread)

```
struct ThreadPlacementOptions {
  // The number of threads. Zero places one on every core (with one_per_core)
  // or hardware thread, and a negative number that many fewer, but at least
  // one.
  int thread_count = 0;

  // Places at most one thread on each core, with an affinity for all of the
  // core's hardware threads, and counts thread_count in cores. Otherwise each
  // thread gets one hardware thread.
  bool one_per_core = false;

  // The number of cores, fastest first, left without threads. At least one
  // core is always left for threads.
  int reserved_cores = 0;

  // If false, the count is the same, but no thread is pinned.
  bool pin = true;
};

struct ThreadPlacement {
  // For each thread, its affinity for CreateThread(), or zero if it isn't
  // pinned.
  std::vector<uint64_t> affinities;

  // For each thread, the index of its core in the topology, or -1 if it isn't
  // pinned.
  std::vector<int> cores;

  // The cores left without threads by reserved_cores, fastest first.
  std::vector<int> reserved_cores;
};

ThreadPlacement PlaceThreads(const CpuTopology& topology,
                             const ThreadPlacementOptions& options);

// Such as "24 threads: cores 0-7 (class 1, 2 hardware threads each), cores
// 8-23 (class 0); reserved: none".
std::string DescribeThreadPlacement(const CpuTopology& topology,
                                    const ThreadPlacement& placement);

// Pins the calling thread to an affinity from the topology, returning false if
// it can't be.
bool SetThisThreadAffinity(uint64_t affinity);
```

- `thread_placement.h`, portable, apart from `SetThisThreadAffinity()` in
  `thread.h` with its platform implementations.
- Cores are ordered fastest class first, and in processor order within a
  class. The first `reserved_cores` of them are left out.
- With `one_per_core`, each thread gets the next core. Otherwise each gets the
  next hardware thread, taking the first of every core before the second of
  any.
- More threads than cores (or hardware threads) pins none, as `FiberJobSystem`
  does today.
- On an i9-14900KF:

  | Options | Threads | Placement |
  |---|---|---|
  | default | 32 | 8 P cores' first threads, 16 E cores, 8 P cores' second threads |
  | `thread_count` 24 | 24 | 8 P cores' first threads, 16 E cores |
  | `one_per_core` | 24 | 8 P cores (both threads each), 16 E cores |
  | `one_per_core`, `reserved_cores` 1 | 23 | P cores 1-7, 16 E cores; P core 0 reserved |

- `PlaceThreads()` is a function of the topology, so tests give it fake ones.
- Within a class, the fastest core is the first in processor order. Windows
  also reports a `SchedulingClass` for each CPU set
  (`GetSystemCpuSetInformation()`), which on this CPU marks the two "favored"
  performance cores (8 and 10) that boost highest, but it is undocumented, so
  this leaves it out (see the backlog's *Favored cores*).

**Brittleness:** with `pin` false, reserved cores only reduce the count, as an
unpinned thread may run anywhere. `FiberJobSystem`'s contract says so.

### Fiber threads (gb/thread)

```
// Creates a fiber running on a new thread for each affinity (zero for a
// thread that isn't pinned).
std::vector<FiberThread> CreateFiberThreads(
    absl::Span<const uint64_t> affinities, FiberOptions options,
    uint32_t stack_size, void* user_data, FiberMain fiber_main);
```

- Counting threads and pinning them moves to `PlaceThreads()`, so
  `FiberOption::kPinThreads` goes.

### FiberJobSystem (gb/job)

- `kConstraintOnePerCore` (bool, default false) and `kConstraintReservedCores`
  (int, default 0) pass through to `PlaceThreads()` with `thread_count` and
  `pin_threads`, from `GetCpuTopology()`.
- `const ThreadPlacement& GetPlacement() const` reports the placement, trimmed
  to the threads actually created.
- `one_per_core` stays off by default, so projects keep their thread count: on
  compute-bound work every hardware thread was 7-9% faster than one per core.
  A project opts in, as Terra Diem plans to (*Job threads per physical core*).

### Waking idle job threads (gb/job)

Today every idle job thread waits on one shared semaphore, which spins briefly
before sleeping in Windows' semaphore. A queued job goes to whichever thread is
spinning at that moment, or else to whichever sleeping thread Windows wakes, in
no set order. So while both kinds of core are idle, a job is as likely to start
on an efficiency core, and take 1.6-2 times as long, as on a performance core.

Instead, each job thread gets its own semaphore, and the idle threads are a
64-bit mask indexed in placement order, which is fastest first
(`kMaxThreadCount` is 64):
- A thread with no work sets its bit, checks the queues once more, and then
  waits on its own semaphore, spinning briefly first as now.
- Queuing a job, or a waiting job's fiber to resume, clears the lowest set bit
  and signals that thread. If no bit is set, every thread is busy, and the
  first to finish takes the work from the queue.
- Stopping signals every thread.

So while threads are idle, work goes to the performance cores first, then the
efficiency cores, then (one per hardware thread) the performance cores' second
SMT threads, which would slow the job already on that core. A light load also
stays on a few threads, leaving the rest asleep.

- Correctness: a thread sets its bit before its last check of the queues, and a
  queuer pushes before reading the mask, with a full fence in each, so either
  the queuer sees the bit or the thread sees the work. A thread that finds work
  after setting its bit clears its bit if it is still set. If a queuer cleared
  it first, the thread's semaphore was signaled, and it takes that signal
  before running the work, so none is left over.
- Performance: a sleeping thread on a performance core takes an OS wake (some
  microseconds) to start, where a thread spinning on an efficiency core would
  start at once. That loses only for jobs shorter than a wake. Measured before
  and after, with many small jobs and with single jobs on an idle job system.
- With `pin_threads` false, the order says nothing about speed, but a light
  load still stays on a few threads.

**Brittleness:** the mask and semaphores are private, set in one place when a
thread goes idle and cleared in one place when work wakes it.

### Power throttling

Windows moves the threads of a background window onto efficiency cores, and
the backlog item asked whether job threads should opt out
(`SetThreadInformation()` with `ThreadPowerThrottling`). Measured on the
i9-14900KF (2026-10-17) with a scratch program running a fixed integer loop on
threads pinned one per core, requesting EcoQoS explicitly:

| Threads | With EcoQoS |
|---|---|
| 8 pinned to P cores and 16 to E cores, all busy | no difference |
| 8 pinned to P cores | 3.5% slower |
| 1 unpinned | moved to E cores, at half the speed |

Pinning already keeps job threads where they are, so opting out would only help
job threads that aren't pinned, and isn't worth an option. A program's main
thread is the one Windows moves (*Multithreaded profiles*), and
`SetThisThreadAffinity()` keeps it in place too. This requested EcoQoS
directly, rather than putting a window in the background, on the assumption
that Windows throttles a background window's threads the same way.

### Limits

- Processes that span processor groups get the generic topology, as they get no
  affinities today (backlog: *Threads across processor groups*).
- Pinning uses affinity masks, which Windows always honors. CPU sets
  (`SetThreadSelectedCpuSets()`) are softer: threads run elsewhere while their
  CPU sets are parked or given to another process. Nothing needs that yet.
- Nothing uses the caches yet. They are there for placing work by cache later,
  such as on CPUs with two last level caches (AMD's dual-CCD parts).

### To confirm

- Record sizes and layout: on the i9-14900KF, Windows writes 48 bytes for each
  core and 56 for each cache (one group mask each), and reports each cache once
  (the L3 once, each E core cluster's L2 once). The test buffers are written in
  the same layout. Confirmed with a scratch dump (2026-10-17).

## CLs

### CL1 [ ] thread: CPU topology

Depends on: nothing.

- `cpu_topology.h`, `win_cpu_topology.h`, `win_cpu_topology.cc`,
  `gen_cpu_topology.cc`, as above.
- `win_thread.cc`: `GetThisThreadCoreClass()` reads its classes from the
  topology.
- Unused, apart from `GetThisThreadCoreClass()`, which behaves the same.

**Verify**
- Standard checks (see CLAUDE.md).
- `win_cpu_topology_test.cc`, parsing buffers written by a small builder in
  Windows' layout:
  - Hybrid, like the i9-14900KF: 8 SMT cores of class 1 with their own L2, and
    16 cores of class 0 sharing an L2 in clusters of four, all sharing one L3.
  - Uniform with SMT, and uniform without.
  - Two processor groups: only the given group's cores, with their masks.
  - Process masks that drop one SMT sibling, and a whole core.
  - Records of unknown relationships, and a cache with `GroupCount` 0, are
    handled.
- `cpu_topology_test.cc`, on the real machine: every hardware thread from
  `GetHardwareThreadAffinities()` is in exactly one core, and a thread pinned
  to each core reads that core's class from `GetThisThreadCoreClass()`.

### CL2 [ ] thread: Thread placement

Depends on: CL1.

- `thread_placement.h` and `thread_placement.cc`: `ThreadPlacementOptions`,
  `ThreadPlacement`, `PlaceThreads()`, and `DescribeThreadPlacement()`.
- `thread.h`: `SetThisThreadAffinity()`, in `win_thread.cc` and
  `gen_thread.cc`.
- Unused, so no visible change.

**Verify**
- Standard checks.
- `thread_placement_test.cc`, on fake topologies (hybrid, uniform with SMT,
  uniform without, one core): the affinities and cores each option gives
  (default, positive, zero, and negative counts, more threads than slots,
  `one_per_core`, `reserved_cores` including more than there are cores, and
  `pin` false), and the description.
- `thread_test.cc`: a thread pinned with `SetThisThreadAffinity()` runs only on
  that core's hardware threads.

### CL3 [ ] thread, job: Fiber threads take affinities

Depends on: CL2.

- `fiber.h`, `win_fiber.cc`, `gen_fiber.cc`: `CreateFiberThreads()` takes
  affinities, and `FiberOption::kPinThreads` goes.
- `FiberJobSystem` places its threads with `PlaceThreads()` from
  `thread_count` and `pin_threads`, and adds `GetPlacement()`. Its threads are
  pinned in the new order.
- Breaks callers of `CreateFiberThreads()`, but no project calls it.

**Verify**
- Standard checks.
- `fiber_test.cc`: tests of thread counts move to `thread_placement_test.cc`,
  and the rest create threads from affinities, pinned and not.
- `fiber_job_system_test.cc`: `GetPlacement()` matches `PlaceThreads()` on the
  real topology, and each job thread's affinity is the one placed.

### CL4 [ ] job: Placement by core

Depends on: CL3.

- `FiberJobSystem`: `kConstraintOnePerCore` and `kConstraintReservedCores`.

**Verify**
- Standard checks.
- `fiber_job_system_test.cc`, on the real topology (skipped where it has too
  few cores): `one_per_core` creates one job thread per core, each pinned to
  its whole core, and `reserved_cores` leaves the fastest cores without job
  threads, reporting them in `GetPlacement()`.

### CL5 [ ] job: Wake the fastest idle job thread

Depends on: CL3.

- `FiberJobSystem`: a semaphore per job thread, and the mask of idle threads,
  in place of the shared semaphore.
- `GetIdleThreadCount()`, so tests can wait (without timing) until every job
  thread is idle.

**Verify**
- Standard checks.
- `fiber_job_system_test.cc`, on the real topology with threads pinned: once
  every job thread is idle, jobs run one at a time each run on the first job
  thread in placement order, and with the first N job threads held busy, the
  next job runs on job thread N.
- Performance, with a scratch benchmark before and after: throughput of many
  small jobs, the time for a single job to start on an idle job system, and on
  the i9-14900KF, the share of a light load's jobs that run on performance
  cores.
