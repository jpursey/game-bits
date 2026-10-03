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

## Profile times that hold across sessions

- **Layers:** profile
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** JPRSurf *Profiles after Game Bits' timing fix*
- **Background:** [Profiler module](worklog/profiler_module.md)

On one machine, an i9-14900KF with 8 performance and 16 efficiency cores,
profiles of the same scenario vary by about 30% between sessions, with every
point faster or slower together, including code that didn't change. Something
shifts the whole profile, not the code. There are two explanations:
- **The tick rate is wrong.** Times are `__rdtsc()` ticks divided by a rate
  measured once, over 1ms against the steady clock (`GetTicksPerSecond()` in
  `profiler.cc`). A wrong rate scales every time by the same factor, which is
  the symptom. With an invariant TSC (a constant rate whatever the core or its
  clock speed, kept in step across cores, as on recent Intel and AMD CPUs) the
  rate should be the same every session, but nothing checks that it is.
- **The code ran slower.** The TSC counts time, not work. On an efficiency
  core, or a core Windows has slowed (such as the power throttling it applies
  to a window in the background), the same code takes longer, and the profile
  is right to say so. Then the conditions vary, not the measurement.

The machine is set up to keep its speed: core parking is off in Windows, and C
states are off in the BIOS. Its cores still slow to 800MHz with nothing
running. With REAPER idle for over a minute, the performance cores ran at
5400-5700MHz and the efficiency cores at 4200-4400MHz (HWMonitor, 2026-10-03).
So a core of one kind varies by about 5%, and the two kinds differ by about
30% in clock speed alone, before the efficiency cores' lower work per clock.
That makes the core a run was on the likelier cause, but nothing has shown it
yet.

The report now tells them apart. Its frame summary has the tick rate measured
and the rate CPUID leaf 0x15 reports (both 3.19GHz on this machine), and a
rate that changes between sessions, or differs from the CPU's, is the first. It
splits frames by the efficiency class of the core they ran on (here, class 1
for performance cores and 0 for efficiency cores), and warns when a profile
mixes them. Frames on slower cores, or a whole session on them, are the
second. Reading the core's class at each end of a frame added 4.8ns to a frame;
a timed point costs what it did.

What is left is the fix, once profiles from that machine show which it is: a
rate from CPUID, or a longer, checked calibration, for the first; a way to
profile on one kind of core for the second. Pinning a thread to one kind of
core needs the table of each processor's class that `gb/profile`'s
`win_cpu_info.cc` builds, which would then move to `gb/thread` beside its
affinity code, with `gb/profile` calling it. Any change to what a timed point or
a frame costs says so here, as projects set a budget for the profiler's own
cost on it.

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
