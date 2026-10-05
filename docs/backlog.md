# Backlog

Work that isn't being done yet, for Game Bits itself or asked for by the
projects built on it. The worklogs in `docs/worklog/` describe what was built;
this is what has not been. See [workflow.md](workflow.md) for how items are
written, picked up, and requested by other projects.

The list is a rough stack rank: the order the items look worth doing, as a best
guess rather than a commitment. Re-order it freely.

Each item carries:

- **Layers**: the Game Bits libraries it touches, lower tiers first. More than
  one usually means more than one CL.
- **Size**: a guess. *Small* is a single CL. *Medium* is a few. *Large* is
  many, usually after a design.
- **Feature workflow**: whether the item follows the feature workflow, with a
  design and a `docs/worklog/` plan of CLs. Anything that comes down to one or
  two simple CLs doesn't, and is done as an ordinary change.
- **Depends on**: other items that should come first, or "nothing".
- **Requested by**: the project and item that need it, for items another
  project asked for. For ranking only.
- **Background**: where the context is, if anywhere.

## Fiber-safe thread locals

- **Layers:** thread, job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Profiler module](worklog/profiler_module.md) (To confirm)

A fiber sees the `thread_local` variables of the thread it runs on, and
without MSVC's `/GT` option the compiler can cache a thread local's address
across a fiber switch, so a fiber that moved threads reads the old thread's
storage (freed, if that thread exited). `FiberJobSystem` moves fibers between
threads, and `job_system.cc` keeps the current `JobSystem` in a thread local,
as `gb/thread` keeps the current thread. Check whether a job that waits and
resumes on another thread can read the wrong value, with a test that does
exactly that, and fix it if so: `/GT` on the affected code, or state that
moves with the fiber.

## Multithreaded profiles

- **Layers:** profile
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Profiler module*
- **Background:** [Profiler module](worklog/profiler_module.md)

Combine the profiles of several threads, each with its own `Profiler`, into
one view: CPU time and calls per point across all threads, per frame of the
main loop, and how busy each thread is per frame. Point indices are global, so
every Profiler's slots line up and combining them is a sum. Each thread adds a
snapshot into a shared total at a safe point of its own (the end of its frame,
or between jobs), or the slots become atomics written only by their own thread
(plain moves on x64) and are read live. Overlap and critical paths need a
timeline of events instead, which this is not. `Profiler::GetReport()` reads
its own slots, so the combined view needs the report to format a snapshot of
slots instead, which a combined total can also provide.

The kind of core a thread runs on matters as much as what it runs. On an
i9-14900KF (8 performance and 16 efficiency cores), frames on efficiency cores
took about 25-30% longer than the same frames on performance cores, and
Windows moved a program's main thread onto them when its window was in the
background (2026-10-05). The tick rate was not at fault: it measured the same
in every session, matching what CPUID reports. The single-thread report
already splits frames by core class and warns when they mix; a combined view
needs the same per thread, since job threads spread across both kinds of core.
Pinning threads to one kind would make profiles compare, but `gb/thread`'s
affinities don't know which hardware threads are which kind. Only
`gb/profile`'s `win_cpu_info.cc` builds that table, which would move to
`gb/thread` for it.

## Fiber-aware profiling

- **Layers:** thread, profile
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Profiler module*, *Fiber-safe thread locals*
- **Background:** [Profiler module](worklog/profiler_module.md)

Let a timed point span a fiber switch, which is a CHECK failure today. When a
fiber switches out, its open timers pause and leave the thread's timer stack,
and when it switches back in, on any thread, they resume there, so a job that
waits is charged only for the time it runs. Needs a hook in `gb/thread`'s fiber
switch that the profiler can use without `gb/thread` depending on it. Most
useful alongside *Multithreaded profiles*, since fiber jobs are the main
multithreaded case.

## Remove Callback

- **Layers:** base
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** none

`gb::Callback` in `gb/base/callback.h` is now only an alias of
`absl::AnyInvocable`, and nothing in Game Bits uses it. It remains so that
projects built on Game Bits keep compiling until they switch to
`absl::AnyInvocable` themselves. Once none use it, delete `callback.h` and
remove it from `gb_base`. This breaks any project still using it.

## Log file

- **Layers:** base
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** Terra Diem *Log file*, JPRSurf *Use Game Bits' log file*
- **Background:** JPRSurf's `src/jpr/common/log_file.h` and `local_time.h`

Abseil logging only writes to stderr, which a Windows (non-console) program or
a DLL loaded by another program doesn't show. Add a `LogFile`: an RAII object
that, for as long as it lives, writes every Abseil log message at or above a
minimum level to a file, which it replaces when it is created. Each message is
flushed as it is written, so the log is complete after a crash or a failed
`CHECK`. The first `LogFile` also initializes Abseil logging.

JPRSurf already has this, proven in use, in `jpr/common/log_file.{h,cc}`, and
it knows nothing of JPRSurf. Move it rather than writing a new one, with:
- The file written without iostreams (it uses `std::ofstream` today), per the
  C++ style.
- `GetLocalTimeZone()` (`jpr/common/local_time.{h,cc}`) moved with it, for the
  timestamps. `absl::LocalTimeZone()` returns UTC inside some host processes,
  so this reads the zone from Windows, behind a portable declaration.
- Tests that log from several threads and read the file back, and that a
  second `LogFile` on the same path replaces the file.

The log's location stays the caller's choice. JPRSurf's `GetLogPath()`, which
puts files under `%APPDATA%`, stays in JPRSurf.
