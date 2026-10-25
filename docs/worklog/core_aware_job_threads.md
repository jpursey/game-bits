# Core-aware job threads

`gb/thread` describes the CPU's cores (which hardware threads share a core,
which cores are faster, and which share a cache), and places threads on them,
fastest cores first. `FiberJobSystem` uses that to pin its job threads to
hardware threads or to whole cores, to leave the fastest cores free for a
program's main thread, and to start each job on the fastest idle job thread.

Before, job thread *i* was pinned to hardware thread *i*, and every idle job
thread waited on one shared semaphore. On an i9-14900KF, Windows numbers the 8
performance cores' pairs of simultaneous multithreading (SMT) hardware threads
0-15 and the 16 efficiency cores 16-31, so 24 job threads (one per core) ended
up two to each performance core, leaving 8 efficiency cores idle. Windows woke
the idle job threads in turn, so a light load cycled through all of them, and
ran on efficiency cores half the time.

## Using it

A program that wants one job thread per core, with its main thread on the
fastest core:

```
auto job_system = gb::FiberJobSystem::Create(
    gb::Context()
        .SetValue<gb::ThreadPinning>(gb::FiberJobSystem::kKeyThreadPinning,
                                     gb::ThreadPinning::kCore)
        .SetValue<int>(gb::FiberJobSystem::kKeyReservedCores, 1));
const gb::ThreadPlacement& placement = job_system->GetPlacement();
LOG(INFO) << "Job threads: " << gb::DescribeThreadPlacement(placement);
gb::SetThisThreadAffinity(
    gb::GetCpuTopology().cores[placement.reserved_cores[0]].affinity);
```

## Behavior

- **CPU topology:** `GetCpuTopology()` returns the cores the process can run
  threads on, in processor order: each core's class (higher is faster, and
  every core is class 0 on a CPU with one kind of core), an affinity for the
  whole core and one for each of its hardware threads, and its L2 and last
  level caches, by index, so cores that share a cache have the same index. It
  covers exactly the hardware threads in `GetHardwareThreadAffinities()`, and
  is read once, at the first call. Where the platform can't tell which hardware
  threads share a core, each hardware thread is a class 0 core of its own, with
  no caches. `GetThisThreadCoreClass()` reads its classes from it.
- **Placement:** `PlaceThreads()` picks where each of a number of threads runs,
  as a `PlacedThread` (an affinity, and the index of its core) for each, and
  lists the reserved cores:
  - `ThreadPinning` says what each thread is pinned to: a hardware thread of
    its own (`kHardwareThread`, the default), a whole core of its own
    (`kCore`), or nothing (`kNone`).
  - Cores are taken fastest class first, and in processor order within a
    class. Pinned to hardware threads, threads take the first hardware thread
    of every core before the second of any.
  - `reserved_cores` leaves that many of the fastest cores without threads,
    though at least one core is always left. Unpinned threads may still run on
    reserved cores, so then it only reduces their number.
  - A `thread_count` of zero means one per hardware thread (or core, with
    `kCore`) that isn't reserved. More threads than that pins none of them.
  - `DescribeThreadPlacement()` writes a placement as one line for logs.
- **Placements on an i9-14900KF**, where P cores are performance cores and E
  cores are efficiency cores:

  | Options | Threads | Placement |
  |---|---|---|
  | default | 32 | 8 P cores' first threads, 16 E cores, 8 P cores' second threads |
  | `thread_count` 24 | 24 | 8 P cores' first threads, 16 E cores |
  | `kCore` | 24 | 8 P cores (both threads each), 16 E cores |
  | `kCore`, `reserved_cores` 1 | 23 | P cores 1-7, 16 E cores; P core 0 reserved |

- `SetThisThreadAffinity()` pins the calling thread, so a program can place
  its main thread, which `CreateThread()` didn't make.
- `CreateFiberThreads()` takes a `PlacedThread` for each thread to create,
  rather than a count and `FiberOption::kPinThreads`, which is gone. A
  placement's `threads` pass straight through, and
  `std::vector<PlacedThread>(count)` creates `count` threads that aren't
  pinned.
- **`FiberJobSystem`:**
  - `thread_pinning` (`kConstraintThreadPinning`) replaces `pin_threads`, and
    `reserved_cores` (`kConstraintReservedCores`) is new. Both pass through to
    `PlaceThreads()` with `thread_count`.
  - `thread_count` is an exact number of job threads (at most
    `kMaxThreadCount`, 64), or zero for one per hardware thread (or core,
    pinned to cores) that isn't reserved. A negative count, which used to leave
    out the slowest hardware threads, makes `Create()` fail. `reserved_cores`
    leaves out the fastest instead, which is what a program's main thread
    wants.
  - By default it still runs one job thread per hardware thread, pinned in the
    new order. Using every hardware thread, that is the same set as before.
    Using fewer, the threads spread across cores before sharing one.
  - A job, or a waiting job that can resume, wakes the idle job thread that is
    first in placement order. So while job threads are pinned, jobs start on
    the fastest idle core, and a light load stays on a few job threads while
    the rest sleep.
  - `GetPlacement()` reports where each job thread runs, and
    `GetIdleThreadCount()` how many are waiting for work, so a test can wait
    until every job thread is idle without timing.
- **Breaking changes:** callers of `CreateFiberThreads()`, and of
  `FiberJobSystem` that set `pin_threads` or a negative `thread_count`, but no
  project did.

## Structure

| File | What |
|---|---|
| `cpu_topology.h` | `CpuTopology`, `CpuCore`, `CpuCache`, and `GetCpuTopology()` |
| `win_cpu_topology.h`, `.cc` | Reading the topology from Windows |
| `gen_cpu_topology.cc` | The hardware thread topology, on other platforms |
| `cpu_topology_internal.h`, `.cc` | The hardware thread topology, for both |
| `thread_placement.h` | `ThreadPinning`, `ThreadPlacementOptions`, `PlacedThread`, `ThreadPlacement`, `PlaceThreads()`, and `DescribeThreadPlacement()` |
| `thread.h` | `SetThisThreadAffinity()`, in `win_thread.cc` and `gen_thread.cc` |
| `fiber.h` | `CreateFiberThreads()` |
| `gb/job/fiber_job_system.h` | The job system's options, `GetPlacement()`, and `GetIdleThreadCount()` |

All but the last are in `gb/thread`.

- **Topology on Windows:** one call to
  `GetLogicalProcessorInformationEx(RelationAll)` at the first use: cores from
  `RelationProcessorCore` (`EfficiencyClass` and `GroupMask`), and data or
  unified caches at level 2 and above from `RelationCache`. Records are walked
  by their `Size`, and a cache with a `GroupCount` of 0 (Windows 10) has one
  mask. Only the process's processor group and affinity mask count: a core
  with none of its hardware threads left is dropped, and one with one SMT
  sibling left keeps just that one. The parse is a function of the buffer, the
  group, and the mask, declared in `win_cpu_topology.h`, so tests give it
  buffers written in Windows' layout. If the process spans processor groups,
  or Windows' call fails, it falls back to the hardware thread topology.
- **Hardware thread topology:** a class 0 core for each hardware thread, with
  no caches. Each core is pinned to its hardware thread if
  `GetHardwareThreadAffinities()` has affinities, and otherwise has an affinity
  of zero.
- **Core classes:** `GetThisThreadCoreClass()`'s table of classes by processor
  is built from the topology. Processors outside it (other processor groups)
  are class 0.
- **Placement** is portable, and `PlaceThreads()` and
  `DescribeThreadPlacement()` take an optional topology, so tests give them
  fake ones.
- **Job threads** are created from the placement, which is then trimmed to the
  threads actually created. A job thread finds its index by looking itself up
  in the job system's threads (`GetThisThread()` is safe on fibers, which move
  between threads), so job threads wait at start until the job system has them
  all.
- **Waking idle job threads:** each job thread has a semaphore of its own, and
  the idle job threads are a 64-bit mask, by index in placement order:
  - A count of queued jobs and fibers stands in for a shared semaphore's
    count. A thread claims one before taking a job or fiber from the queues,
    and retries the queues until it gets one, as `ConcurrentQueue` may not yet
    show an entry another thread just queued.
  - A thread with nothing to claim sets its bit, and checks the count once
    more. If there is work, it wakes the first idle thread (which may be
    itself) rather than taking it, so work always goes to the first idle
    thread. Then it waits on its semaphore, which spins briefly before
    sleeping.
  - Queuing work adds to the count, then clears the lowest set bit and signals
    that thread. If no bit is set, every thread is busy, and the first to
    finish takes the work. Stopping signals every thread.
  - Every step is sequentially consistent: a thread sets its bit before its
    last check of the count, and a queuer adds to the count before reading the
    mask, so either the queuer sees the bit or the thread sees the work. Only
    a thread sets its own bit, and only whoever clears it signals its
    semaphore, so every signal is taken by the wait that follows.
  - Each job thread's handle, semaphore, and a flag it sets while waiting share
    a cache line of their own, and the count and mask share another.
    `GetIdleThreadCount()` counts the flags rather than the mask's bits: a
    thread sets its flag only after its last check for work, so once every
    flag is set, a queued job wakes only the first thread, which tests rely
    on.

Reusable pieces, should anything else need them: `GetCpuTopology()` and
`PlaceThreads()`, to place any set of threads, and `SetThisThreadAffinity()`,
for a thread `CreateThread()` didn't make. Nothing uses the topology's caches
yet. They are there for placing work by cache, such as on CPUs with two last
level caches (AMD's dual-CCD parts).

## Measurements

All on an i9-14900KF: 8 performance cores of class 1 with two SMT hardware
threads each, and 16 efficiency cores of class 0.

- **Placement:** Terra Diem's chunk generation and lighting (16,384 chunks of
  compute-bound jobs, 2026-10-15), with one per core set up by restricting the
  process affinity before creating the job system:

  | Job threads | Placement | Generation | Lighting |
  |---|---|---|---|
  | 24 | one per core | 0.38 s | 1.96 s |
  | 24 | in processor order | 0.45 s | 2.33 s |
  | 32 | every hardware thread | 0.35 s | 1.83 s |

  Placement mattered more than count: processor order took 17-19% longer than
  one per core, while using the SMT siblings as well gained only 7-9%. While
  jobs contended for the heap, every hardware thread took about 30% longer
  than one per core. A performance core did about 1.6 times the work of an
  efficiency core. As every hardware thread was faster on compute-bound work,
  `kHardwareThread` stays the default, and a project opts in to `kCore`, as
  Terra Diem plans to (*Job threads per physical core*).
- **Waking idle job threads:** 32 job threads (the default) in Release, with a
  scratch benchmark (2026-10-24), before and after:

  | | Before | After |
  |---|---|---|
  | 200,000 empty jobs run from the main thread | 284 ms | 102-113 ms |
  | 200,000 empty jobs run from a job, which waits for them | 270-370 ms | 84-91 ms |
  | A single job on an idle job system, until it starts (median, 90th percentile) | 27-29 µs, 62 µs | 17-21 µs, 35-38 µs |
  | A single job right after the last one finished | 5-6 µs | 0.2-0.4 µs |
  | Light load (1, 4 or 8 jobs of 100 µs at a time): share on performance cores | 50% | 100% |

  Throughput improved as well, as a job queued while every thread is busy
  signals nothing, where before each signaled the shared semaphore.
- **Power throttling:** Windows moves the threads of a background window onto
  efficiency cores. A scratch program ran a fixed integer loop on threads
  pinned one per core, requesting EcoQoS explicitly (2026-10-17):

  | Threads | With EcoQoS |
  |---|---|
  | 8 pinned to P cores and 16 to E cores, all busy | no difference |
  | 8 pinned to P cores | 3.5% slower |
  | 1 unpinned | moved to E cores, at half the speed |

  This assumes Windows throttles a background window's threads the same way.
- **Windows' records:** Windows writes 48 bytes for each core and 56 for each
  cache (one group mask each), and reports each cache once (the L3 once, each
  E core cluster's L2 once). The test buffers are written in the same layout.

## Deliberate choices

- Job threads don't opt out of power throttling (`SetThreadInformation()` with
  `ThreadPowerThrottling`): pinning already keeps them where they are, so it
  would only help job threads that aren't pinned. A program's main thread is
  the one Windows moves, and `SetThisThreadAffinity()` keeps it in place too.
- Pinning uses affinity masks, which Windows always honors, rather than CPU
  sets (`SetThreadSelectedCpuSets()`), which are softer: threads run elsewhere
  while their CPU sets are parked or given to another process.
- Affinities stay opaque, so a caller can only use the values the topology
  gives (whole cores, or single hardware threads), never combine them. The
  topology is read once, as `GetHardwareThreadAffinities()` is, so a program
  that restricts its process affinity must do so first.
- `CreateFiberThreads()` has no overload taking `ThreadPlacementOptions`: the
  job system keeps its placement (for `GetPlacement()`, and to order idle job
  threads), so only tests would use it.
- A sleeping job thread on a performance core takes an OS wake (some
  microseconds) to start, where one spinning on an efficiency core would start
  at once, so waking the fastest idle thread loses only for jobs shorter than
  a wake.

## Known limitations

Each is a backlog item:

- **Threads across processor groups:** a process that spans processor groups
  (Windows 11, on more than 64 hardware threads) has no affinities, so its
  topology is a single core that can't be pinned.
- **Favored cores:** within a class, the fastest core is taken to be the first
  in processor order, though Windows' undocumented `SchedulingClass` marks the
  cores that boost highest.
