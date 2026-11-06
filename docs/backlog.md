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

## Default texture for unset bindings

- **Layers:** render, render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md),
  [Volume textures and storage buffers](worklog/volume_textures.md)

Drawing with a texture, texture array, volume texture, or storage buffer
binding that was never set crashes in `VulkanBindingData::OnRender()`.
Skipping the binding would leave its descriptor unwritten or pointing at a
deleted image or buffer, which is worse. Give the backend a default for each
binding type (one opaque pixel for textures and texture arrays, one texel for
volumes, and a small buffer of zeros for storage buffers) that every binding
starts with and falls back to, and check it by hand in `examples/render-check`
with a material whose bindings are never set.

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

## Optional commas in text configs

- **Layers:** config
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** OZ-3 *Ozzy machine*
- **Background:** the parser programs in `gb/config/text_config.cc`

Text configs separate map entries and array values with commas, as JSON does,
including the entries of a rootless file, and reject a comma after the last
one. Config files written by hand trip over both: a rootless file reads
naturally as one entry per line with no commas, and a trailing comma makes
lines easy to reorder or comment out. Let reading take a trailing comma, and
entries with no comma between them, in the Game Bits format (not
`kJsonTextConfig`). Check first that no text becomes ambiguous without commas,
such as `[1 -2]`. Writing keeps its commas.

## Shaders compiled by the build

- **Layers:** CMake, examples
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** `gb_embed_data` in `CMake/GameBitsTargetCommands.cmake`; the
  SPIR-V pasted into `examples/render-check/*_shaders.h`

Shaders are compiled to SPIR-V by hand (`glslc -mfmt=c`) and the output pasted
into headers, so a shader edit is easy to leave uncompiled. Compile them in the
build instead, with `glslc` from the Vulkan SDK, and embed the result with
`gb_embed_data`, whose data is aligned for reading as `uint32_t` words. This
adds `glslc` to the build, which only the Vulkan SDK provides, so a target
without shaders must not need it.

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

## Main-thread busy runs

- **Layers:** profile
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Profiled jobs](worklog/profiled_jobs.md)

A main Profiler with a thread counts its thread's busy time as its frames'
time, while a joined thread's is the time its outermost timed points ran. That
is the one remaining reason such a main Profiler's frames can't span a fiber
switch, and its joined Profilers can't time frames: a frame that waited would
show the main thread busy during the wait, and twice if it ended on a joined
thread. Every other part of a frame (its start and end ticks, the in-frame
flag, its count of timed points, and recording it under the frame mutex) is
already the same for both kinds of main Profiler. Record the main thread's
outermost runs as a joined thread's are, gathered with the others at each
frame's start and end, and drop the checks on what a frame can do, so
`Options::threadless` only says whether the main Profiler has a thread. The
main thread's busy time then leaves out the time between its points, which
changes the threads table of every program with a main Profiler.

## Point slots reset by count

- **Layers:** profile
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Profiled jobs](worklog/profiled_jobs.md)

A main Profiler's `Reset()` clears what its joined Profilers recorded by
baselines, as only a joined Profiler's own thread writes its slots: it copies
each one's totals as it resets (`JoinedThread::baseline`), and every gather
subtracts them. A job point's stats instead hold the count of the reset they
are for, the first record after a reset clears them, and readers ignore them
until then, as a slot's frame part already works with its frame number. Do the
same for slots, removing the baselines and the subtraction in every gather, at
the cost of a compare as each point is recorded. Measure a timed point before
and after.

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

## Job priorities and cancellation

- **Layers:** job
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Requested by:** Terra Diem *Scheduler for derived work*
- **Background:** Terra Diem's world parallelism assessment
  (`docs/tech/parallelism-assessment.md` there)

`FiberJobSystem` takes new jobs from one queue (`pending_jobs_`), roughly first
come, first served, and once `Run()` accepts a job, it will run. A program
with work of differing urgency (what the camera sees now against what it may
see later, or a frame's work against background loading) can't have the urgent
work run first, and can't take back work that's no longer wanted. Add:
- **A few priority bands**, chosen when a job is queued, with job threads
  taking work from the highest band that has some. A job that waits and resumes
  keeps its band. Decide whether bands are strict, or weighted so low bands
  can't starve, and say which in the contract.
- **Cancellation** of queued jobs, cheap and from any thread: a cancelled job
  never runs, and its job counter is still decremented, so waiters aren't left
  hanging. A running job isn't interrupted, but can check whether it has been
  cancelled.
- The same in `FakeJobSystem`, so tests can check order and cancellation.

Moving a queued job between bands isn't needed: a program whose priorities
change continuously (with the camera, say) can keep its own ordered queue and
hand the job system work a band at a time as threads free up. Test the order
of jobs across bands with one job thread, starvation under the chosen rule,
and that cancelled jobs never run while their counters still complete.

Terra Diem would queue work for the chunks near the camera and in view in a
higher band than for those farther away or behind it, and cancel work for
chunks that leave range.

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

## Typed values in gb_base

- **Layers:** base, cli, pilot
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** *Scripted runs of graphical programs*
- **Background:** [Scripted runs](worklog/scripted_runs.md)

`gb_cli` keeps each value given on the command line as
`internal::CliValue`, a `std::variant<bool, std::string_view, int64_t,
double>`, and `gb_pilot` passes a command's values as the text written in
the script (`absl::Span<const std::string>`), so each command converts them
itself. The script grammar already tells integers, identifiers, and strings
apart. A small value type in `gb_base` (a variant of the scalar types, with
conversions that fail cleanly) could serve both, without either depending on
`gb_parse`'s `Token` or `gb_config`'s `Config`. Changing `Pilot::Command`'s
values breaks every program's commands, which each project fixes as it
updates.

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

## Saving textures over 2 GiB

- **Layers:** render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:**
  [Volume textures and storage buffers](worklog/volume_textures.md)

`SaveTextureArrayChunk()` and `SaveVolumeTextureChunk()` put every texel in
one FlatBuffers vector, but FlatBuffers buffers are at most 2^31-1 bytes. The
largest texture array (`kMaxTextureArrayPixels`) or volume
(`kMaxVolumeTextureTexels`) in a 4-byte format is 2 GiB, so saving it asserts
in Debug and writes a corrupt size in Release. Either fail such a save with an
error, or split the texels across several chunks, and test it with the test
backend at the limit.

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

## Pilot items behind modal popups

- **Layers:** imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

With a pilot, `ImGuiInstance` only reports an item where a click at its center
reaches it, but it doesn't account for modal popups. While one is open, ImGui
ignores clicks outside it, yet an item behind it is still found, so a script's
click on it does nothing, rather than the script failing to find it. Leave out
items outside the open modal's window and its children, as ImGui's own hover
test does (`GetTopMostPopupModal()`).

## Pilot items behind other ImGui instances

- **Layers:** imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

With a pilot, `ImGuiInstance` leaves out items behind another of its own
windows, but only looks at its own context's windows. When two instances share
a render system and one draws on top of the other, an item covered by the
other instance's window is still found, though a click reaches the window on
top. Order the instances by their scene order, and leave out an item that a
window of a later instance covers.

## Pilot drags

- **Layers:** pilot, pilot_script
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

A pilot script holds a key or mouse button only for the length of one step
(`key w for 20`, `click "OK" for 30`), so the mouse can't move while a button
is held, and one key can't be held while others are pressed. Steps that press
and release separately (`press left "Extra rectangles"`, `move 400 300`,
`release left`) would allow drags, such as an ImGui slider or a window, and
keys held across other steps, such as shift. As with `key` and `click`, each
release needs a frame with no other input, for programs that poll input.

## Pilot clicks on scrolled out items

- **Layers:** imgui
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

With a pilot, `ImGuiInstance` only finds items that aren't clipped, so an item
scrolled out of its window, or below the edge of the screen, can't be clicked.
render-check's scripts collapse the sections above the one they check to bring
it into the window, which ties each script to the panel's layout. Scroll the
window to an item a script names, as ImGui's own test engine does. ImGui only
gives an item's label for items that aren't clipped, so finding a clipped one
by its label needs another way to learn it.

## OS file system for command lines

- **Layers:** cli
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

A program whose command line takes paths (any using `CreateCliPilot()`, and
OZ-3's assembler and simulator) builds the same file system in `main()`: an
OS-root `LocalFileProtocol`, registered as the default protocol, in the context
given to `RunCommandLine()`. A program that forgets the default protocol fails
only when a path without one is given. Add a `gb_cli` function that builds it,
or a `RunCommandLine()` that does, beside `GetMainArgs()`. render-check and
block-world also repeat the start of their command's run function: creating the
pilot with `CreateCliPilot()`, failing if it fails, and passing it to the game
in a context whose parent is the command's.

## Block-world camera turning in scripts

- **Layers:** block-world
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

Block-world turns the camera while the right mouse button is held, using SDL's
relative mouse mode and moving the cursor back on release
(`SDL_GetGlobalMouseState()` and `SDL_WarpMouseGlobal()`). These act on the
real cursor, so a script that right-clicks in play would capture and move the
user's mouse. Under a pilot, turn the camera from the mouse motion events alone,
without relative mode or moving the cursor.

## Pilot scripts with a byte order mark

- **Layers:** pilot_script
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

`ReadPilotScript()` fails on a script that starts with a UTF-8 byte order mark,
as Windows tools often write one (PowerShell 5.1's `Out-File -Encoding utf8`,
and Visual Studio's "UTF-8 with signature"). The error is at line 1, column 1,
which doesn't say why. Skip a leading byte order mark, as `gb_file` leaves it in
the text it reads.

## Pilot clicks on a scaled display

- **Layers:** pilot_sdl, imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

A pilot's mouse positions are in window pixels, the pixels of its saved frames,
so a click at a point seen in a frame lands on what is drawn there. That is
confirmed on a display at 100%, where SDL's window coordinates and the frame's
pixels are the same. Check it on a display scaled above 100% (render-check's
scripts click by label, so they show it), and if SDL's coordinates differ
there, convert between them in `PilotSdl`.

## Pilot window resizing and polled mouse

- **Layers:** pilot, pilot_script, pilot_sdl
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

No pilot step resizes the window, so render-check's resizing checks are still
done by hand. A `resize <width> <height>` step would set the window's size
through `PilotSdl`, and the frames after it would be saved at the new size.
Separately, a program that polls the mouse (`SDL_GetMouseState()`) rather than
reading events still sees the real one, as SDL keeps that state itself; only
ImGui, through `ImGuiInstance`, reads the pilot's mouse. Programs read mouse
input from events until something needs polling.

## Pilot steps from a running session

- **Layers:** pilot, pilot_script
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

A script is fixed when a run starts, so a session that wants to look at a frame
before choosing the next step runs the script again with the step added, which
works as runs are repeatable. Steps read from a pipe, with results written
back, would let a session drive one run step by step. `StepPilot` already takes
steps from anywhere, and more while it runs, so a pipe is another source of
steps. It needs an option to wait for more steps, rather than being done when
they run out.

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
