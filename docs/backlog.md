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

## Core-aware job threads

- **Layers:** thread, job
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Requested by:** Terra Diem *Job threads per physical core*
- **Background:** *Multithreaded profiles*, `gb/profile/win_cpu_info.cc`

`gb/thread` knows the hardware threads only as a flat list of affinities in
processor order (`GetHardwareThreadAffinities()`, read from the process
affinity mask, so it covers one processor group of at most 64), and
`FiberJobSystem` defaults to one job thread per hardware thread, pinning job
thread *i* to hardware thread *i*. Neither knows which hardware threads share a
core, or which cores are faster. On an i9-14900KF, Windows numbers the 8
performance cores' SMT pairs 0-15 and the 16 efficiency cores 16-31, so asking
for 24 job threads (one per core) pins two to each performance core and leaves
8 efficiency cores idle.

Measured on that CPU with Terra Diem's chunk generation and lighting (16,384
chunks of compute-bound jobs, 2026-10-15), with one per core set up by
restricting the process affinity before creating the job system:

| Job threads | Placement | Generation | Lighting |
|---|---|---|---|
| 24 | one per core | 0.38 s | 1.96 s |
| 24 | in processor order | 0.45 s | 2.33 s |
| 32 | every hardware thread | 0.35 s | 1.83 s |

Placement mattered more than count: processor order took 17-19% longer than
one per core, while using the SMT siblings as well gained only 7-9%. While
jobs contended for the heap, every hardware thread took about 30% longer than
one per core. A performance core did about 1.6 times the work of an efficiency
core.

Windows tells an ordinary program all of this.
`GetLogicalProcessorInformationEx()` reports each core's hardware threads (in
every processor group), whether it has SMT, and its efficiency class (higher is
faster, as `gb/profile` already uses it), and with `RelationCache`, which cores
share each cache. `GetSystemCpuSetInformation()` reports the same for each
hardware thread, with an ID for `SetThreadSelectedCpuSets()`, which steers a
thread without the hard limit of an affinity mask. Add:
- A topology query in `gb/thread`: the cores, each with its hardware threads
  (as affinities `CreateThread()` accepts), its class, and the caches it shares
  (the last level, and the L2 that efficiency cores share in clusters of four).
  The generic version reports one class-0 core per hardware thread. Parse
  Windows' buffer in a function tests can feed captured buffers: a hybrid CPU
  like this one, a uniform CPU with SMT, one without, and two processor groups.
  `gb/profile`'s class table then comes from it, as *Multithreaded profiles*
  expects.
- Placement by core in `FiberJobSystem`: an option to count job threads in
  cores, one per core, fastest class first, each pinned to its core's hardware
  threads (or steered with CPU sets) rather than to one hardware thread. Its
  contract says which is the default; changing the default changes every
  project.
- An option to leave the fastest core, or a given number of cores, without job
  threads, for a program's main loop, and the topology a program needs to
  place its main thread, and any pool for blocking work, on hardware threads
  the job threads don't use.
- A report of where each job thread went (core and class), for logs and
  *Multithreaded profiles*.
- Windows moves the threads of a background window onto efficiency cores
  (*Multithreaded profiles*). Check whether opting job threads out of that
  (`SetThreadInformation()` with `ThreadPowerThrottling`) is worth an option.

Tests check parsing against captured buffers, and placement against a fake
topology: which hardware threads each job thread gets under each option.

## Default texture for unset bindings

- **Layers:** render, render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Drawing with a texture binding that was never set crashes in
`VulkanBindingData::OnRender()`. Skipping the binding would leave its
descriptor unwritten or pointing at a deleted image, which is worse. Give the
backend a default texture (one opaque pixel, say) that every texture binding
starts with and falls back to, and check it by hand in `examples/render-check`
with a material whose texture is never set.

## clang-cl build

- **Layers:** container, parse, job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Game Bits is meant to build with clang-cl as well as MSVC (CMakeSettings.json
has `x64-Clang-Debug` and `x64-Clang-Release`), but clang 19 rejects some of
it:
- `TokenIndex`'s defaulted `operator<=>` in `parse_types.h` compares
  `uint32_t` bit fields, which clang won't narrow to the `int` they promote
  to. Every file that includes `parse_types.h` fails.
- `array.h`'s `using` declarations of the base's dependent types need
  `typename`, and one function reads the base's protected `data`.
- `fiber_job_system_test.cc` has an unused lambda capture, an error with
  warnings as errors.

These were found by clang-tidy, which parses with clang; there may be more
once those build. Building needs Visual Studio's "C++ Clang tools for Windows"
component, as the MSVC standard library rejects older clang versions. Fix them,
build and test both clang configurations, and consider building one in CI so it
stays working.

## Multithreaded profiles

- **Layers:** profile
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Profiler module*
- **Requested by:** Terra Diem *Profile the world's jobs*
- **Background:** [Profiler module](worklog/profiler_module.md)

Combine the profiles of several threads, each with its own `Profiler`, into
one view: CPU time and calls per point across all threads, per frame of the
main loop, and how busy each thread is per frame. Point indices are global, so
every Profiler's slots line up and combining them is a sum. Each thread adds a
snapshot into a shared total at a safe point of its own (the end of its frame,
or between jobs), or the slots become atomics written only by their own thread
(plain moves on x64) and are read live. Overlap and critical paths need a
timeline of events instead, which this is not (see *Profile timelines*).
`Profiler::GetReport()` reads its own slots, so the combined view needs the
report to format a snapshot of slots instead, which a combined total can also
provide.

Terra Diem would be its main user: its world loads, lights, meshes, and
(later) simulates as thousands of short jobs on a `FiberJobSystem`. For that:
- Job threads get their Profilers from the job system, which owns the threads
  (*Profiled jobs*).
- A job thread has no frames of its own, so its points count in the main
  loop's frame whose interval they end in, on the shared timestamp counter.
- Counters sum across threads (a job counting the chunks it lit), and a value
  is the latest set on any thread.
- The report shows each thread's busy time and class of core, and gives each
  point's share of the frame alongside its total: a frame is wall time, while
  the threads' points add up to CPU time, so the two can't be summed.

The kind of core a thread runs on matters as much as what it runs. On an
i9-14900KF (8 performance and 16 efficiency cores), frames on efficiency cores
took about 25-30% longer than the same frames on performance cores, and
Windows moved a program's main thread onto them when its window was in the
background (2026-10-05). The tick rate was not at fault: it measured the same
in every session, matching what CPUID reports. The single-thread report
already splits frames by core class and warns when they mix; a combined view
needs the same per thread, since job threads spread across both kinds of core.
Pinning threads to one kind would make profiles compare, but `gb/thread`'s
affinities don't know which hardware threads are which kind. Only
`gb/profile`'s `win_cpu_info.cc` builds that table, which would move to
`gb/thread` for it. *Core-aware job threads* does that move, and pins each job
thread to one core, which fixes each thread's class.

## Fiber-aware profiling

- **Layers:** thread, profile
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Profiler module*, *Fiber-safe thread locals*
- **Requested by:** Terra Diem *Profile the world's jobs*
- **Background:** [Profiler module](worklog/profiler_module.md)

Let a timed point span a fiber switch, which is a CHECK failure today. When a
fiber switches out, its open timers pause and leave the thread's timer stack,
and when it switches back in, on any thread, they resume there, so a job that
waits is charged only for the time it runs. Needs a hook in `gb/thread`'s fiber
switch that the profiler can use without `gb/thread` depending on it. Most
useful alongside *Multithreaded profiles*, since fiber jobs are the main
multithreaded case.

Terra Diem plans phases of work that wait on job counters inside jobs, so the
timed points around them span fiber switches. Besides the time a timed point
runs, record the time it spent switched out, as its wait time: a phase that
mostly waits for other jobs needs a different fix from one that runs long.

## Profiled jobs

- **Layers:** profile, job
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Multithreaded profiles*, *Fiber-safe thread locals*
- **Requested by:** Terra Diem *Profile the world's jobs*
- **Background:** *Multithreaded profiles*, *Fiber-aware profiling*

Let `FiberJobSystem` profile the jobs it runs, so a program can see whether its
job threads are kept busy and where their time goes, without timing every job
itself:
- An option that gives each job thread a `Profiler` for as long as the thread
  runs, made with options the program passes (so tests can give a
  `FakeTicks`), and combined through *Multithreaded profiles*.
- Each job timed as a point named after the job: its count, run time, and
  distribution (p50, p90, p99, and max, from the histogram frames already
  use), since a phase that waits for many jobs finishes with the slowest.
- How long each job waited to start, from `Run()` to running, and how long a
  waiting job took to resume once its counter reached zero, per job name. That
  shows whether a program's jobs need priorities, or more threads.
- Each job thread's busy and idle time per frame of the main loop, idle being
  the time it spends waiting for work.
- Time blocked in the OS on a contended `absl::Mutex`. Abseil reports each
  contended wait through `absl::RegisterMutexProfiler()`, in cycles that on
  x86-64 are timestamp counter ticks, like the profiler's. A program can
  register only one such hook, so Game Bits should own it, recording the waits
  as a point of their own on the thread that waited (check which thread the
  hook runs on).
- Job names that cost nothing per run. Naming jobs today (`kKeySetFiberNames`)
  copies each name into a `std::string`, adds the job to a set under a global
  mutex, and sets the thread's description twice, a system call each time. A
  job named by a `ProfileName` (a string literal as a template argument) or a
  registered `ProfilePoint` would have a fixed name for profiles, the shutdown
  report, and debuggers, with no allocation.

Profiling is off unless asked for, and costs a few timestamp reads per job when
on.

## Profile timelines

- **Layers:** profile, job
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Multithreaded profiles*, *Fiber-aware profiling*,
  *Profiled jobs*
- **Requested by:** Terra Diem *Profile the world's jobs*
- **Background:** *Multithreaded profiles*, which leaves timelines out

Totals can't show overlap, idle gaps, or what a frame waited for, and on many
threads those are what decide frame times. Record each timed point as an event
(its point, start and end ticks, thread, and job) in a fixed-size buffer per
thread, cheap enough to leave on, and write a captured stretch as a trace in
Chrome's trace event format (JSON), which Perfetto's viewer (ui.perfetto.dev)
and `chrome://tracing` open, with no new dependency:
- Capture the last few frames on request, and a slow frame automatically:
  `on_slow_frame` can save the slow frame's trace beside its breakdown, so an
  occasional hitch can be examined afterwards.
- Jobs from *Profiled jobs*: queued, started, waiting, resumed, and finished,
  linked from where each was queued to where it ran, so a frame's critical
  path through its jobs can be followed.
- Fiber switches from *Fiber-aware profiling*, so a job that moves between
  threads stays one track.
- Counters and values as plotted tracks.

Tracy (a third-party profiler) already does all of this, with a live viewer,
fibers, locks, and plots. It would be a new dependency, with its own
instrumentation alongside `gb/profile`'s points, so decide between it and this
before starting.

## Allocator overrides

- **Layers:** base, alloc
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Every `Allocator` subclass that overrides `Alloc(size, align)` hides the
one-argument `Alloc(size)`, so it can't be called on the concrete type
(`pool_allocator.Alloc(64)` doesn't compile). `TsAllocator` brings it back with
a `using`, but each other subclass would have to remember to. Make `Alloc()`
and `Free()` public non-virtual functions that call protected virtual
`DoAlloc()` and `DoFree()`, so overriding can't hide anything. This breaks
every subclass outside Game Bits, which must rename its overrides.

## Replace ScopedCall with absl::Cleanup

- **Layers:** base, file, resource
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** none

`gb::ScopedCall` does what `absl::Cleanup` does (from `absl/cleanup/cleanup.h`),
which also has `Cancel()` and `Invoke()`. Switch Game Bits' own uses to
`absl::Cleanup` (`local_file_protocol_test.cc`, `resource_file_reader.cc`, and
`resource_file_writer.cc`), and mark `ScopedCall` deprecated in favor of it.
Removing `ScopedCall` breaks every project that uses it, so each one switches
first, and it is removed once they have.

## Parser rules shared between threads

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`ParserRules::Validate()` is const, but sets the value of each token item from
the lexer it validates against, so two `Parser::Create()` calls with the same
new `ParserRules` on different threads race, and rules validated against one
lexer keep values from it when used with another (a lexer that forces
identifiers to lower case parses a literal differently). Resolve token values
per parser instead, such as in a table the `Parser` builds from its rules and
lexer, so shared rules are never written. `ParserProgram` validates its rules
once and is unaffected.

## Parser groups merge their children's items

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** `Parser::MatchGroup` and `Parser::MatchRuleItem` in
  `parser.cc`

`Parser::items_` tells each rule item where its named sub-items go: the
enclosing group's items for an unnamed group or unscoped rule (`<rule>`), or
null for one that keeps its own. `MatchGroup` sets it before each sub-item, and
every match function must restore it before returning (`MatchGroup` does with
an `absl::Cleanup`, `MatchRuleItem` by hand). One that doesn't sends the items
of later sub-items to the wrong map.

Remove the member and let the parent decide instead. Each group returns its own
items, and when an unnamed sub-item matches, `MatchGroup` merges that match's
items into its own if the sub-item inlines them: a group does, a rule name does
unless it is scoped, and a token has none. That removes `items_`, the
`scope_items` argument to `MatchRuleItem`, and the cleanup, and a failed match
leaves nothing behind because it has no items to merge. The existing parser
tests, including those for failed inline groups, cover the behavior.

## Validate chunk contents

- **Layers:** file, resource
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

A corrupt chunk file can make readers read out of bounds. `ResourceFileReader`
now checks that a chunk is large enough for its count of structs, but
`ChunkReader::ConvertToPtr()` trusts the offset in a `ChunkPtr` (and the string
it points to is not checked for a terminator), and flat buffer chunks are
passed to `flatbuffers::GetRoot()` without verifying them. Check `ChunkPtr`
offsets and strings against the chunk size in `ChunkReader`, and run a
`flatbuffers::Verifier` bounded by the chunk size before a flat buffer chunk
reaches its reader, failing the read if either is invalid.

## Testable render asserts

- **Layers:** render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`RENDER_ASSERT` calls `assert`, and is off unless `SetRenderAssertEnabled()`
turns it on, so tests can't check that misuse is caught: the `DrawList`
argument checks, for instance, were inverted without any test noticing. Let a
test install a handler that records failed render asserts instead of aborting
(as the test backend counts invalid calls), and test the asserts that guard
`DrawList`, `MeshView`, `RenderBufferView`, and `BindingData`.

## Explicit resource bool conversions

- **Layers:** resource
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`ResourcePtr`, `ResourceEntry`, and `ResourceNameReservation` have an implicit
`operator bool`, so comparisons and arithmetic that have no meaning compile
by converting to `bool` (`ResourcePtr` equality used to). Make the conversions
`explicit`. This breaks callers that convert implicitly, such as returning one
from a function that returns `bool`, which each project fixes as it updates.

## Resource loading races

- **Layers:** resource
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Resources loaded or released from several threads still have gaps:
- Loading a name that another thread is loading fails, as the `TODO` in
  `ResourceSystem` says, rather than waiting for that load.
- A resource is in the system from its `Resource` base constructor on, so two
  threads reading files that embed the same resource ID can find each other's
  resource before it is fully constructed, or just before a failed read
  deletes it.
- A reference added and removed while a release handler runs, after the
  handler's delete failed because of it, leaves the resource unreferenced and
  not released.
- Resources embedded in a file read without a `ResourceSet` are never
  referenced, so they stay loaded until their manager is destroyed.

Each needs a test that makes the interleaving happen deterministically.

## Vulkan frame failure loose ends

- **Layers:** render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Smaller problems in the Vulkan backend, none of which can be unit tested:
- If the command buffer fails to begin after a swap chain image is acquired,
  the image's semaphore is left signaled, so the next acquire waits on a
  signaled semaphore.
- While `BeginFrame()` keeps failing (such as while minimized), nothing is
  garbage collected, so resources deleted meanwhile are only freed once
  rendering resumes.
- A `kPerFrame` texture's host buffers start uninitialized, so mipmaps
  generated by its first partial update include garbage.
- `VulkanBindingDataFactory::NewBindingData()` builds the binding data while
  holding the factory's lock.

Check each by hand in `examples/render-check`.

## GPU-side kStaticWrite updates

- **Layers:** render_vulkan
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`kStaticWrite` resources keep no copy of their data on the host, so updates
that need the existing contents fail:
- A partial update of a mipmapped `kStaticWrite` texture fails, as its mipmaps
  would be generated from host memory that doesn't hold the rest of the
  texture. Generating mipmaps on the GPU (with `vkCmdBlitImage`) would let it
  work, and would be faster for every volatility.
- Growing a `kStaticWrite` render buffer that was recently rendered fails, as
  the old values can't be read back. A device to device copy from the old
  buffer would keep them. Nothing grows one today.

## Large GUIs across meshes

- **Layers:** imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`ImGuiInstance` draws a GUI as one mesh, and its indices and
`DrawList::DrawPartial()`'s vertex offset are 16 bits, so a GUI over
`kMaxMeshVertices` skips its frame, logging an error. Split a large GUI's draw
lists across several meshes instead, checked by hand with
`examples/render-check`'s "Go over the vertex limit" button, which should then
keep drawing.

## Register returns matching render types

- **Layers:** render, imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`RenderSystem`'s `Register*` functions fail for a name already registered, so
code that may run more than once (such as each `ImGuiInstance`'s `Init()`)
has to look each type up first and register it only if missing. That also
never checks that the existing type matches. Have `Register*` return the
existing type when its definition matches, and fail when it differs, then
simplify `ImGuiInstance::Init()` to just register.

## Shared gb_render test support

- **Layers:** render, imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`gb_imgui`'s tests draw with `gb_render`'s test backend, which they compile
from its source files, listed again in `gb_imgui`'s `CMakeLists.txt`. Make
the test backend and `RenderTest` a library target that tests link instead,
so the list is kept in one place.

## FakeJobSystem

- **Layers:** job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** Terra Diem *Use Game Bits' FakeJobSystem in World tests*
- **Background:** none

Code that does its work in jobs can only be tested on `FiberJobSystem`, which
runs jobs whenever its threads get to them, so a test can't choose the order
work finishes in, and a bug that depends on that order shows up only by chance.
Add `FakeJobSystem` in `gb/job/fake_job_system.h`, a `JobSystem` that runs jobs
only when the test says, as `FakeClock` moves time only when the test says:
- `Run()` queues a job, and `RunPendingJobs()` runs the queued jobs, and the
  jobs they queue, until none are left. They run one at a time, on a job thread
  (one where `JobSystem::Get()` returns the fake).
- Jobs with names the test lists run before `Run()` returns, as if a thread had
  picked them up at once, so a test can have work finish while the code that
  started it is part way through. Each runs on a job thread of its own, as its
  caller may hold locks the job needs; a job that needs a lock its caller holds
  deadlocks, which the API says.
- Job counters, contexts, and job data work as they do in `FiberJobSystem`.
  Each job has its own context and data, freed when it ends. Counting jobs
  needs `FakeJobSystem` among `JobInternal`'s friends.
- `Wait()` runs queued jobs until the counter is done, and fails, rather than
  hanging, if none are left and it still isn't.

Test each of these in `gb_job`'s tests.

## Deterministic concurrency tests

- **Layers:** game, job, message, resource
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Several tests still sleep or use the real clock, and several concurrency fixes
have no test that fails without them, as nothing lets a test arrange the
interleaving:
- Some `FiberJobSystem` tests sleep in jobs so that waits are likely to block.
- `MessageSystem`'s `RemoveEndpointFromOtherThread` and two thread dispatcher
  destruction tests sleep to make their race likely. The message system's
  fixes for erasing a stale iterator, a concurrent erase, and destroying an
  endpoint or stack while delivering have no deterministic tests.
- Resource system races other than the release race have none either.
- The `Game::Run()` overloads that make their own context run a frame on the
  real clock, as nothing can give them a fake one.

Add test hooks (a callback at the point of interest, or a clock passed in)
that let each test wait for the state it needs, rather than sleeping.

## Lexer storage on re-parse

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Decoded strings, escaped characters, and forced-case identifiers are stored
again each time a token is parsed again after the parser backs up, so a
lexer's memory grows with the parsing work done rather than the text. Cache
them per token, which needs care where a block comment re-tokenizes a line.

## Symbols longer than 7 characters

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

A `Symbol` holds up to 7 characters and silently truncates a longer one, so a
lexer config with a long symbol gets a shorter one than it asked for. Reject a
symbol that is too long where it is made, and in `LexerConfig` validation,
with a test.

## Config text edge cases

- **Layers:** config
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Text configs still don't round trip a few values:
- Infinities, NaNs, and subnormal floats are written as text that can't be
  read back, as JSON has no text for them. Pick a text form for them in the
  Game Bits format.
- JSON output escapes control characters as `\xNN`, which JSON doesn't
  support (`\u00NN` is the JSON form).
- A non-compact empty map is written as `{` and `}` on separate lines with a
  blank line between.
- Reading recurses once per nesting level, so very deeply nested text can
  overflow the stack. A nesting limit would fail it instead.

## Collision precision and conversions

- **Layers:** collide
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Ray / triangle intersection treats a ray as parallel to the triangle using an
absolute epsilon, so a ray with a very short direction can miss; scale it by
the direction's length. Separately, `CLine` and `CTriangle` convert implicitly
from a pointer to an array, so any pointer converts; making them `explicit`
breaks callers that rely on it.

## Game state trace details

- **Layers:** game
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

A `kInvalidChangeSibling` trace from a state that isn't active names that
state only in its message, as `GameStateTrace` has no field for the requesting
state. Trace messages are also built when tracing is enabled but the handler
is null. Add the field, and skip building the message without a handler.

## Image size checks

- **Layers:** image
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`Image` and `ImageView` don't check their sizes or regions, so a negative
size, or one whose pixel count doesn't fit in an `int`, goes unnoticed. Check
them where they are made, as `gb_render`'s texture loading now does, with
tests.

## Binding compatibility by type key

- **Layers:** render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`Binding::IsCompatible()` compares constants types by pointer, while
`Binding::operator==` compares their `TypeKey`, so two bindings can be equal
but not compatible. Compare by `TypeKey` in both, with a test.

## size_t in public APIs

- **Layers:** job, render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

A few public APIs still use `size_t`, against the Game Bits style:
`JobDataHandle`, and the sizes `Texture` and `TextureArray` take.
`TextureArray::Get()` also returns pixels through a vector taken by reference
rather than a pointer. Changing them breaks callers, which each project fixes
as it updates.

## ImGui contexts made outside ImGuiInstance

- **Layers:** imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`ImGuiInstance::GetActive()` assumes any current ImGui context belongs to an
`ImGuiInstance`, as it reads the instance from the context's
`ImGuiIO::UserData`. So a
context made directly with `ImGui::CreateContext()` must not be current while
ImGui uses its file functions, which find the instance this way. Mark the
contexts an instance makes, so `GetActive()` returns null for any other.

## Repeatable include check

- **Layers:** all
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

The code audit found unused includes with clang-tidy's `misc-include-cleaner`,
but nothing in the tree runs it again, so unused includes, bad include guards,
and headers missing their `*_types.h` creep back. It also reports about 40
includes that are needed (umbrella headers such as `<windows.h>` and
`glm/glm.hpp`, `<new>` for placement new, headers needed to destroy a
`std::unique_ptr`, and each `*_types.h`). Decide whether to add:
- A `.clang-tidy` with the check (and `IgnoreHeaders` for the umbrella
  headers), run by a script or a CMake option that sets
  `CMAKE_CXX_CLANG_TIDY`. Running clang-tidy over MSVC compile commands needs
  `-D_CRT_USE_BUILTIN_OFFSETOF`.
- `// IWYU pragma: keep` on the includes it reports wrongly.
- A script that checks include guards and the `*_types.h` rule.
