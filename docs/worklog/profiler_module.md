# Profiler module

A profiler for a program with a main loop, cheap enough to leave on all the
time in a realtime program. The program defines points where they are used, in
its own code and around its calls out to other systems. The profiler records
where each frame's time goes, with each timed point charged only for its own
time, and writes a plain text report that diffs cleanly between runs.

It is a new tier 0 library, `gb/profile`, built on `gb_base`. Nothing uses it
yet.

## Behavior

```
// Once, for the thread that runs the main loop:
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

- **Points** have a kind and a name, and are defined once where they are used,
  with the name as a template argument. The same name always means the same
  point, wherever it appears, and using one name with two kinds is an error
  (CHECK). A point whose name is only known at runtime is a `ProfilePoint`
  the program registers and keeps.
  - **Frame:** one iteration of the loop. Frames can't nest.
  - **Scope:** a section of the program's own code.
  - **Call:** a call out to another system.
  - **Counter:** work done, added to as it happens.
  - **Value:** a number describing the workload, set when it changes. The
    report shows the latest value.
- **Self time:** a frame, scope, or call records its time less the time of the
  timed points inside it. A call that calls back into the program has the
  callback's scope subtracted, so the split between the program and the other
  system comes out right.
- **One thread:** a Profiler records the points on the thread that created it,
  and ignores points on any other thread. There is at most one Profiler per
  thread (CHECK). With no Profiler, a point costs about 1ns.
- **No fiber switches inside a timed point:** a timed point must end on the
  fiber and thread it started on, with no switch in between. Ending one that
  isn't the thread's innermost timer is an error (CHECK), which catches both a
  switch to another fiber that times points, and resuming on another thread.
- **Frames:** a histogram of frame times, with 8 buckets for each power of
  two, gives the percentiles (to within 1/16th). The breakdown by point of the
  slowest frame so far is kept. A frame longer than `slow_frame` calls
  `on_slow_frame` with that frame's report.
- **Cost:** when it starts, the Profiler measures what a timed point costs, and
  from that reports its own share of each frame. It checks that against the
  budget: the larger of a fixed time per frame and a fraction of the average
  frame, or no budget if neither is set. Counters and values aren't included;
  they cost a few adds.
- **Report:** values first as a header, then a frame summary, then one line
  per point, grouped by kind and sorted by name so two reports diff cleanly,
  then the slowest frame's breakdown. The program writes its own header lines
  (build, date) before it.
- **For tests:** `GetCount()`, `GetSelfTime()`, and `GetValue()` read a point
  by name, and `Reset()` clears everything recorded. A `FakeTicks` passed in
  the options replaces the CPU's timestamp counter, so no test reads real time.

Only x86-64, where `__rdtsc` exists, which is every platform Game Bits builds
on today.

## Design

### Names

| Name                 | What                                                    | Avoids                                                                |
| -------------------- | ------------------------------------------------------- | --------------------------------------------------------------------- |
| `Profiler`           | Records the points on its thread, and writes the report | -                                                                     |
| `ProfilePoint`       | A registered kind and name, with a fixed index          | -                                                                     |
| `ProfilePoint::Kind` | `kFrame`, `kScope`, `kCall`, `kCounter`, `kValue`       | "type" (`gb/base/type_info.h`)                                        |
| `ProfileTimer`       | Times a scope or call `ProfilePoint` for its lifetime   | `ScopedCall` (`gb/base/scoped_call.h`)                                |
| `ProfileFrame`       | Times a frame point, named by its template argument     | -                                                                     |
| `ProfileScope`       | Times a scope point, named by its template argument     | -                                                                     |
| `ProfileCall`        | Times a call point, named by its template argument      | -                                                                     |
| `ProfileCount`       | Adds to a counter, named by its template argument       | -                                                                     |
| `ProfileSetValue`    | Sets a value, named by its template argument            | -                                                                     |
| `ProfileCallHook`    | A `FunctionHook` Hook that times each call as a call    | -                                                                     |
| `FakeTicks`          | A timestamp source a test controls                      | `FakeClock` (`gb/base`, which is `absl::Time`, not the CPU's counter) |
| "ticks"              | Counts of the CPU's timestamp counter                   | -                                                                     |

### ProfilePoint (`profile_point.h`)

```
class ProfilePoint {
 public:
  enum class Kind { kFrame, kScope, kCall, kCounter, kValue };

  // Registers the point, or returns the one already registered with the name.
  // CHECK-fails if the name was registered with another kind, or if there are
  // more than kMaxProfilePoints.
  ProfilePoint(Kind kind, std::string_view name);

  Kind GetKind() const;
  std::string_view GetName() const;

  // Adds to a counter, or sets a value, in this thread's Profiler, if any.
  void Count(int64_t count) const;
  void SetValue(int64_t value) const;
};
```

- The registry is global and mutex protected. It is only touched when a point
  is registered. Each point gets the next index in a fixed range
  (`kMaxProfilePoints`, 1024), which is its slot in every Profiler.
- Points can also be registered at runtime with a name made then (such as one
  per named task), and kept.

### Profiler (`profiler.h`)

```
class Profiler {
 public:
  struct Options {
    FakeTicks* fake_ticks = nullptr;  // For tests; must outlive the Profiler.

    // The profiler's own budget per frame is the larger of these.
    absl::Duration budget_per_frame;
    double budget_fraction = 0;  // Of the average frame.

    absl::Duration slow_frame = absl::InfiniteDuration();
    Callback<void(std::string_view report)> on_slow_frame;
  };

  // Makes this the Profiler for points on the calling thread. It must also be
  // destroyed on that thread.
  explicit Profiler(Options options = {});
  ~Profiler();

  // The cost of one timed point, as measured when the Profiler started.
  absl::Duration GetPointCost() const;

  // Whole-profile frame statistics: frames, total, average, p50/p90/p99, max,
  // the profiler's own cost per frame, and its budget per frame.
  FrameSummary GetFrameSummary() const;

  // For tests. Unknown names return zero.
  int64_t GetCount(std::string_view name) const;  // Calls, frames, or count.
  absl::Duration GetSelfTime(std::string_view name) const;
  int64_t GetValue(std::string_view name) const;

  std::string GetReport() const;

  // Clears everything recorded. CHECK-fails inside a timed point.
  void Reset();
};
```

- The current Profiler is a `thread_local` pointer, which is what makes points
  on other threads free and harmless. Its slots are one heap array of
  `kMaxProfilePoints`, indexed by the point.
- Each slot holds the totals (calls, self ticks, max self ticks) and the
  current frame's (calls, self ticks), tagged with the frame number they
  belong to. A point's first add in a new frame resets the frame's part, so
  nothing is cleared per frame.
- Recording is the Profiler's job: timers and points call its private inline
  members (start and end a timing, add time, add a count, set a value), and
  never touch its slots directly, so later CLs change what is recorded in one
  place.
- Ticks are converted to time with a frequency calibrated against
  `std::chrono::steady_clock` (QPC, never adjusted, unlike the wall clock
  `absl::Now()` reads) when the first Profiler in the program starts, over
  about 1ms, which is also long enough to measure a timed point's cost. Each
  end of the calibration reads the clock between two reads of the counter,
  five times, keeping the tightest, so a thread switch between the paired
  reads can't skew it. With `FakeTicks`, the fake gives the frequency, and
  startup takes no time.
- **Brittleness:** points recorded on another thread are dropped silently.
  That is deliberate (a hooked function may be called from any thread), but a
  point placed on the wrong thread just reads zero.

### Timers (`profile_timer.h`)

```
class ProfileTimer {
 public:
  // Times a scope or call point.
  [[nodiscard]] explicit ProfileTimer(const ProfilePoint& point);
  ~ProfileTimer();
};

template <ProfileName kName>
class ProfileScope {  // ProfileCall is the same, for a call point.
 public:
  [[nodiscard]] ProfileScope();
};

template <ProfileName kName>
class ProfileFrame {
 public:
  [[nodiscard]] ProfileFrame();
  ~ProfileFrame();
};

template <ProfileName kName>
void ProfileCount(int64_t count);

template <ProfileName kName>
void ProfileSetValue(int64_t value);
```

- `ProfileName` holds a string literal as a template argument (a C++20
  class-type template parameter). Each named template registers its point once,
  in a function-local static, so after the first use a point costs no lookup,
  exactly as a point kept by the program does.
- The constructors are `[[nodiscard]]`, so a timer created without a variable
  name (which would end immediately) is a compile error under `/WX`.
- A timer holds a `Profiler::Timing`, its entry in a stack of the timings
  running on the thread, linked through the timers themselves on the C++
  stack. Starting one reads the counter and pushes it. Ending one checks it is
  the top, reads the counter, pops it, adds its elapsed time to its parent's
  child time, and adds its elapsed time less its own child time into its slot.
  Everything is inline, and there is no lookup or allocation.
- `ProfileFrame` is separate so that timers don't pay a branch on the kind.
  Ending a frame adds it to the histogram, and, rarely, captures its
  breakdown (a `FrameBreakdown`) when it is the slowest so far or longer than
  `slow_frame`. A capture walks the slots of every registered point, which is
  fine for a rare event. Between frames, points are recorded against frame 0,
  which is never captured.
- **Brittleness:** timers are RAII, so they can't be left unbalanced, and the
  top-of-stack CHECK catches a fiber switch inside one. Without `/GT`, MSVC
  can cache a `thread_local`'s address across a fiber switch, so the CHECK
  compares against the Profiler the timer started with, not a fresh read.

### ProfileCallHook (`profile_call_hook.h`)

```
class ProfileCallHook {
 public:
  explicit ProfileCallHook(std::string_view name);  // Registers a call point.

  template <typename Function, typename... Args>
  auto Call(Function original, Args&&... args);  // Times the call.
};
```

### To confirm

- **Is the timestamp counter invariant?** Checked before CL1 on the
  development machine: CPUID reports an invariant TSC, at 3.187GHz. The
  performance counter (QPC) runs at only 10MHz, so it can't stand in for it.
- **What a timed point costs:** a prototype of the timer above (thread-local
  profiler, self time, frame tagging) measured 13.4ns a point in an optimized
  build, nested or not, and 0.9ns with no profiler. A bare `__rdtsc` is 5.3ns.
  CL3 measured the real thing in a Release (RelWithDebInfo) build, as a
  Profiler measures it when created: 11.75ns to 12.25ns a point over six runs
  (17.75ns in Debug), well under the 20-25ns target. That measures the timing
  itself, from the Profiler's side, so a ProfileTimer's thread_local read and
  null check (about 1ns) aren't included.
- **Does `thread_local` follow a fiber?** Checked before CL1: no. A fiber sees
  the `thread_local` of the thread it runs on, and without `/GT` (which Game
  Bits doesn't use) a fiber that moved threads read the old thread's storage.
  Hence the rule that a timed point can't span a fiber switch.
- **How reliable is a 1ms calibration?** Checked in CL1, against a 1s
  calibration: within 100ppm (0.01%), limited by the clock's 100ns
  resolution. A thread switch inside the window doesn't matter, as both clocks
  keep running, but a simulated 2ms switch between the paired reads at either
  end made the rate 100% to 1500% wrong. Bracketing each end with two counter
  reads, keeping the tightest of five, held it within 63ppm in every case.
  Reading the steady clock costs 11ns and the wall clock 14ns, both in 100ns
  steps, against 5ns for the counter in 0.3ns steps, so neither could time the
  points themselves.

## CLs

### CL1 [x] gb/profile: Timed points, counters, and values

Depends on: nothing.

- New `gb_profile` library, added to the tier 0 list in README.md.
- `FakeTicks`, `ProfilePoint` and its registry, `Profiler` (install, slots,
  tick frequency, `GetCount`/`GetSelfTime`/`GetValue`, `Reset`),
  `ProfileTimer`, `ProfileName`, `ProfileScope`, `ProfileCall`,
  `ProfileCount`, and `ProfileSetValue`.
- Unused, so no visible change.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: registering by name and kind, and the kind mismatch CHECK; named
  templates and runtime points with the same name are the same point; self
  time through nesting, including a scope inside a call inside a scope;
  counters and values; points on another thread and with no Profiler are
  ignored; one Profiler per thread; ending a timer that isn't the innermost
  CHECKs; `Reset`.

### CL2 [x] gb/profile: Frames

Depends on: CL1.

- `ProfileFrame`, the frame histogram and percentiles, the slowest frame's
  breakdown, and `GetFrameSummary()` (without cost).
- For tests, `GetSlowestFrameCount()` and `GetSlowestFrameSelfTime()` read a
  point's part in the slowest frame by name, and
  `ProfilePoint::GetRegisteredCount()` bounds the capture.

**Verify**
- Standard checks.
- Unit tests: percentiles within a bucket; self time per frame; the slowest
  frame's breakdown is replaced only by a slower frame; points outside frames
  count in totals; nested frames CHECK.

### CL3 [x] gb/profile: Own cost and budget

Depends on: CL2.

- The timed point cost measured at startup, the profiler's cost per frame, and
  the budget check in `GetFrameSummary()`.

**Verify**
- Standard checks.
- Unit tests, with `FakeTicks` advancing on each read: the measured cost, cost
  per frame, and each side of the budget (fixed, fraction, larger of the two).
- Performance: a real Profiler's `GetPointCost()` in a Release build, logged
  by a test, against the 20-25ns target.

### CL4 [x] gb/profile: Report and slow frames

Depends on: CL3.

- `GetReport()`, and `on_slow_frame` with the frame's report.
- `ProfilePoint::GetRegisteredPoints()` gives the report each point's name and
  kind. Only points the Profiler has recorded are reported, so setting a value
  now also counts it.

**Verify**
- Standard checks.
- Unit tests: the whole report for a fixed profile, as text; points in a
  stable order regardless of registration order; a slow frame reports once
  with its own breakdown.

### CL5 [x] gb/profile: ProfileCallHook

Depends on: CL1.

- `ProfileCallHook`, for timing calls through a `FunctionHook`.

**Verify**
- Standard checks.
- Unit tests: a hooked function pointer is timed as a call, with a callback
  into the program's scope subtracted; a hook with no Profiler still calls
  through.
