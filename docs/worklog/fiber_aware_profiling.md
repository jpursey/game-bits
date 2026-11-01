# Fiber-aware profiling

Lets a timed point span a fiber switch, which CHECK-fails today, so a job can
time a phase of work that waits on a job counter. When a fiber suspends, its
open timers pause and leave the thread's timer stack, and when it resumes, on
any thread, they resume there, so a job that waits is charged only for the time
it runs. The time a point spent suspended is recorded as its wait time, since a
phase that mostly waits for other jobs needs a different fix from one that runs
long.

Requested by Terra Diem *Profile the world's jobs*, which plans phases of work
that wait on job counters inside jobs. *Profiled jobs*, which gives each job
thread a Profiler, builds on this.

## Behavior

- **Fiber switch callbacks** (`gb/thread`): `SetFiberSwitchCallbacks()` sets
  callbacks that every `SwitchToFiber()` calls, one on the fiber that suspends,
  just before it switches, and one on the same fiber when it resumes, on the
  thread it resumed on, with what the first returned. A library carries what
  belongs to a fiber across a switch with them, without `gb/thread` knowing of
  it. A program has one set, which the profiler sets when the first Profiler is
  made.
- **Timed points across switches:** a scope or call can span a fiber switch.
  - A timer must still be destroyed on the fiber it was created on, now on any
    thread, and in the reverse order timers were created (CHECK, as now).
  - Its time is the time it ran, on every thread it ran on. Its self time and
    its part in a frame are recorded in the Profiler of the thread it ends on.
    For an outermost point, each joined thread's busy time has the part that
    ran on it, counted as the fiber suspends there, or as the point ends, so a
    job that moves doesn't make the thread it ends on look busier than it was.
  - Timers that resume on a thread with no Profiler record nothing, even if the
    fiber later moves to a thread with one, as a timer started on a thread with
    no Profiler is ignored today.
  - A frame can't span a fiber switch: switching fibers with a frame in
    progress CHECK-fails.
- **Wait time:** the time from a fiber's suspending to its resuming is the wait
  time of the innermost timed point open on it, as self time is charged to the
  innermost point. It is recorded as the fiber resumes, in the Profiler of the
  thread it resumes on, so it counts in the frame in progress then.
  - The report's points table gains Wait and Wait/call columns, and frame
    breakdowns a Wait column, once any point has waited, so the reports of
    programs that never wait don't change. `GetWaitTime()` reads it in tests.
  - It is measured from one thread's ticks to another's, which agree for the
    CPU's timestamp counter. Tests with a `FakeProfilerCpu` per thread keep
    them in step.

No public API breaks.

## Design

### Names

- `FiberSwitchCallbacks`, with `on_suspend` and `on_resume`, and
  `SetFiberSwitchCallbacks()`, in `gb/thread/fiber.h`. "Suspend" is the word
  `FiberState::kSuspended` already uses. Not "hook", which
  `gb/base/function_hook.h` uses for what a `FunctionHook` calls, and not
  "observer", as `on_suspend` returns state. Alternative:
  `FiberSwitchListener`.
- "Wait time": `Profiler::GetWaitTime()`, and the report's "Wait" columns. Not
  "idle", which *Profiled jobs* uses for a job thread with no work, nor
  "blocked", for its contended mutex waits.
- `Profiler::SuspendTimings()` and `ResumeTimings()`, the profiler's callbacks.

### Fiber switch callbacks (gb/thread)

```
// Callbacks that every SwitchToFiber() calls, so a library can carry what
// belongs to a fiber, such as the profiler's open timers, from the thread it
// suspends on to the thread it resumes on.
struct FiberSwitchCallbacks {
  // Called on the fiber that is switching away, on its thread, just before it
  // suspends. Returns what is passed to on_resume when this fiber resumes.
  void* (*on_suspend)();

  // Called on the fiber that suspended, as soon as it resumes, on the thread
  // it resumed on, with what on_suspend returned.
  void (*on_resume)(void* state);
};

// Sets the callbacks every SwitchToFiber() calls from now on, or none if
// `callbacks` is null. Both must be set (CHECK), and they must outlive every
// fiber switch. A program has one set: setting others while some are set
// CHECK-fails.
void SetFiberSwitchCallbacks(const FiberSwitchCallbacks* callbacks);
```

- `SwitchToFiber()` loads the callbacks once (an atomic pointer), calls
  `on_suspend` just before `::SwitchToFiber()`, keeps what it returns and the
  callbacks in locals on the suspending fiber's own stack, and calls
  `on_resume` with them once the fiber resumes and has released the fiber it
  came from. Nothing is stored per fiber, and a fiber resumes with the
  callbacks it suspended with, even if they changed meanwhile.
- Only `SwitchToFiber()` calls them. A fiber's first run starts in its main,
  not in a switch, and a fiber that exits, or a thread starting its first
  fiber, has nothing to carry. A switch that fails calls neither.
- `gen_fiber.cc` has no switches, and ignores them.
- Cost: an atomic load and a branch per switch with none set.

**Brittleness:** `on_suspend` and `on_resume` are paired by `SwitchToFiber()`
itself, so no caller can miss one. A second library that set its own would
CHECK-fail, rather than silently replace the profiler's; chaining is left until
one needs to.

### Timers across fiber switches (gb/profile)

- `Profiler::Timing` gains the Profiler it is timed in and its point's index,
  which move out of `ProfileTimer` and `ProfileFrame`, so a switch can move a
  timing to another Profiler. A timer ends its timing in that Profiler, or not
  at all if it has none. A timer has the same fields as before, in a different
  place, and `EndTiming()` is unchanged, so a timed point costs the same.
- `SuspendTimings()`: with no Profiler on the thread, or no timing open,
  returns null. Otherwise it CHECKs that the Profiler has no frame in
  progress, replaces each open timing's start with its ticks so far, notes the
  suspend ticks in the innermost (CL3), empties the thread's stack, and returns
  the innermost. On a joined Profiler, it adds the outermost timing's run on
  this thread to the thread's busy time. The outermost keeps the ticks it has
  run before (`prior_run_ticks`), which its end leaves out of the busy time it
  adds, so each thread is charged for its own part. Each timing keeps its Profiler, so a timer ended on another fiber
  meanwhile fails `EndTiming()`'s CHECK rather than being ignored.
- `ResumeTimings()`: with null, does nothing. With no Profiler on the thread,
  it clears each timing's Profiler, so they record nothing. Otherwise it adds
  the wait to the innermost timing's point (CL3), turns each timing's ticks so
  far back into a start, sets its Profiler, and makes them the thread's stack.
- Each switch takes the thread's whole stack. The stack only ever holds the
  running fiber's timings, since every switch empties it and a fiber starts
  with none, which `ResumeTimings()` CHECKs.
- A point's run time is measured in each thread's own ticks, so it doesn't
  depend on the threads' counters agreeing. Only its wait does.
- Each Profiler's constructor sets the callbacks, which only the first does
  anything, as setting the same ones again does nothing. No timing is open
  before the first Profiler, so no switch with one is missed.

**Brittleness:** a timer must still end on the fiber it started on, in order,
which the existing CHECK in `EndTiming()` catches. The stack holding only the
running fiber's timings depends on every switch going through
`SwitchToFiber()`, as the fiber API requires.

### Wait time (gb/profile)

- `Slot` gains `wait_ticks` and `frame_wait_ticks`, after the value fields, so
  the fields a timed point writes stay together. Only a resume writes them.
- `PointTotals` and `FrameBreakdown::Point` gain `wait_ticks`, gathered,
  folded, and kept for destroyed Profilers as self ticks are.
- A point that has waited, but not yet ended, counts as recorded.

### To confirm

- What a switch costs, with no callbacks, with the profiler's and no open
  timings, and with three open timings (CL1, CL2).
  - CL1, in RelWithDebInfo, the best of five rounds of a million round trips
    between two fibers on one thread: 51.6ns a switch with no callbacks set,
    and 52.0ns with callbacks that do nothing.
  - CL2, the same way, averaged over the two switches of a round trip: 51ns
    with no callbacks set, 53.5ns with the profiler's and no open timings, and
    63.25ns with three open on one of the two fibers.
- That a timed point's cost is unchanged (CL2): in RelWithDebInfo, a scope in
  a loop costs 14.75ns before and after. `GetPointCost()` went from
  11.75-12.25ns to 12.25-12.75ns, as its measurement now stores the Profiler
  and index, which a timer stored outside `StartTiming()` before.
- That a timing's fields are read fresh after a switch in Release, since its
  address is on the thread's stack: CL2's tests that move a fiber between
  threads pass in RelWithDebInfo.

## CLs

### CL1 [x] thread: Fiber switch callbacks

Depends on: nothing.

- `fiber.h`: `FiberSwitchCallbacks` and `SetFiberSwitchCallbacks()`.
- `win_fiber.cc`: the callbacks, called in `SwitchToFiber()`.
- `gen_fiber.cc`: ignores them.
- Unused, so no visible change.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: each switch calls `on_suspend` on the fiber switching away and
  `on_resume` on it when it resumes, with what `on_suspend` returned,
  including after resuming on another thread; a failed switch calls neither;
  with none set, nothing is called.
- Performance: a switch's cost with and without callbacks.

### CL2 [x] profile: Timers across fiber switches

Depends on: CL1.

- `profiler.h`/`.cc`: `Timing` holds its Profiler and index, the switch
  callbacks, set by the constructor, and the frame CHECK.
- `profile_timer.h`: timers end in their timing's Profiler, and the rules in
  the comments.
- `profiler_module.md`: the fiber rules in Behavior, and the limitation.

**Verify**
- Standard checks.
- Unit tests: a scope spanning a switch to another fiber that times its own
  points on the same thread, with each point's self time; a scope and a scope
  inside it spanning a move to a thread with its own Profiler, recorded there,
  with run time only; resuming on a thread with no Profiler records nothing;
  a timer started with no Profiler stays ignored after moving to a thread with
  one; a scope that moves between two joined threads during a frame adds each
  thread's part to its busy time, by its class of core; switching with a frame
  in progress dies; ending timers out of order still dies.
- Performance: a timed point, and a switch with the profiler's callbacks, with
  no open timings and with three.

### CL3 [ ] profile: Wait time

Depends on: CL2.

- `profiler.h`/`.cc`: wait ticks in slots, totals, and breakdowns, recorded on
  resume; the report's Wait columns; `GetWaitTime()`.

**Verify**
- Standard checks.
- Unit tests: the innermost point's wait across a switch, on the same thread
  and on another; an outer point's wait stays zero; a joined Profiler's wait in
  the main Profiler's totals and frame breakdown, including once destroyed;
  report text with and without waits.

### CL4 [ ] job: Timed points across job waits

Depends on: CL3.

- `fiber_job_system_test.cc`: with one job thread, a job with a Profiler times
  a scope around `Wait()` on another job, which times its own scope. The
  scope's self time leaves out the other job's run, and the other job's run is
  its wait. Test only, with `gb_profile` as a test dependency.

**Verify**
- Standard checks.
- The test above, which goes through `FiberJobSystem`'s real wait, rather than
  a hand-made switch.
