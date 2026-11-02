# Profiled jobs

`FiberJobSystem` profiles the jobs it runs, so a program can see whether its job
threads are kept busy and where their time goes, without timing every job
itself. A program switches it on when it creates the job system, which then owns
a threadless main `Profiler` (no thread's own), and gives each job thread a
Profiler joined to it. It times every job as a point named after it, and records
how long jobs wait to start and to resume, and how long each job thread sits
idle. The main loop can itself be a job: its frames are timed on whichever job
thread it runs on, and a frame can wait on other jobs, ending on another thread.
The report has a jobs table, and each job thread's idle time, beside what it
already shows.

Requested by Terra Diem *Profile the world's jobs*.

## Behavior

All of this is new API. Nothing existing changes for its callers, so OZ-3,
jprsurf, and Terra Diem build as they are. Only Terra Diem uses `gb_job`.

**Profiling the jobs.** `FiberJobSystem` takes two new optional constraints:
- `kConstraintProfileJobs`, a `bool` (false by default), which switches it on.
- `kConstraintProfilerOptions`, an owned `Profiler::Options` for the main
  Profiler (its budget, `on_slow_frame`, and a `FakeProfilerCpu` for tests).
  Create fails if it sets `main`.

With profiling on:
- The job system makes a threadless main Profiler before it starts its
  threads, and destroys it after they have stopped.
  `GetProfiler()` returns it, or null with profiling off, for reading the
  report, and for other threads to join (which must be destroyed before the
  job system).
- Each job thread has a Profiler joined to it for as long as the thread runs,
  named "job thread 1" to "job thread N" in placement order.
- Each job is timed as a point of a new kind, `ProfilePoint::Kind::kJob`, named
  after the job, or "unnamed job" (see Job names below). A job's timer
  encloses everything the job does, so each job thread's busy time in the
  threads table is the time it spent running jobs, whichever jobs they were. A
  job that waits and resumes on another thread has the part it ran on each
  thread counted in that thread's busy time, as *Fiber-aware profiling* already
  splits it.
- Job code times frames with `ProfileFrame` as any code does (see Frames below).
- The report gets a jobs table, a row for each job point: its count, its run
  time (including the points inside it, and leaving out time suspended) in
  total, on average, and at p50, p90, p99, and max, and its average and
  longest start delay and resume delay:
  - **Start delay:** from `Run()` to the job starting on a job thread.
  - **Resume delay:** from the counter a job waits on reaching zero to the job
    running again. It only counts waits that suspended.
- The threads table gets each thread's idle time per frame: the time a job
  thread spends waiting for work. What is neither busy nor idle is the job
  system's own time between jobs.

Without `kConstraintProfileJobs`, nothing is recorded, and the job system costs
what it does today.

**A threadless main Profiler.** `Profiler::Options::threadless` is false by
default, as every Profiler is today. A main Profiler made with it set isn't
the Profiler of the thread that made it, or of any other, and records only what
its joined Profilers do. It can be made and destroyed on any thread, and any
thread can read its report.

**Frames.** With a threadless main Profiler, frames are timed on its
joined Profilers' threads, and a frame may span fiber switches, so it can wait
on jobs and end on another thread:
- A frame's time is from its start to its end, including the time its fiber
  was suspended, which is the wait of the innermost point open at the time.
- Only one frame can be in progress (CHECK).
- The frame's breakdown shows the points inside the frame, on its fiber ("In
  frame"), apart from everything else that ran on any thread while it was in
  progress ("Alongside"). The first add up, with their waits, to the frame's
  time, so the kinds table has a wait row in a frame where a point waited.
- The profiler's own cost per frame counts the timed points inside the frame.
- `on_slow_frame` is called on the thread the frame ended on.
- `Reset()` CHECK-fails only inside a frame, as a threadless Profiler has no
  timings of its own.

Any other main Profiler works as today, and its frames still can't span a fiber
switch. A joined Profiler of one still can't time frames.

**Busy time.** A thread's busy time is the time its outermost timed point ran
on it during frames, which for a job thread is the job it is running. A point
still running as a frame ends counts in that frame up to its end. So a long
job, such as a main loop run as a job, counts in each frame it runs in, rather
than all of it in the frame it ends in. Each run counts on the class of core it
started or resumed on (it was the class it ended or suspended on).

**Job names.** Jobs are named as today, by the `std::string_view` passed to
`Run()`. With profiling on, each distinct name registers a job point the first
time it is run, so a program's job names must come from a fixed set: points are
limited to `kMaxProfilePoints` (1024), and registering more CHECK-fails. A
name built at runtime for each piece of work, such as one per chunk, would
reach it.

**Mutex contention.** Contended `absl::Mutex` waits are recorded as the wait of
a point, "absl::Mutex", on the thread that unlocked the mutex (see Design).

**Tests.** A joined Profiler with no `fake_cpu` uses its main Profiler's, and
`FakeProfilerCpu` can be read and advanced from any thread, so a job system test
gives only the main Profiler a fake.

## Design

### Names

- `ProfilePoint::Kind::kJob`: "job" is already the job system's word for what
  it runs, and is in no profile name yet.
- `Options::threadless`: says what it lacks, and is false by default, as a
  `bool` option reads best. `own_thread` was the first name, but it owns no
  thread, and `collect_only` reads as if nothing else differs.
- `kConstraintProfileJobs` / `kKeyProfileJobs` ("profile_jobs"), matching
  `kKeySetFiberNames`, and `kConstraintProfilerOptions` /
  `kKeyProfilerOptions` ("profiler_options").
- Start delay, resume delay: "wait" is taken, by a timed point's time
  suspended (see *Fiber-aware profiling*), and "latency" is jargon here. A
  delay is time before something runs.
- Idle time: the opposite of the threads table's busy time.

### Threadless Profiler (gb_profile)

```
struct Options {
  ...
  // Makes this a main Profiler that is no thread's Profiler, which records
  // only its joined Profilers' points. Its frames are timed on their threads
  // (see Frames).
  bool threadless = false;
};
```

- With `threadless` set, the Profiler doesn't set `s_current`, so it can be
  destroyed on any thread, and has no slots of its own to gather.
- Frame state (the frame number, start core, frame count and time, histogram,
  and slowest frame) is written under a mutex, taken as each frame starts and
  ends by the thread starting or ending it. So the report can be read from any
  thread, and a second frame starting while one is in progress reliably
  CHECK-fails. The report formats a copy, so it holds the mutex only to copy
  it. The two locks add about 20 ns to a frame (an empty frame went from 35 ns
  to 56 ns, or 61 ns on a threadless main, in Release), and nothing to a timed
  point (13.9 ns before and after).
- A joined Profiler of a threadless main Profiler times frames, recording them
  in the main Profiler. Whether its thread is in a frame is its own flag, which
  the fiber switch callback CHECKs, as the frame number is the main Profiler's.
- The breakdown's split is the frame's own points against the rest, with the
  columns "In frame" and "Alongside" (CL2 had "Frame thread" and "Other
  threads", and before it "Main thread"). For a main Profiler with a thread,
  that is the same split as by thread, as its frames can't span a switch.
- Each timing records whether it is inside a frame, from its parent, so its
  self time and wait can be split from the rest in the breakdown, by slot
  fields for the part outside the frame. Each also counts the timed points
  that end inside it, so a frame's own cost is its timing's count, wherever
  they ended. Neither needs any per-thread frame state.
- A frame's time is from its start ticks, kept in the main Profiler under the
  frame mutex, to its end ticks, which are its timing's start ticks (moved on
  by each resume) plus its elapsed ticks.
- A joined Profiler publishes its outermost point's run in progress (its start
  ticks and class of core) beside the runs that have ended, so the main
  Profiler can read its busy time up to any moment without stopping it. The
  run is ended before it is added, and read in the other order, so a read
  racing with a run's end leaves it out, which a later read includes, but
  never counts it twice. The main Profiler gathers it as each frame starts,
  dropping what was before the frame, and as it ends, counting the frame's.
- Performance, in Release, against CL2: a timed point inside a frame goes from
  15.4 to 16.7 ns, for the in-frame flag and point count. An outermost point
  on a joined thread goes from 20.4 to 21.8 ns during a frame, but outside any
  frame from 15.8 to 21.6 ns, as it now always reads the class of core. An
  empty frame goes from 58 to 66 ns (63 to 75 ns threadless), for gathering
  busy time as it starts.

**Brittleness:** what a frame can do depends on whether its main Profiler has a
thread, which is set once, at construction, and CHECKed where it matters.

### Profiler for jobs (gb_profile)

```
// In ProfilePoint::Kind:
kJob,  // A job run by a job system (see gb/job).

// Times a job, from when it starts to when it ends, enclosing every point the
// job reaches. `queued_ticks` is ReadTicks() from when the job was queued.
class ProfileJobTimer final {
 public:
  ProfileJobTimer(const ProfilePoint& job, int64_t queued_ticks);
  ~ProfileJobTimer();
};

// Times the calling thread waiting for work, as its idle time.
class ProfileIdleTimer final { ... };

class Profiler {
  ...
  //--------------------------------------------------------------------------
  // For job systems
  //--------------------------------------------------------------------------

  // Returns the ticks now, which any thread may call, for the delays below.
  int64_t ReadTicks() const;

  // Adds a delay to the innermost job on the calling thread's fiber, from
  // `ready_ticks` (ReadTicks() when the counter it waits on reached zero).
  static void AddResumeDelay(int64_t ready_ticks);
};
```

- A job point is timed as a scope is, with the same self time and wait, and
  also records its run time in a histogram, and its delays. Histograms use the
  frame histogram's buckets (within 1/16th), about 4 KB each, so only job
  points have them, made the first time each thread ends a job of that point.
  The main Profiler reads them without stopping the other threads, as it reads
  slots, and folds and baselines them as it does slots.
- Idle time is recorded by the joined Profiler, and gathered by the main
  Profiler at each frame's end with busy time. A wait still in progress at a
  frame's end counts in that frame up to its end, so a thread asleep for many
  frames shows idle in each, rather than all of it in the frame it wakes in.
- Points are recorded as today: about 15 ns for a timed point. A job adds a
  read of the ticks for its start delay, and a histogram add.

**Brittleness:** the job system must start and end each job's timer on the
right fiber, and read `queued_ticks` from the same tick source. Both are
inside `FiberJobSystem`, so no program has to get them right.

### Profiling in FiberJobSystem (gb_job)

- `gb_job` comes to depend on `gb_profile`, both tier 1, which is the point of
  the feature.
- The job system owns the main Profiler, made in `Init()` before the threads,
  with `threadless` set, and destroyed after the threads are joined.
- Each `JobThread` owns its joined Profiler, made on the thread before it runs
  jobs, and destroyed on the thread as it stops. A fiber can move between
  threads, so the Profiler can't be a local on the thread's first fiber.
- `Job` gets its job point, found in `DoRun()` from its name. Each thread
  keeps the points it has found by name, in a thread-local hash map, so only a
  name's first run on a thread goes to the point registry and its mutex. After
  that, finding it costs hashing the name, about 15 to 20 ns, with no lock.
- `Job` gets `queued_ticks`, read in `DoRun()` from the main Profiler.
- `FiberState` gets `ready_ticks`, read in `EnqueueFibers()` when the counter
  reaches zero, and the job adds its resume delay when `DoWait()` returns.
- `WaitForWork()` times its wait with a `ProfileIdleTimer`.

**Brittleness:** none for the program, as the job system owns every Profiler
and destroys them in order. A thread outside the job system that joins
`GetProfiler()` must be destroyed first, as with any main Profiler, which
CHECK-fails otherwise.

### Mutex contention (gb_profile)

Abseil reports contended `absl::Mutex` waits through
`absl::RegisterMutexProfiler()`, which a program can register once. The first
Profiler registers it, as it sets the fiber switch callbacks. Each report is
recorded as the wait of a point, "absl::Mutex", on the thread it is reported
on, counting the contended unlocks.

The hook is called by the thread that unlocks the mutex, with the total that
the threads it wakes waited, not on the threads that waited. So the time is
charged to the thread holding the lock, which shows where contended locks are
released, but not who waited for them.

The wait is in Abseil's cycle clock, which is the timestamp counter shifted
right by 2 in Debug and 1 in Release, and is converted by
`absl::base_internal::CycleClock::Frequency()`, an Abseil internal. A unit test
checks the conversion against the profiler's own tick rate, so an Abseil update
that changes it fails a test.

### To confirm

- Where Abseil calls the mutex profiler hook, and in what units: confirmed
  above, from `absl/synchronization/mutex.cc` (`Mutex::UnlockSlow()`).
- That a frame's state is ordered across the threads it starts and ends on:
  confirmed, as CL2 writes it under the frame mutex at both.
- What a job's histogram costs in memory with Terra Diem's job names and
  thread count: CL4 measures it.

## CLs

### CL1 [x] profile: job points, and a shared fake CPU

Depends on: nothing.

- `ProfilePoint::Kind::kJob`, timed by a `ProfileTimer` as a scope is, and
  shown in the points table as "job". `ProfileJobTimer` comes with CL4, once
  a job's timer has delays and a histogram to record.
- `FakeProfilerCpu` can be used from any thread, and a joined Profiler with no
  `fake_cpu` uses its main Profiler's.
- Unused by gb_job, so no visible change there.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: a job point is timed and reported; a joined Profiler without a
  fake reads its main Profiler's.

### CL2 [x] profile: a threadless main Profiler

Depends on: CL1.

- `Options::threadless`, frames on its joined Profilers' threads, one at a
  time, and frame state under a mutex at frame start and end.
- Frames still can't span a fiber switch.

**Verify**
- Standard checks.
- Unit tests: a threadless main's report of two joined threads; frames on
  either; a second frame in progress CHECK-fails; `Reset()` between frames;
  destroying it on another thread.
- Performance: a timed point and a frame, against today.

### CL3 [x] profile: frames across fiber switches, and busy time per frame

Depends on: CL2.

- A threadless main's frame may span fiber switches, timed from start to end,
  with the breakdown split into the frame's own points and the rest, and a
  wait row.
- An outermost point's run so far is added to busy time at each frame's end,
  and busy time before a frame is dropped as it starts.

**Verify**
- Standard checks.
- Unit tests: a frame that suspends and ends on another thread; its breakdown
  adds up with its waits; a long outermost point's busy time in each frame.
- Performance: a timed point, for the in-frame flag.

### CL4 [ ] profile: job run times and delays

Depends on: CL1.

- Run-time histograms, start and resume delays for job points, and the jobs
  table in the report. `Profiler::ReadTicks()` and `AddResumeDelay()`.
- Folding, baselines, and `Reset()` cover them.

**Verify**
- Standard checks.
- Unit tests: percentiles, delays, a joined thread's jobs in its main
  Profiler's report and after it is destroyed, and `Reset()`.
- Memory: a histogram's size, and the total for Terra Diem's job names on 32
  threads.

### CL5 [ ] profile: idle time per thread

Depends on: CL1.

- `ProfileIdleTimer`, and idle time in the threads table, gathered at each
  frame's end, including a wait in progress.

**Verify**
- Standard checks.
- Unit tests: idle in one frame, a wait spanning several frames, and a
  thread destroyed while idle.

### CL6 [ ] job: profile the jobs

Depends on: CL3, CL4, CL5.

- `kConstraintProfileJobs`, `kConstraintProfilerOptions`, and `GetProfiler()`.
- `Create()` returns once every job thread has made its Profiler, so a test
  that sets an auto advance afterwards doesn't have it applied to their
  construction.
- The threadless main Profiler, a joined Profiler per job thread, each job
  timed as its point, found by name, with its delays, and each thread's wait
  for work as idle time.

**Verify**
- Standard checks.
- Unit tests, with a fake CPU in the options: a job's count, run time, and
  start delay; a waiting job's resume delay; idle time on a job thread; a
  frame in a job that waits on another job and ends on another thread; two
  names and an unnamed job as their own points; nothing recorded without the
  constraint; Create fails with `main` set.
- Performance: a job's cost with and without profiling, including finding its
  point.

### CL7 [ ] profile: mutex contention

Depends on: CL1.

- The first Profiler registers Abseil's mutex profiler hook, recording each
  report as the wait of "absl::Mutex" on the reporting thread.

**Verify**
- Standard checks.
- Unit tests: the hook's recording, given a wait in cycles; the cycle
  conversion against the profiler's tick rate.
