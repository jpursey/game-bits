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

## Scripted runs of graphical programs

- **Layers:** render, render_vulkan, imgui, imgui_sdl, game, and possibly a new
  library for the steps themselves
- **Size:** large
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** `tools/run-program.ps1`, which this would mostly replace

Every graphical program built on Game Bits, the examples included, should be
able to run a script of steps by itself, so a session can check its drawing
and its GUI without the user. `tools/run-program.ps1` does this from outside
the program, posting key messages to its window and capturing it with
`PrintWindow()`, which leaves it unable to:
- **Use the mouse.** ImGui's SDL backend reads the mouse position from the real
  cursor (`SDL_GetGlobalMouseState()`), so a posted click lands wherever the
  user's cursor is, and no step can press an ImGui button or check box.
- **Count frames.** Steps wait in seconds, so a slow frame or a slow start can
  make a screenshot catch the wrong moment, and runs aren't repeatable.
- **Know what the program is doing.** A step can't wait until something has
  loaded, read a value back, or name an action ("press *Get layer 1*") rather
  than a place on screen.
- **Capture an exact frame.** A screenshot is whatever the window shows when
  it is taken, at the display's scaling, and needs the window on screen and
  not minimized.

Drive it from inside the program instead:
- Steps come from a file named on the command line (through `gb_cli`), or from
  a pipe that a session writes to and reads results from, which also lets a
  session look at one screenshot before choosing the next step. The design
  picks one or both.
- Steps are counted in frames, and include: key and mouse input, given as
  input events ahead of the program's own (so ImGui and the program both see
  them, with ImGui's mouse position taken from them rather than the real
  cursor); waiting until a condition the program names holds; running a
  command the program names; and saving a frame.
- A frame is saved by the render backend reading back the image it presented,
  written as a PNG with `gb_image`, so it is exact and works with the window
  hidden.
- A program opts in with little code, ideally through `gb_game`'s loop and
  `gb_imgui`, so programs that use them get it for free. A program adds its own
  commands and conditions where a step needs them.
- A run ends the program normally, and reports failure (a step that fails, an
  error logged, or a wait that times out) through its exit code and log, so
  `run-program.ps1` (or what replaces it) only launches it and collects what it
  saved.

Check it by converting `examples/render-check`'s hand checks to scripts that
press its buttons and save frames, and reading the frames.

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

## Profile timelines

- **Layers:** profile, job
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Multithreaded profiles*, *Fiber-aware profiling*,
  *Profiled jobs*
- **Requested by:** Terra Diem *Profile the world's jobs*
- **Background:** [Multithreaded profiles](worklog/multithreaded_profiles.md),
  which leaves timelines out

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

## Joined Profilers' own cost

- **Layers:** profile
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Multithreaded profiles](worklog/multithreaded_profiles.md)

A main Profiler's cost per frame (`FrameSummary::profiler_cost`) and the
"profiler" figure on each frame breakdown count only the main thread's timed
points, though the report and breakdowns now include every joined thread's.
Count the timed points that end in each frame on every joined Profiler too
(each measures its own point cost), and show the other threads' cost beside the
main thread's, as the self time by kind does. The budget is a fraction of the
main thread's frame, so it should stay against the main thread's cost alone.

## Profile slots on cache lines

- **Layers:** profile
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Fiber-aware profiling](worklog/fiber_aware_profiling.md)

Each point's `Profiler::Slot` is 72 bytes, packed in one array, so the 56 bytes
every timed point writes (its totals, wait, and frame part) cross a cache line
boundary for many points, depending on the point's index. The value fields,
which only value points use, make up the rest. Measure a scope in a loop over
points at several indices to see what crossing costs, and if it matters, move
the value fields to an array of their own and align each slot to a cache line,
so every timed point touches one line.

## Cheaper job names

- **Layers:** job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *Profiled jobs*
- **Background:** [Profiled jobs](worklog/profiled_jobs.md)

Keeping job names (`FiberJobSystem::kKeySetFiberNames`, on by default in Debug)
costs every job a copy of its name into a `std::string`, an insert into and
erase from a set of running jobs under a mutex every job thread shares (for
naming the jobs that keep the job system from shutting down), and two renames
of its thread, a system call each, as it starts and ends. Measure what that
costs a short job, then cut what matters:
- Keep each job's name as the job point *Profiled jobs* finds for it, rather
  than a copy, as a profiled job already does.
- Count running jobs by point in an atomic count each, rather than a set, and
  name each point with its count in the shutdown error.
- Rename the thread only when a job starts or resumes, or not at all, if a
  debugger can show the job some other way.

Without profiling, a job's point would be found by name for each run, which
*Profiled jobs* does through a thread-local cache. Names built at runtime, one
per piece of work, would then reach the limit of 1024 points.

## Volume textures and storage buffers

- **Layers:** render, render_vulkan
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Requested by:** Terra Diem *Chunk light volumes*
- **Background:** Terra Diem's meshing assessment
  (`docs/tech/meshing-assessment.md` there)

Shaders can read only constants, 2D textures, and texture arrays
(`BindingType`), and every texture is RGBA at 8 bits per channel. Data that
varies through a volume, or is an array too large or too variable for a
constants type, has nowhere to go. Add:
- **3D textures**, with width, height, and depth, the usual sampler options
  (filtering, wrap or clamp), and updates of a box at a time, so a program can
  update part of a large volume or keep many small ones, for every
  `DataVolatility`.
- **Texel formats** for every kind of texture: at least one, two, and four
  8-bit channels, and a packed 16-bit format of four 4-bit channels, besides
  today's RGBA8. Small volumes are often one or two bytes per texel. The
  backend reports which formats it can filter.
- **Storage buffers:** arrays of structured data that shaders read (writing
  can come later), bound in any binding set like constants, but sized when
  created rather than by a constants type, and updated in ranges. Data for
  each of thousands of draws (see *Shared mesh buffers and indirect draws*)
  lives in one, indexed in the shader.
- A binding type for each, checked against a shader's bindings as today's are,
  with support in the test backend so `gb_render`'s tests cover them.

Check by hand in `examples/render-check`: a 3D texture sampled with filtering
by a quad that moves through it, and a storage buffer giving each of several
draws its color.

Terra Diem would keep a small volume of light for each chunk of its world
(18^3 texels of one or two bytes), sample it with filtering in its block
shaders, and update it when light changes, instead of rebuilding meshes with
light in their vertices.

## Shared mesh buffers and indirect draws

- **Layers:** render, render_vulkan
- **Size:** large
- **Feature workflow:** yes
- **Depends on:** *Volume textures and storage buffers*
- **Requested by:** Terra Diem *Chunk meshes in shared buffers*
- **Background:** Terra Diem's meshing assessment
  (`docs/tech/meshing-assessment.md` there)

Every `Mesh` owns a vertex buffer and an index buffer (two GPU allocations),
its indices are 16 bits (`kMaxMeshVertices` is 65,535), and every draw is a
command of its own, with instance binding data of its own. A voxel world draws
thousands of small meshes (Terra Diem had 4,212 chunk meshes within its default
view distance) and rebuilds them often, from job threads, so the cost per mesh
and per draw dominates. Add:
- **Mesh pools:** large vertex and index buffers for one vertex type, from which
  meshes allocate and free ranges. Allocating is cheap and safe from any
  thread, with no lock the render loop also needs, and uploads are batched
  through staging buffers. A pool reports how fragmented it is, and can grow
  or compact.
- **32-bit indices**, as an option per pool, so large meshes needn't be split.
- **Drawing ranges** of a mesh (first index, count, and base vertex), so a mesh
  can hold groups (by direction, or material) drawn or skipped separately.
  `DrawPartial()` does part of this.
- **Indirect draws:** draws whose ranges come from a buffer, many in one call
  (multi-draw), each able to find its own data in a storage buffer by its draw
  index. A program fills the buffer, or later a compute pass.
- A fallback in the backend for GPUs without multi-draw, and the test backend
  recording what was drawn.

Check in `examples/block-world` by drawing all of its chunks from one pool with
an indirect draw, and compare its frame time with drawing them one by one.

Terra Diem would allocate each chunk's mesh from a pool, as ranges grouped by
face direction, and draw every visible chunk in a few indirect draws, skipping
the directions that face away from the camera.

## Render passes and targets

- **Layers:** render, render_vulkan
- **Size:** large
- **Feature workflow:** yes
- **Depends on:** nothing
- **Requested by:** Terra Diem *Water refraction and reflections*, *Sun and
  moon shadows*
- **Background:** Terra Diem's meshing assessment
  (`docs/tech/meshing-assessment.md` there)

Everything is drawn in one render pass, to the window, with scenes in
`scene_order`. Effects that read what was already drawn (refraction and
reflections on water, fog by depth, soft particles) or draw from somewhere
else (shadow maps, mirrors, maps) need more than one pass. Add:
- **Render targets:** color textures, in the formats of *Volume textures and
  storage buffers* if that's done, and depth textures, sized in pixels or
  relative to the frame, following the window as it resizes.
- **Passes:** each draws scenes into a set of targets (or the frame), clearing
  them or keeping what's there, with its own scene constants (a camera of its
  own). A later pass can sample an earlier pass's color and depth as textures.
  Passes are declared up front, in order, so the backend can place the
  synchronization between them: a small frame graph rather than commands
  issued as they come.
- **Depth-only passes**, for shadow maps.
- The test backend recording passes, so tests can check their order and what
  each reads.

Check by hand in `examples/render-check`: a scene drawn to a target, then shown
distorted, beside its depth shown as gray.

Terra Diem would draw its opaque world first, then its water, sampling the
world's color (offset by the water's moving normals, to distort what's below)
and depth (for color by depth, and foam at the shore), with reflections in
screen space. Later, optionally, it would draw shadow maps.

## Instanced draws

- **Layers:** render, render_vulkan
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** Terra Diem *Detail blocks*
- **Background:** none

Drawing one mesh many times (grass, flowers, rocks, props) takes a draw per
copy today, each with instance binding data of its own. Add draws of a mesh
many times in one call, with per-instance attributes from a second vertex
stream (a vertex type read once per instance), so a program can draw thousands
of small decorations at once. Once *Shared mesh buffers and indirect draws* is
done, its indirect draws take an instance count too. Check by hand in
`examples/render-check` with a field of instanced quads.

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

## size_t in public APIs

- **Layers:** job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`JobDataHandle` still uses `size_t` in its public API, against the Game Bits
style. Changing it breaks callers, which each project fixes as it updates.

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

## FakeJobSystem waits that depend on the waiting job

- **Layers:** job
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** none

`FakeJobSystem::Wait()` runs queued jobs inside the call, on the waiting job's
thread, so the waiting job can't continue until each of them ends. A job it
runs that waits on work the waiting job does after its own `Wait()` makes the
fake fail, although `JobSystem` allows it: job A waits on counter 1 and then
runs a job for counter 2, job B waits on counter 2, and A's `Wait()` runs B
before counter 1 is done. Let a waiting job continue as soon as its counter is
done (for example, by giving each job a thread of its own that blocks in
`Wait()`, while still running one job at a time), and test this case.

## Shared job teardown in JobSystem

- **Layers:** job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** none

`FiberJobSystem` and `FakeJobSystem` each count jobs on their counters and end
a job the way `JobSystem` requires (job data freed, then the context, before
any waiting job resumes), so the rule is written out in each, and each is
among `JobInternal`'s friends to do it. Give `JobSystem` protected functions
that start and end a job's counting and free its data and context in order, so
the rule lives in one place and the job systems no longer need to be friends.

## Threads across processor groups

- **Layers:** thread, job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *Core-aware job threads*
- **Background:** [Core-aware job threads](worklog/core_aware_job_threads.md)

An affinity is a mask of hardware threads in one processor group of up to 64,
so `GetHardwareThreadAffinities()` and `GetCpuTopology()` cover only the group
the process runs in. On Windows 11, a process on a machine with more than 64
hardware threads can have threads in more than one group, and then
`GetProcessAffinityMask()` returns zero: there are no affinities,
`GetMaxConcurrency()` returns 1 (so a default `FiberJobSystem` runs one job
thread), and the topology is a single core that can't be pinned. Give
affinities a group (a small struct rather than a `uint64_t`, set with
`SetThreadGroupAffinity()`), so the topology covers every group. This changes
`CreateThread()` and everything else that takes an affinity. `FiberJobSystem`
also runs at most `kMaxThreadCount` (64) job threads, which its 64-bit mask of
idle job threads relies on. No consumer CPU has more than 64 hardware threads;
workstation ones do.

## Favored cores

- **Layers:** thread
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *Core-aware job threads*
- **Background:** [Core-aware job threads](worklog/core_aware_job_threads.md)

Within a class of core, `PlaceThreads()` takes cores in processor order, so the
fastest core it leaves for a main loop is the first. On an i9-14900KF,
`GetSystemCpuSetInformation()` reports a `SchedulingClass` of 2 for two
performance cores (8 and 10, the ones Intel lets boost highest) and 1 for the
other six. The field is in the Windows SDK headers but undocumented. Check what
it means, and what other CPUs (AMD's in particular) report, and if it is
reliable, order cores by it within a class.

## UTF-8 console output on Windows

- **Layers:** gb_cli
- **Size:** small
- **Feature workflow:** no
- **Depends on:** Command line library
- **Background:** [Command line library](worklog/command_line_library.md)

`gb_cli`'s default writer writes UTF-8 to `stdout` and `stderr`, but a Windows
console shows text in its own output code page (often the OEM one, such as
437), which the manifest that makes the process's code page UTF-8 doesn't
change. Help or an error quoting a path with characters outside ASCII is
garbled there, though it is correct when redirected to a file. Have the
default writer write UTF-8 a console shows correctly, behind a `win_` / `gen_`
split: `SetConsoleOutputCP(CP_UTF8)`, or `WriteConsoleW()` when the handle is
a console. Check by hand first that the console garbles it.

## Canonical local paths

- **Layers:** gb_file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** none
- **Background:** `FileProtocol::GetCanonicalPath()` in `file_protocol.h`

`LocalFileProtocol::GetCanonicalPath()` only maps a drive's paths to the
`"file"` name's paths, so two paths for the same folder that differ in case
(Windows ignores it) or go through a link have different canonical paths.
`FileSystem::CopyFolder()` then misses a folder copied into itself
(`C:\src` into `c:\SRC\sub`), which `std::filesystem::copy()` recurses into.
Have it ask the file system, with `std::filesystem::weakly_canonical()`,
which resolves the case and links of the part of a path that exists. Folding
case in the path itself can't match Windows: NTFS compares names with an
uppercase table of its own, and a folder can be made case-sensitive. A link
under a rooted protocol that leads outside its root falls back to the path
as written.
