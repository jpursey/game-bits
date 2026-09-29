# Profiler module

A profiler for a program with a main loop, cheap enough to leave on all the
time in a realtime program. The program defines points where they are used, in
its own code and around its calls out to other systems. The profiler records
where each frame's time goes, with each timed point charged only for its own
time, and produces a plain text report that diffs cleanly between runs.

It is the tier 0 library `gb/profile`, built on `gb_base`. A timed point costs
about 12ns in an optimized build, and about 1ns on a thread with no Profiler.

## Using it

```
// Once, on the thread that runs the main loop:
gb::Profiler profiler({.budget_per_frame = absl::Microseconds(20),
                       .budget_fraction = 0.01,
                       .slow_frame = absl::Milliseconds(5),
                       .on_slow_frame = [](std::string_view report) {
                         LOG(WARNING) << report;
                       }});

void Surface::Run() {
  gb::ProfileFrame<"Run"> frame;
  {
    gb::ProfileScope<"TrackCache::Refresh"> scope;
    ...
  }
  gb::ProfileCount<"midi_out/messages">(sent);
}

gb::ProfileSetValue<"tracks">(track_count);  // When it changes.

// Every call through the pointer is a call point named "reaper/GetTrack".
gb::FunctionHook<&GetTrack, gb::ProfileCallHook> hook("reaper/GetTrack");

std::string report = profiler.GetReport();
```

## Behavior

- **Points** have a kind and a name. The same name always means the same point,
  and using one name with two kinds CHECK-fails. Most are named by template
  argument where they are used (`ProfileFrame`, `ProfileScope`, `ProfileCall`,
  `ProfileCount`, `ProfileSetValue`). A point named at runtime is a
  `ProfilePoint` the program keeps, timed with `ProfileTimer`, or counted and
  set with `Count()` and `SetValue()`. A program can register up to
  `kMaxProfilePoints` (1024) distinct names; the limit counts only points that
  are reached, and is one constant to raise.
  - **Frame:** one iteration of the loop. Frames can't nest (CHECK).
  - **Scope:** a section of the program's own code.
  - **Call:** a call out to another system.
  - **Counter:** work done, added to as it happens.
  - **Value:** a number describing the workload, set when it changes.
- **Self time:** a frame, scope, or call records its time less the time of the
  timed points inside it, so a callback into the program from inside a call is
  charged to the program, not the call.
- **One Profiler per thread:** a Profiler records the points on the thread that
  created it, and must be destroyed there. Points on other threads, or on a
  thread with no Profiler, are ignored.
- **No fiber switches inside a timed point:** a timed point must end on the
  fiber and thread it started on. Ending one that isn't the thread's innermost
  CHECK-fails, which catches a switch to another fiber that times points, and
  resuming on another thread.
- **Frames:** `GetFrameSummary()` gives the frame count, total, average, p50,
  p90, p99, and max, from a histogram with 8 buckets for each power of two
  (percentiles within 1/16th, exact below 16 ticks). The breakdown by point of
  the slowest frame so far is kept. Points between frames count in the totals,
  but in no frame's breakdown.
- **Cost and budget:** a Profiler measures what a timed point costs when it is
  created, and `FrameSummary` reports its own cost per frame (the timed points
  in frames times that cost) and its budget: the larger of `budget_per_frame`
  and `budget_fraction` of the average frame, or infinite if neither is set.
- **Report:** `GetReport()` returns the values, the frame summary (with the
  profiler's cost against its budget), one line per point with its count and
  self time in total, per frame, and per call, and the slowest frame's
  breakdown. Only points the Profiler recorded appear (a counter whose total is
  zero counts as not recorded), grouped by kind and sorted by name, with aligned
  columns and times to three significant digits, so two reports diff cleanly.
  A program puts its own header (build, date) before it.
- **Slow frames:** a frame longer than `slow_frame` calls `on_slow_frame` with
  that frame's report, just after the frame ends, so any points the callback
  reaches are outside the frame.
- **For tests:** `GetCount()`, `GetSelfTime()`, `GetValue()`,
  `GetSlowestFrameCount()`, and `GetSlowestFrameSelfTime()` read a point by
  name, and `Reset()` clears everything recorded. A `FakeTicks` in the options
  replaces the CPU's timestamp counter, so no test reads real time.
- **Platforms:** x86-64 only, where `__rdtsc` exists, which is every platform
  Game Bits builds on. The timestamp counter must be invariant.

## Structure

| File                  | What                                                                                  |
| --------------------- | ------------------------------------------------------------------------------------- |
| `profile_point.h`     | `ProfilePoint` and its registry, `ProfileName`, `ProfileCount`, and `ProfileSetValue` |
| `profile_timer.h`     | `ProfileTimer`, `ProfileScope`, `ProfileCall`, and `ProfileFrame`                     |
| `profiler.h`          | `Profiler`: recording, frames, cost, and the report                                   |
| `profile_call_hook.h` | `ProfileCallHook`, a Hook for `gb/base/function_hook.h`                               |
| `fake_ticks.h`        | `FakeTicks`, the timestamp counter for tests                                          |

- **Registry:** global and mutex protected, touched only when a point is
  registered. Points live in a fixed array, so names stay at fixed addresses,
  with a `flat_hash_map` from name to index. A point's index is its slot in
  every Profiler.
- **Named points:** `ProfileName` holds a string literal as a C++20 class-type
  template argument. Each named template registers its point once, in a
  function-local static, so after the first use it costs no lookup.
- **Recording:** the current Profiler is a `constinit thread_local` pointer.
  Timers and points call the Profiler's private inline members, which alone
  touch its slots. Each slot holds totals (count, self ticks, latest value) and
  the current frame's count and self ticks, tagged with the frame number; the
  first add in a new frame resets the frame's part, so nothing is cleared per
  frame.
- **Timing:** each timer holds a `Profiler::Timing`, its entry in a stack of
  the timings on the thread, linked through the timers on the C++ stack.
  Ending one reads the counter, pops it, adds its elapsed ticks to its parent's
  child ticks, and adds its elapsed ticks less its own child ticks to its slot.
  `ProfileFrame` is a separate type, so other timers don't pay a branch on the
  kind.
- **Frames:** ending a frame adds it to the histogram, and, rarely, captures its
  breakdown (walking only the registered points) when it is the slowest so far
  or slow.
- **Cost:** the constructor times 5 rounds of 100 points inside an outer
  timing, keeps the fastest round, then clears everything with `Reset()`.
- **Calibration:** ticks are converted to time with a rate measured once per
  program against `std::chrono::steady_clock` over 1ms. Each end reads the
  clock between two counter reads, five times, keeping the tightest, so a
  thread switch between the paired reads can't skew it.
- **Report:** built in a `std::string` with Abseil's string utilities.

Reusable pieces, should anything else need them: `FakeTicks` (a fake
timestamp counter with auto advance, like `FakeClock`), `ProfileName` (a string
literal as a template argument), and the bracketed calibration of the
timestamp counter in `profiler.cc`.

## Measurements

On the development machine (3.187GHz, invariant TSC; QPC runs at 10MHz):

- **A timed point:** 11.75ns to 12.25ns in a RelWithDebInfo build, as a
  Profiler measures it (17.75ns in Debug). That leaves out finding the thread's
  Profiler, about 1ns. With no Profiler, a point costs about 1ns. A bare
  `__rdtsc` is 5.3ns.
- **Calibration:** a 1ms calibration is within 100ppm of a 1s one, limited by
  the steady clock's 100ns resolution. A simulated 2ms thread switch between
  the paired reads at either end made an unbracketed calibration 100% to 1500%
  wrong; bracketed, it stayed within 63ppm.
- **Other clocks:** the steady clock costs 11ns and the wall clock 14ns to
  read, both in 100ns steps, so neither could time the points themselves.
- **Fibers:** a fiber sees the `thread_local`s of the thread it runs on, and
  without `/GT` (which Game Bits doesn't use) a fiber that moved threads read
  the old thread's storage, hence the rule against fiber switches inside a
  timed point.

## Known limitations

Each is a backlog item:

- **Multithreaded profiles:** each thread's Profiler is separate, and the
  report reads a Profiler's own slots.
- **Fiber-aware profiling:** a timed point can't span a fiber switch.
- **Fiber-safe thread locals:** whether Game Bits' own thread locals are safe
  across fiber moves.
