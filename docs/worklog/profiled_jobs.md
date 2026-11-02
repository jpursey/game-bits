# Profiled jobs

`FiberJobSystem` can profile the jobs it runs, so a program can see whether its
job threads are kept busy and where their time goes, without timing every job
itself. With profiling on, the job system owns a threadless main `Profiler`
(one that is no thread's own), and gives each job thread a Profiler joined to
it. It times every job as a point named after the job, records how long jobs
wait to start and to resume, and records how long each job thread sits idle.
The main loop can itself be a job: its frames are timed on whichever job thread
it runs on, and a frame can wait on other jobs and end on another thread.
Contended `absl::Mutex` locks are recorded on any thread with a Profiler.

It adds to `gb_profile` (job points, a threadless main Profiler, frames across
fiber switches, busy time per frame, idle time, and mutex contention), and
`gb_job` comes to depend on `gb_profile` to use them.

Requested by Terra Diem *Profile the world's jobs*. It builds on *Multithreaded
profiles* and *Fiber-aware profiling*.

## Using it

```
auto job_system = gb::FiberJobSystem::Create(
    gb::ContextBuilder()
        .SetValue<bool>(gb::FiberJobSystem::kKeyProfileJobs, true)
        .SetValue<gb::Profiler::Options>(
            gb::FiberJobSystem::kKeyProfilerOptions,
            gb::Profiler::Options{.slow_frame = absl::Milliseconds(20),
                                  .on_slow_frame = LogSlowFrame})
        .Build());

// The main loop, run as a job:
job_system->Run("MainLoop", [] {
  while (running) {
    gb::ProfileFrame<"Frame"> frame;
    ...
    gb::JobSystem::Wait(&counter);  // The frame waits, and may end elsewhere.
  }
});

// From any thread:
LOG(INFO) << job_system->GetProfiler()->GetReport();
```

## Behavior

All of this is new API. No public API broke, so OZ-3, jprsurf, and Terra Diem
build as they are. Reports change only once something new is recorded: a
contended mutex, for any program with a Profiler, and the rest only for a
profiled job system.

**Profiling the jobs** (`gb_job`). `FiberJobSystem` takes two optional
constraints:
- `kConstraintProfileJobs`, a `bool` (false by default), switches it on.
- `kConstraintProfilerOptions`, a `Profiler::Options` for the main Profiler
  (its budget, `on_slow_frame`, and a `FakeProfilerCpu` for tests, which the
  job threads' Profilers share). `Create()` moves the options out of the
  context, as `on_slow_frame` can't be copied, sets `threadless`, and fails if
  they set `main`.

With profiling on:
- `GetProfiler()` returns the main Profiler (null with profiling off), for any
  thread to read, or for another thread to join, if that thread's Profiler is
  destroyed before the job system.
- Each job thread has a joined Profiler while it runs, named "job thread 1" to
  "job thread N" in placement order. `Create()` returns once every one is made.
  Jobs can't make Profilers of their own, as a thread has at most one.
- Each job is timed as a job point named after it, or "unnamed job", around its
  callback. Each distinct name registers a point the first time it runs, so
  job names must come from a fixed set: points are limited to
  `kMaxProfilePoints` (1024), and registering more CHECK-fails, as does a job
  name that names another kind of point.
- A job's start delay is from `Run()` to its start. Its resume delay is from the
  counter it waits on reaching zero to its running again, counted only for
  waits that suspended.
- Each job thread's blocking wait for work is its idle time.
- Without the constraint, nothing is recorded, and no point is registered.

**Job points** (`gb_profile`). `ProfilePoint::Kind::kJob` points are timed by
`ProfileJobTimer`, which a job system puts around each job, with
`Profiler::ReadTicks()` from when the job was queued. A job point is timed as a
scope is, with the same self time and wait, and also records each run's time
in a histogram (within 1/16th, as frames are), and its delays.
`Profiler::AddResumeDelay()` adds a resume delay to the innermost job open on
the calling fiber. The report gets a jobs table, a row for each job point that
has run or resumed: Count, Run, Run/call, P50, P90, P99, Max, Start delay, and
Max start, and once any job has resumed, Resumes, Resume delay, and Max resume.
`GetJobSummary()` reads one row's numbers. A run's time includes the points
inside it and leaves out the time it was suspended. `ProfileTimer` no longer
takes job points, so every job's run is recorded.

**A threadless main Profiler.** `Profiler::Options::threadless` makes a main
Profiler that is no thread's Profiler, and records only what its joined
Profilers do. It can be made and destroyed on any thread, any thread can read
its report, and `Reset()` CHECK-fails only inside a frame. Any other main
Profiler works as before.

**Frames on a threadless main Profiler.** Frames are timed on its joined
Profilers' threads, one at a time (CHECK), and a frame can span fiber switches:
- A frame's time is from its start to its end, including the time its fiber
  was suspended, which is the wait of the innermost point open at the time. Its
  fiber must only resume on threads joined to the same main Profiler (CHECK).
- A frame's breakdown splits its time by kind into the frame's own points, on
  its fiber ("In frame"), and everything else that ran on any thread while it
  was in progress ("Alongside"). The frame's own add up, with their waits, to
  the frame's time, so the kinds table has a wait row in a frame where a point
  in it waited.
- The profiler's own cost per frame counts the timed points inside the frame.
- `on_slow_frame` is called on the thread the frame ended on.

A main Profiler with a thread still times frames only on its own thread, which
can't span a fiber switch, and its joined Profilers can't time frames.

**Busy time** (the threads table). A joined thread's busy time is the time its
outermost timed points ran on it during frames, which for a job thread is the
jobs it ran. A point still running as a frame ends counts in that frame up to
its end, and the rest in the frames after, so a long job, such as a main loop,
counts in each frame it runs in. Each run counts on the class of core it
started or resumed on. A main thread's busy time is its frames' time.

**Idle time** (`ProfileIdleTimer`). A joined thread's time waiting for work,
inside a `ProfileIdleTimer`, is its idle time, which the threads table shows
per frame and as a share of the average frame (Idle/frame and Idle share) once
any joined thread has been idle in a frame. It is counted in frames as busy
time is, so a thread asleep for many frames is idle in each. Other threads'
idle time isn't reported. An idle timer must be outside every timed point,
can't nest, span a fiber switch, or outlive its Profiler, and no timed point can
start inside one (CHECK). What is neither busy nor idle is the thread's time
between its timed points, which for a job thread is the job system's own.

**Mutex contention.** Contended `absl::Mutex` locks are recorded as a call
point, "absl::Mutex", whose count is the unlocks that woke a thread waiting to
lock, and whose wait is the time those threads waited:
- Abseil reports them on the thread that unlocks the mutex, so they are
  recorded in its Profiler, and show where contended mutexes are released, but
  not who waited for them.
- Waits for a condition (`absl::CondVar`, or a `Condition`) aren't included, as
  Abseil doesn't report them.
- As other threads waited, the wait is in no frame's own time, even on the
  frame's thread. The kinds table shows a wait row only for a wait in the
  frame, or with an "Alongside" column for one outside it.
- The first Profiler registers Abseil's mutex profiler, so no other library can
  register its own, and a program that registers one first can't make a
  Profiler (Abseil asserts in Debug, and contention isn't recorded in Release).

**Tests.** A joined Profiler with no `fake_cpu` uses its main Profiler's, and
`FakeProfilerCpu` can be read and advanced from any thread, so a job system test
gives only the main Profiler a fake.

## Structure

**Threadless Profiler and frames** (`profiler.h`, `profiler.cc`):
- A threadless Profiler doesn't set `s_current`, and has no slots of its own to
  gather.
- Frame state (the frame number, the frame's start ticks and class of core,
  and the frame totals) is written under `frame_mutex_`, which the thread
  starting or ending a frame takes, so the report can be read from any thread,
  and a second frame starting while one is in progress reliably CHECK-fails.
  The report formats a copy, so it holds the mutex only to copy it.
- A joined Profiler of a threadless main Profiler times frames, recording them
  in the main Profiler (its `frame_profiler_`).
- Each timing records whether it is inside a frame, from its parent, and counts
  the timed points that end inside it. Slots have fields for the part of a
  frame's self time and wait outside it, so the breakdown splits the frame's
  own from the rest, and a frame's own cost is its timing's count, wherever
  they ended. Neither needs any per-thread frame state.
- A frame's time is from its start ticks, kept under the frame mutex, to its
  end ticks, which are its timing's start ticks (moved on by each resume) plus
  its elapsed ticks.

**Thread time** (busy and idle):
- A joined Profiler's `ThreadTime`, on a cache line of its own, has its runs by
  state (busy on each class of core, or idle as `kIdle`) that have ended or
  whose fiber suspended, and the run in progress: its start ticks and state.
  A thread is in at most one run at a time, so it is never busy and idle at
  once.
- A run is ended before it is added, and read in the other order, with release
  and acquire, so the main Profiler can read a thread's time up to any moment
  without stopping it. A read racing with a run's end leaves it out, which a
  later read includes, but never counts it twice.
- The main Profiler gathers each joined thread's time as each frame starts,
  dropping what was before the frame, as it ends, counting the frame's, and as
  a joined Profiler is destroyed, keeping its part in the frame in progress.

**Job stats:**
- A job point's `JobStats` (its histogram, and its runs, delays, and their
  longest) are made the first time each thread ends a job of that point, and
  published through a table of pointers, one per point, that the main Profiler
  reads without stopping the other threads, as it reads slots, and folds as it
  does slots.
- A job's longest run and delays can't be baselined, so `Reset()` counts its
  resets, and each thread's stats hold the reset count they are for. The first
  record after a reset clears them, and readers ignore them until then.

**Mutex contention:**
- `RegisterMutexHook()` registers the "absl::Mutex" point before the hook, as
  registering a point locks the registry's mutex, which the hook can't do from
  inside an unlock. Each Profiler reads the rate of Abseil's cycle clock as it
  is made, as `ticks_per_mutex_cycle_`, so the hook never measures it.
- The rate is `absl::base_internal::CycleClock::Frequency()`, an Abseil
  internal: the clock is the timestamp counter shifted right by 2 in Debug and
  1 in Release. A test checks the conversion against the profiler's tick rate,
  so an Abseil update that changes it fails a test.
- Contention can't be made to order, so the tests call the hook through
  `ProfilerMutexTest`, a fixture `Profiler` names as a friend. One test
  contends a mutex for real, repeating until Abseil reports it, to show the
  hook is registered.

**In `FiberJobSystem`** (`fiber_job_system.h`, `fiber_job_system.cc`):
- The job system owns the main Profiler, made in `Init()` before the threads,
  and destroyed after they are joined.
- Each `JobThread` owns its joined Profiler, made on the thread before it runs
  jobs, and destroyed at the end of `JobMain()`, which returns once per thread,
  as it stops, on whichever fiber the thread is on by then. A fiber can move
  between threads, so the Profiler can't be a local on the thread's first
  fiber.
- `DoRun()` finds the job's point by name through a thread-local hash map, so
  only a name's first run on each thread goes to the point registry and its
  mutex, and reads its `queued_ticks` from the main Profiler.
- `EnqueueFibers()` stamps each waiting fiber's `ready_ticks` as its counter
  reaches zero (and `CompleteWait()` as it resumes one at once), and
  `DoWait()` adds the resume delay once the wait returns.
- `WaitForWork()` times only the part of its wait that blocks, so a job thread
  that finds work at once doesn't read the ticks for it. The idle timer starts
  before the thread marks itself waiting, so a thread `GetIdleThreadCount()`
  counts is already idle.
- A job's timer encloses only its callback, so freeing its data and context,
  and ending its counter, are the job system's own time between jobs.

Reusable pieces, should anything else need them: a threadless main Profiler,
for any set of threads that share frames, `ProfileJobTimer` for another job
system, and `ProfileIdleTimer` for any thread that waits for work.

## Measurements

On the development machine (an i9-14900KF), in RelWithDebInfo builds:

- **A timed point:** 13.9ns, unchanged by the frame mutex. Inside a frame, 15.4
  to 16.7ns, for the in-frame flag and point count. An outermost point on a
  joined thread, 20.4 to 21.8ns during a frame, and 15.8 to 21.6ns outside any
  frame, as it always reads the class of core.
- **An empty frame:** 35ns before, 66ns after (75ns on a threadless main), for
  the frame mutex at its start and end, and gathering busy time as it starts.
- **A job:** a job that runs the next, on one job thread, takes 113ns without
  profiling and 154ns with it. Of the 41ns, finding its point by name is
  15.5ns, and the rest is its timer, its delays, and reading the ticks as it is
  queued.
- **Job stats memory:** a job point's stats are 3,976 bytes per thread that
  runs it, and each Profiler has an 8KB table of pointers to them. Terra Diem's
  six job names (with the unnamed job) on 32 threads come to about 760KB of
  stats, and 256KB of tables.
