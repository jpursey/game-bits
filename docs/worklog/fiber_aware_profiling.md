# Fiber-aware profiling

A timed point can span a fiber switch, so a job can time a phase of work that
waits on a job counter. When a fiber suspends, its open timers pause and leave
the thread's timer stack, and when it resumes, on any thread, they carry on
there, so a job that waits is charged only for the time it runs. The time a
point spent suspended is recorded as its wait time, since a phase that mostly
waits for other jobs needs a different fix from one that runs long.

It is a small addition to `gb/thread` (fiber switch callbacks) and changes to
`Profiler`, with one new public reader, `GetWaitTime()`. A timed point costs
what it did, and a fiber switch about 2.5ns more with no timers open.

Requested by Terra Diem *Profile the world's jobs*, which plans phases of work
that wait on job counters inside jobs. *Profiled jobs*, which gives each job
thread a Profiler, builds on this.

## Using it

```
// In a job, on a thread with a Profiler:
{
  gb::ProfileScope<"World::Update"> scope;
  ...
  gb::JobSystem::Wait(&counter);  // Other jobs run, here or elsewhere.
  ...
}  // Ends in the Profiler of whichever thread the job is on now.
```

## Behavior

- **Fiber switch callbacks** (`gb/thread`): `SetFiberSwitchCallbacks()` sets
  callbacks that every `SwitchToFiber()` calls: `on_suspend` on the fiber that
  suspends, just before it switches, and `on_resume` on the same fiber when it
  resumes, on the thread it resumed on, with what `on_suspend` returned. A
  library carries what belongs to a fiber across a switch with them, without
  `gb/thread` knowing of it. A program has one set, which the first Profiler
  sets; setting a different set while one is set CHECK-fails, and setting the
  same one again does nothing. A switch that fails calls neither, and the
  generic implementation, which has no switches, ignores them.
- **Timed points across switches:** a scope or call can span a fiber switch.
  - A timer must be destroyed on the fiber it was created on, now on any
    thread, after the timers created inside it (CHECK). Ending a timer on
    another fiber CHECK-fails too.
  - Its time is the time it ran, on every thread it ran on. Its self time and
    its part in a frame are recorded in the Profiler of the thread it ends on.
  - For an outermost point, each joined thread's busy time has the part that
    ran on that thread, counted as the fiber suspends there or as the point
    ends, so a job that moves doesn't make the thread it ends on look busier
    than it was.
  - Timers that resume on a thread with no Profiler record nothing, even if the
    fiber later moves to a thread with one, as a timer started on a thread with
    no Profiler is ignored.
  - A frame can't span a fiber switch: switching fibers with a frame in
    progress CHECK-fails.
- **Wait time:** the time from a fiber's suspending to its resuming is the wait
  time of the innermost timed point open on it, as self time is charged to the
  innermost point. It is recorded as the fiber resumes, in the Profiler of the
  thread it resumes on, so it counts in the frame in progress then.
  - It is measured from one thread's ticks to another's, which agree for the
    CPU's invariant timestamp counter, and is never less than zero. Tests with a
    `FakeProfilerCpu` per thread keep them in step.
  - The report's points table gains Wait and Wait/call columns once any point
    has waited, and a frame breakdown a Wait column once a point in that frame
    has, so the reports of programs that never wait don't change.
  - A point that has waited but not yet ended counts as recorded, with a count
    of zero and no time per call.
  - A main Profiler's wait times are the sum over every thread, as self times
    are. `GetWaitTime()` reads one in tests.
- **Reset:** timed points open on a suspended fiber when a Profiler is reset
  record all their time once they end, and their whole wait once they resume.

No public API broke.

## Structure

- **Callbacks** (`fiber.h`, `win_fiber.cc`): `SwitchToFiber()` loads the
  callbacks once (an atomic pointer), calls `on_suspend` just before
  `::SwitchToFiber()`, and keeps what it returns and the callbacks in locals on
  the suspending fiber's own stack. It calls `on_resume` with them once the
  fiber resumes and has released the fiber it came from. Nothing is stored per
  fiber, and a fiber resumes with the callbacks it suspended with. With none
  set, a switch pays an atomic load and a branch.
- **Timing:** `Profiler::Timing` holds the Profiler it ends in and its point's
  index, which moved out of `ProfileTimer` and `ProfileFrame`, so a switch can
  move a timing to another Profiler. The thread's stack of timings only ever
  holds the running fiber's, since every switch takes the whole stack and a
  fiber starts with none.
- **Suspending** (`SuspendTimings()`): with no Profiler on the thread or no
  timing open, it returns null. Otherwise it CHECKs that no frame is in
  progress, replaces each open timing's start with its ticks run so far, adds
  the outermost timing's run on this thread to a joined Profiler's busy time,
  notes the suspend ticks in the innermost, empties the stack, and returns the
  innermost. Each timing keeps its Profiler while suspended, so a timer ended
  on another fiber meanwhile fails `EndTiming()`'s CHECK rather than being
  ignored.
- **Resuming** (`ResumeTimings()`): with no Profiler on the thread, it clears
  each timing's Profiler, so they record nothing. Otherwise it CHECKs that the
  thread's stack is empty, adds the wait to the innermost timing's point, turns
  each timing's ticks so far back into a start, sets its Profiler, and makes
  them the thread's stack.
- **Run time** is measured in each thread's own ticks, so it doesn't depend on
  the threads' counters agreeing. Only the wait does.
- **Busy time across threads:** the outermost timing keeps the ticks it ran
  before its fiber last resumed (`prior_run_ticks`), already in the busy time
  of the threads they ran on, and its end adds only the rest.
- **Wait in slots:** each slot's `wait_ticks` and `frame_wait_ticks` sit with
  the other fields a timed point writes, before the value fields, as a new
  frame's first add resets `frame_wait_ticks` with the frame's other parts.
  `PointTotals` and `FrameBreakdown::Point` carry `wait_ticks`, gathered,
  folded for destroyed Profilers, and subtracted from baselines as self ticks
  are. `PointTotals::IsRecorded()` says whether a point appears in the report.

Reusable piece, should anything else need it: the fiber switch callbacks, for
anything else that belongs to a fiber rather than a thread.

## Measurements

On the development machine (an i9-14900KF), in RelWithDebInfo builds:

- **A fiber switch,** averaged over the two switches of a round trip between
  two fibers on one thread: 51-51.6ns with no callbacks set, 52.0ns with
  callbacks that do nothing, 53.5ns with the profiler's and no open timings,
  and 63.25ns with three timings open on one of the two fibers.
- **A timed point:** 12.25ns as `GetPointCost()` measures it, against
  11.75-12.25ns before, as its measurement now includes storing the Profiler
  and index, which a timer stored outside `StartTiming()` before. A scope in a
  loop measured 14.75-15.25ns across the CLs, which leaving a new field
  uninitialized didn't change.
- **A timing's fields after a switch:** a timing's address is on the fiber's
  stack, so code that switches reads its fields fresh even in an optimized
  build; the tests that move a fiber between threads pass in RelWithDebInfo.
</content>
</invoke>
