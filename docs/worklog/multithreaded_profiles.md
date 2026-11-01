# Multithreaded profiles

The profiles of several threads, each with its own `Profiler`, combined into the
report of the Profiler on the thread that runs the main loop. Each other thread
has a Profiler joined to that one. The main Profiler's report then shows every
point's time and calls across all threads, a frame's breakdown includes what the
other threads did during it, and a table shows how busy each thread is per
frame, and on which class of core.

It is all in `gb/profile`, as changes to `Profiler`, with no new public types.
Points cost what they did, and a joined thread's outermost points about 5ns
more.

## Using it

```
// The main loop's thread, as before:
gb::Profiler profiler({.slow_frame = absl::Milliseconds(20),
                       .on_slow_frame = ...});

// Each other thread, for as long as it runs:
gb::Profiler thread_profiler({.main = &profiler, .name = "loader"});
```

## Behavior

- **Main and joined Profilers:** a Profiler made with `Options::main` (a joined
  Profiler) records its own thread's points as any Profiler does, and adds them
  to the main Profiler's report and test readers. It can be made and destroyed
  at any time, on any thread, and what it recorded stays in the main Profiler's
  report once it is destroyed. CHECKs: the main Profiler outlives its joined
  Profilers (checked when it is destroyed, naming the thread left), a joined
  Profiler can't time frames or be reset, a main Profiler can't itself be
  joined, and both have the same tick rate (so tests give each its own
  `FakeProfilerCpu` at one rate).
- **Names:** `Options::name` names the thread in the threads table. An empty
  name is "main" for a main Profiler, and for a joined one the thread's own
  name (`GetThreadName()`) when it is made, or "thread N" for a thread with
  none (including any thread `CreateThread()` didn't make), with the lowest N
  no existing joined Profiler has. The main Profiler keeps what each name
  recorded: a Profiler that joins under the name of one that was destroyed
  carries on from it, so a program that keeps recreating threads under the same
  names holds a fixed amount. Two that exist at once with one name are kept
  apart, as two rows of the same name.
- **Totals:** every point's count and self time in the main Profiler's report is
  the sum over the main thread and every joined Profiler, including those
  destroyed. A counter is the sum of its counts. A value is the one set most
  recently on any thread, by the timestamp counter.
- **Points table:** each timed point's self time also shows as a share of the
  frames' time. On many threads the shares add up to more than 100%, as a
  frame is wall time on the main thread and the points are time on every
  thread.
- **Frames:** a point on another thread counts in the main Profiler's frame that
  is in progress when the point ends, and in no frame if it ends between
  frames. The main Profiler's frame breakdowns (the slowest frame, and
  `on_slow_frame`'s report) include those points, including those of a joined
  Profiler destroyed during the frame. Once a Profiler has joined, the self time
  by kind has two columns: the main thread's, which adds up to the frame's
  time, and the other threads', which is time spent alongside it.
- **Threads table:** once a Profiler has joined, the report has a row for the
  main thread and one for each name, sorted by name: busy time per frame on
  average and at most, the average as a share of the average frame, and a
  column for each class of core any thread was busy on, with the share of the
  thread's busy time on it. A joined thread's busy time is the time its
  outermost timed points ran on it, counted when each ends, or its fiber
  suspends, during a frame, by the class of core it was on then. The main
  thread's is its frames' time, by the class of core each frame ran on
  ("mixed" for one that changed).
- **Reading:** only the main Profiler's thread reads its report, as before. It
  reads the joined Profilers' slots while their threads write them, without
  stopping them, so a point that ends as it reads can be missing from a report
  or a frame's breakdown, and appear in the next. Nothing is lost or counted
  twice in the totals. Busy time is gathered at the frame's end, so a point that
  ends in the moment between the gathering and the frame's end counts in the
  next frame.
- **Reset:** `Reset()` on the main Profiler clears what every thread recorded.
  Frame numbers carry on counting rather than starting again, so a thread's
  parts of frames from before the reset are never taken for new ones.
- **For tests:** `GetCount()`, `GetSelfTime()`, `GetValue()`, and the slowest
  frame readers read every thread on a main Profiler, and only the Profiler's
  own thread on a joined one.

A Profiler with nothing joined reports what it did before, plus the share
column in the points table.

## Structure

- **Slots:** each slot field is a `SingleWriterInt64`, a relaxed atomic that
  only its Profiler's thread writes, with `Add()` as a load and a store, so on
  x86-64 each costs the plain move it did before. Another thread reads each
  field whole, in no particular order with the others.
- **Frame numbers:** points tag their frame part through `frame_`, a pointer to
  the frame number in progress: the Profiler's own, or the main Profiler's for a
  joined one. The main Profiler's number has a cache line of its own, apart
  from what each point writes. A slot's frame number is written after its frame
  part is reset, with release and acquire, so the main Profiler never reads an
  old frame's part under a new number. Frame numbers come from
  `last_frame_number_`, which `Reset()` leaves.
- **Joined threads:** the main Profiler keeps a `JoinedThread` per name, under a
  mutex, with at most one Profiler that exists, and what destroyed ones
  recorded. A joined Profiler joins as the last step of its constructor and
  leaves in its destructor, through the main Profiler's `AddJoinedThread()` and
  `RemoveJoinedThread()`, so there is no call to pair, and every lock is taken
  inside the main Profiler's own methods.
- **Totals:** the report gathers a snapshot of totals by point
  (`GatherPointTotals()`), then formats it. It adds the main thread's slots,
  each entry's `folded` totals from destroyed Profilers, and its live
  Profiler's slots less its `baseline`. As the main Profiler can't clear
  another thread's slots, `Reset()` records each live Profiler's totals as its
  baseline instead, and leaving folds its totals less its baseline into the
  entry.
- **Frame breakdowns:** capturing a frame adds every thread's frame part into
  one vector by point index, then drops the points with none. A joined Profiler
  destroyed during a frame leaves its part with the main Profiler
  (`left_frame_points_`), tagged with the frame's number. Each point keeps the
  part of its self time on joined threads, for the two columns of self time by
  kind.
- **Busy time:** when a joined Profiler's outermost timed point ends during a
  frame, it adds the time to a running total for the class of core it ends on
  (`BusyTime`, on a cache line of its own). A point that spans a fiber switch
  adds its run on each thread as its fiber suspends there, and the rest when
  it ends, less what it ran before (`Timing::prior_run_ticks`). These only
  grow, so each frame's
  end gathers what each thread has added since the last, by class, keeping
  each entry's totals since the reset and its busiest frame. What a destroyed
  Profiler added since the last gathering counts in the next.

Nothing here is meant for reuse outside `Profiler`.

## Measurements

On the development machine (an i9-14900KF, with 8 performance cores of class 1
and 16 efficiency cores of class 0), in RelWithDebInfo builds:

- **A timed point:** 11.75ns as a Profiler measures it, and 14.8ns for a
  `ProfileScope` in a loop, the same as before. That needs Release's inlining
  (`/Ob2`, which Game Bits' RelWithDebInfo now uses): with CMake's `/Ob1`,
  MSVC calls two STL helpers in every `std::atomic` load and store, and a point
  costs 20ns.
- **An outermost point on a joined Profiler:** 19.9ns, against 15.0ns for a
  nested one, so busy time adds about 5ns, most of it reading the class of core
  (2.4ns). That is 0.1-0.25% of a 2-5us job.
- **A frame's end:** 27ns with nothing joined, against 13ns before (the lock to
  gather busy times), 100ns with 24 idle joined threads, and 400ns with 24 that
  were each busy in the frame, as their busy times are read from other cores.
  Gathering only each thread's total per frame took 41ns and 170ns, but needed
  a second set of baselines for the times by class; at 0.003% of a 16ms frame,
  the simpler one won.
- **Capturing a frame:** with 24 joined threads, each in all of 300 points in
  the frame, a frame that is the slowest so far takes 12us to capture, against
  0.5us with no threads. A slow frame's report takes 130-185us with or without
  them, nearly all of it formatting.
- **The timestamp counter across cores:** two threads pinned to a performance
  and an efficiency core, passing a reading back and forth, saw it advance by
  at least 20-70ns each way, so the cores agree to within that, and a point on
  another thread lands in the right frame.

## Known limitations

Each is a backlog item:

- **Profiled jobs:** `FiberJobSystem` doesn't give its job threads Profilers,
  so a program joins its own threads.
- **Profile timelines:** there are totals, but no timeline, so overlap, idle
  gaps, and critical paths don't show. A point's busy time counts in the frame
  it ends in, or its fiber suspends in, so a thread's busy time in a frame can
  include time from before it.
- **Joined Profilers' own cost:** the profiler's cost per frame, and in a frame
  breakdown, counts only the main thread's timed points.
