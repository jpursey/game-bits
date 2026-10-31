# Multithreaded profiles

Combine the profiles of several threads, each with its own `Profiler`, into the
report of the Profiler on the thread that runs the main loop. A program makes
that Profiler as it does today, and gives each of its other threads a Profiler
that is joined to it. The main Profiler's report then shows every point's CPU
time and calls across all threads, a frame's breakdown includes what the other
threads did during it, and a table shows how busy each thread is per frame, and
on which class of core.

Overlap, idle gaps, and critical paths need a timeline of events, which this is
not (see *Profile timelines* in the backlog). The job system giving its job
threads Profilers is *Profiled jobs*; this feature is what they join.

Terra Diem is the main user: its world loads, lights, meshes, and simulates as
thousands of short jobs on a `FiberJobSystem`. Its job threads are pinned, so
each runs on one class of core, but they spread across performance and
efficiency cores, which on an i9-14900KF run the same frame 25-30% apart. The
single-thread report already splits frames by class of core; the combined
report needs the same per thread.

## Behavior

```
// The main loop's thread, as today:
gb::Profiler profiler({.slow_frame = absl::Milliseconds(20),
                       .on_slow_frame = ...});

// Each other thread, for as long as it runs:
gb::Profiler thread_profiler({.main = &profiler, .name = "loader"});
```

- **Joined Profilers:** a Profiler made with `Options::main` records its own
  thread's points as any Profiler does, and adds them to the main Profiler's
  report. The main Profiler must outlive it (CHECK when the main Profiler is
  destroyed), and it can't time frames (CHECK): its points count in the main
  Profiler's frames. A joined Profiler can be made and destroyed while the main
  loop runs, on any thread. When it is destroyed, what it recorded stays in the
  main Profiler's report.
- **Totals:** every point's count and self time in the main Profiler's report
  is the sum over the main thread and every joined Profiler, including those
  already destroyed. A counter is the sum of its counts on every thread. A
  value is the one set most recently on any thread.
- **Frames:** a point on another thread counts in the main Profiler's frame
  that is in progress when the point ends, by the shared timestamp counter, and
  in no frame if it ends between frames, as on the main thread. A frame's
  breakdown (the slowest frame, and `on_slow_frame`) includes those points.
  The frame's time is still the main thread's wall time; the self times by kind
  now add up to the frame's time only for the main thread, and the breakdown
  shows the other threads' CPU time beside them.
- **Share of the frame:** the points table gives each point's self time per
  frame as a share of the average frame, alongside its total. On many threads
  the shares add up to more than 100%: a frame is wall time, and the threads'
  points are CPU time, so they can't be summed with it.
- **Threads:** once a Profiler has joined, a table with a row for each joined
  Profiler, and one for the main thread: its name, its busy time (the time in
  its outermost timed points) per frame on average and at most, its average as
  a share of the average frame, and the class of core it was busy on, or
  "mixed" with the time on each. Rows are sorted by name, so reports diff
  cleanly.
- **Reading:** only the main Profiler's thread reads its report, as today. The
  main Profiler reads the joined Profilers' slots while their threads write
  them, with no lock on their side, so a point ending at the same moment can be
  missing from a report or a frame breakdown, and appear in the next. Nothing
  is lost or counted twice in the totals.
- **For tests:** `GetCount()`, `GetSelfTime()`, `GetValue()`, and the slowest
  frame readers on the main Profiler read the combined profile. On a joined
  Profiler, they read its own thread, and `Reset()` CHECK-fails. `Reset()` on
  the main Profiler clears what every thread recorded, too. Each thread's
  Profiler takes its own `FakeProfilerCpu`, which must have the main
  Profiler's tick rate (CHECK).

No existing API changes. A Profiler with no other Profilers joined reports
what it does today, plus the share-of-frame column.

## Design

All of it is in `gb/profile`, as `Profiler` changes. Nothing in `gb/thread` or
`gb/job` changes; *Profiled jobs* uses `Options::main` from `FiberJobSystem`.

### Names

- **`Options::main`**, the Profiler another one joins. Candidates:
  `main` (matches "the main loop", which the docs already use for the thread
  that runs frames; collides only with `main()`, which a member can't be
  confused with), `frames_from` (says what it takes from it, but reads oddly
  for totals), and `combine_into` (says what happens, but not which Profiler
  it should be). Recommended: `main`.
- **Joined Profiler** and **main Profiler**, in docs and comments, for the two
  roles. No type names: both are `Profiler`. "Joined" matches what the code
  does (`AddJoinedThread()`, `joined_threads_`); "thread Profiler", used at first,
  didn't say which role, as every Profiler is on a thread.
- **`Options::name`**, the thread's name in the threads table. When empty, it
  defaults to the thread's own name (`GetThreadName()`), or "thread N" for a
  thread with none, with the lowest N no existing joined Profiler has, and
  "main" for the main Profiler. The thread's name is taken when the Profiler is made, as
  `kKeySetFiberNames` renames a job thread after each job it runs, so
  *Profiled jobs* should make each thread's Profiler before it runs a job.
- **Busy time**, the time a thread spends inside its outermost timed points,
  against the frame's wall time. Avoids "load" and "utilization", which suggest
  a measure of the CPU rather than of the points.

### Slots readable from another thread

```
struct Slot {
  // Written only by the Profiler's thread, and read by the main Profiler.
  std::atomic<int64_t> count;
  std::atomic<int64_t> self_ticks;
  std::atomic<int64_t> value;
  std::atomic<int64_t> value_ticks;  // When `value` was set.
  std::atomic<int64_t> frame;
  std::atomic<int64_t> frame_count;
  std::atomic<int64_t> frame_self_ticks;
};
```

- Every Profiler's slots become relaxed atomics, each written only by its own
  thread as a load and a store (not a read-modify-write), which on x86-64 are
  the same plain moves as today. The main Profiler's own slots don't need it,
  but one Slot type keeps one code path.
- `SetValue()` also records the ticks it was set at, so the main Profiler can
  find the value set most recently on any thread. That adds a counter read
  (about 5ns) to setting a value, which is done when the value changes.
- The frame a point is tagged with comes from `frame_`, a pointer to the frame
  number in progress: the Profiler's own, or the main Profiler's for a joined
  Profiler. The main Profiler's frame number is a relaxed atomic on its own
  cache line, written twice a frame, so the threads reading it on every point
  don't share a line with the main thread's timing stack, which it writes on
  every point.

### Joining a main Profiler

```
// In the main Profiler, under a mutex. One entry per name: at most one
// joined Profiler that exists, and every one that was destroyed.
struct JoinedThread {
  Profiler* profiler;   // The one that exists, or null.
  std::string name;
  // While `profiler` exists, its totals when the main Profiler was last
  // reset, by point.
  std::vector<PointTotals> baseline;
  // What the destroyed ones recorded since the last reset, by point.
  std::vector<PointTotals> folded;
  // Busy time per frame, gathered by the main Profiler at each frame's end.
  BusyFrames busy;
};
std::vector<JoinedThread> joined_threads_ ABSL_GUARDED_BY(joined_mutex_);
```

- A joined Profiler joins in its constructor and leaves in its destructor, so
  there is no call to pair. Leaving folds its totals into its entry, which
  stays, so its row and its points stay in the report.
- A Profiler that joins with the name of an entry whose Profilers were all
  destroyed takes it over, so the entries grow with the names in use, not with
  every thread made. A program that keeps making threads under the same names
  (a pool that recreates its threads) holds a fixed number. Threads that share
  a name share a row, which suits a pool, but merges unrelated threads that
  happen to share one. Two that exist at once with the same name each have an
  entry.
- The report gathers totals from the main thread's slots, and from each
  entry's folded totals, plus its live slots less its baseline, under the
  mutex, then formats them. `GetReport()` is split into gathering a
  snapshot of totals by point, and formatting it, which a single Profiler then
  does the same way.
- A frame's end locks the mutex once, to read each joined thread's busy time in
  that frame (two loads per thread), and, only when the frame is the slowest or
  slow, each joined thread's slots tagged with that frame. That is about 20ns
  for the lock, and a few nanoseconds per thread, per frame.

### Busy time and classes of core

- When a joined Profiler's outermost timing ends, it adds the elapsed time to
  its busy time, in total, in the frame part (tagged as slots are), and by the
  class of core it ends on. Reading the class costs about 2.4ns, once per
  outermost point, so a job (one outermost point, once *Profiled jobs* times
  them) pays it once.
- The main thread's busy time is its frames' time, by the class each frame ran
  on, which it records already.

**Brittleness:**
- The main Profiler must outlive its joined Profilers. Its destructor CHECKs
  that none are left, which catches the order being wrong on the first run,
  but only at the end. *Profiled jobs* destroys its Profilers with the job
  system, so a program destroys the job system before the main Profiler.
- A joined Profiler's points before it joins aren't in the report, which can't
  happen, as it joins in its constructor.
- The main Profiler reads the threads' slots without their threads' help, so
  nothing on a thread has to be called at a safe point.

### To confirm

- The relaxed atomic slots and the frame pointer leave the cost of a timed
  point unchanged (12.25ns as the Profiler measures it, 15.0ns for a
  `ProfileScope` in a loop). Confirmed in CL2: 11.75ns and 14.8ns, against
  11.75ns and 15.1ns before. That needs Release's inlining (`/Ob2`): with
  CMake's `/Ob1`, MSVC calls two STL helpers in every `std::atomic` load and
  store, and a point costs 20ns.
- The timestamp counter agrees across cores to well under a microsecond, so a
  point on another thread is put in the right frame. Windows keeps an
  invariant counter in step across processors; CL3 checks it by hand, with two
  threads pinned to a performance and an efficiency core. Confirmed in CL3 on
  an i9-14900KF: two such threads passing a reading of the counter back and
  forth saw it advance by at least 20-70ns each way, so the cores differ by
  less than that.
- What a frame's end costs with 24 joined threads, and what capturing a slow
  frame costs with 24 threads and a few hundred points. CL4 and CL5 measure
  them. Capturing confirmed in CL4, with every thread in all of 300 points in
  the frame: a frame that is the slowest so far takes 12us to capture, against
  0.5us with no threads. A slow frame's report takes 130-185us with or without
  them, nearly all of it formatting. A frame that captures nothing is
  unchanged.
- Whether `GetThisThreadCoreClass()` on every outermost point is cheap enough
  for Terra Diem's shortest jobs. CL5 measures it against a job of a few
  microseconds.

## CLs

### CL1 [x] profile: report from a snapshot of totals

Depends on: nothing.

- `Profiler::GetReport()` gathers a snapshot of each point's totals, then
  formats it, rather than reading the slots as it goes.
- The points table gains the share-of-frame column.
- No other change in behavior.

**Verify**
- Standard checks (see CLAUDE.md).
- Existing report tests pass unchanged, apart from the new column.

### CL2 [x] profile: slots readable from other threads

Depends on: CL1.

- `Slot` fields become relaxed atomics, and `SetValue()` records when it was
  set.
- Points tag their frame part through `frame_`, and the main Profiler's frame
  number moves to its own cache line.
- Unused, so no visible change.

**Verify**
- Standard checks.
- Performance: a timed point's cost, as the Profiler measures it and in a
  loop, before and after, in RelWithDebInfo.

### CL3 [x] profile: joined Profilers

Depends on: CL2.

- `Options::main` and `Options::name`; joining and leaving; the CHECKs on
  lifetime, frames, `Reset()`, and the tick rate.
- The main Profiler's totals, counters, values, and test readers combine
  every thread.

**Verify**
- Standard checks.
- Unit tests, with threads that time points and count while the main thread
  waits on notifications, so no test waits on time: totals sum across threads;
  a value is the latest set on any thread; a destroyed thread's points stay;
  `Reset()` on the main Profiler clears every thread; each CHECK.
- By hand: two threads pinned to a performance and an efficiency core read
  the timestamp counter in turn, to check they agree.

### CL4 [x] profile: other threads' points in frame breakdowns

Depends on: CL3.

- A frame's end captures each joined thread's slots tagged with that frame,
  when the frame is the slowest or slow.
- The breakdown shows the other threads' CPU time beside the main thread's
  self time by kind.
- Frame numbers keep counting up across `Reset()`, rather than starting again
  from 1, as a joined Profiler's slots aren't cleared by it, and can still be
  tagged with a frame number from before the reset.
- A joined Profiler destroyed during a frame leaves its part in that frame
  with the main Profiler, so the frame's breakdown still has it.
- A slot's frame number is written after its frame part is reset, with
  release and acquire, so the main Profiler never reads an old frame's part
  under the new frame's number.

**Verify**
- Standard checks.
- Unit tests: a thread's point that ends during a frame is in its breakdown
  and `on_slow_frame`'s report; one that ends between frames is in neither.
- Performance: capturing a slow frame with 24 joined threads and 300 points.

### CL5 [ ] profile: busy time per thread

Depends on: CL4.

- Joined Profilers record busy time, in total, per frame, and by class of
  core; a frame's end gathers each thread's busy time in it.
- The report's threads table.

**Verify**
- Standard checks.
- Unit tests, with a `FakeProfilerCpu` per thread: busy time per frame on
  average and at most, share of the frame, and class of core, including a
  thread that is busy on two classes.
- Performance: a frame's end with 24 joined threads; an outermost point on a
  joined Profiler, against a job of a few microseconds.
