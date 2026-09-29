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
timeline of events instead, which this is not.

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
