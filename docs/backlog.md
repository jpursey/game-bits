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

## Web demos

- **Layers:** build, CI, examples
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *WebGPU render backend*, *Files on the web*
- **Background:** [Web support](designs/web_support.md)

Show Game Bits running without building it. Host block-world and render-check
from the repository with GitHub Pages, built and published by a workflow on
each push to `main`, and linked from README.md. Also package a program as a
Claude artifact, to share by its link: a page of at most 16 MB with each file
beside it at most 15 MB, built without threads, as an artifact isn't
cross-origin isolated. Block-world's assets and a release build's size decide
whether it fits. Block-world's shaders are compiled by a hand-run Windows
script, and it has never been built with clang, so this compiles them in the
build (the block-world half of *Block-world and ImGui shaders compiled by the
build*) and fixes what clang rejects (most of *Block-world on Linux*).

## Contracts own their contexts

- **Layers:** base, resource, render
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** the `Context*` constructors and `Assign` of
  `ValidatedContext` and `ContextContract` in `src/gb/base/validated_context.h`

A `ValidatedContext` or `ContextContract` made from a raw `Context*` doesn't own
it, so one kept after the context is destroyed points at a destroyed context,
and nothing checks that the caller kept it alive. Every other way to make one
owns or shares its context. Remove the raw `Context*` forms, so every
`ValidatedContext` keeps its context alive. That also removes the special case
where assigning the raw context it already holds keeps its ownership.

The raw form is used to lend a context for the length of a call:
- The resource layer passes loaders and writers a `Context*`
  (`ResourceManager`, `ResourceSystem`, `ResourceFileReader`, and
  `ResourceFileWriter`), which `RenderSystem` makes contracts from. These take
  a form that shares ownership instead, such as a `ValidatedContext&`.
- About 50 test call sites, mostly `Create(&context)`. Tests that read the
  context's outputs afterwards keep a `std::shared_ptr<Context>` to it.

This breaks the public API for any project that makes a contract from a
`Context*`.

## clang-cl build

- **Layers:** job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md),
  [Linux and Clang](worklog/linux_clang.md)

Game Bits is meant to build with clang-cl as well as MSVC (CMakeSettings.json
has `x64-Clang-Debug` and `x64-Clang-Release`). Linux and Clang's CL1 fixed what
clang rejects in portable code, as clang 18 on Linux found it, but clang-cl
also builds the Windows-only files and with more warnings:
- `fiber_job_system_test.cc` has an unused lambda capture, an error with
  warnings as errors.

This was found by clang-tidy, which parses with clang; there may be more in the
`win_` files once clang-cl builds them. Building needs Visual Studio's "C++
Clang tools for Windows" component, as the MSVC standard library rejects older
clang versions. Fix them, build and test both clang configurations, and
consider building one in CI so it stays working.

## ImGui update

- **Layers:** imgui, examples
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** ImGui's changelog (`docs/CHANGELOG.txt` in the ImGui
  repository); `src/gb/imgui/CMakeLists.txt`

ImGui's submodule is 1.84.1, from 2021. Update it to the current release. Its
API has changed a lot since: keyboard and mouse input in `ImGuiIO`, the SDL
backend (now `imgui_impl_sdl2`, beside `imgui_impl_sdl3`), and functions it has
since removed, which matter as `gb_imgui` defines
`IMGUI_DISABLE_OBSOLETE_FUNCTIONS`. ImGui is used by `gb_imgui`, `gb_imgui_sdl`,
block-world, and render-check, which change with it.

1.84 calls `memset` and `memcpy` on its own types that aren't trivially
copyable, which newer clang's `-Wnontrivial-memcall` flags in its headers, so
`gb_imgui` turns that warning off for every target that links it. Remove that
if the new version no longer needs it.

This changes the ImGui API that projects using `gb_imgui` see, so it needs a
follow-up in each of them.

## SDL3

- **Layers:** build, sdl, imgui, pilot, render, examples, templates
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** ImGui update
- **Background:** SDL's migration guide (`docs/README-migration.md` in the SDL
  repository)

SDL3 is where SDL's development is now (3.4 at the time of writing), and SDL2
only gets fixes. Moving to it changes the API: many functions and events are
renamed, and some behavior differs (window and display handling, and how
events report keyboard and mouse state). Game Bits uses SDL in `gb_sdl`,
`gb_pilot_sdl`, `gb_render_vulkan_sdl`, and `gb_imgui_sdl`, and directly in
block-world, render-check, and `templates/sdl`.

ImGui's submodule only has the SDL2 backend (`imgui_impl_sdl.cpp`), so this
needs ImGui updated first, to a version with `imgui_impl_sdl3`. Plan the order
of the libraries in the worklog.

This changes what projects using `gb_sdl` see wherever they call SDL directly,
so it needs a follow-up in each of them.

## Linux threads and fibers

- **Layers:** thread, job
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Linux and Clang](worklog/linux_clang.md); `gb/thread/thread.h`
  and its `win_` and `gen_` files

On Linux, threads and fibers come from the `gen_` implementations, which do less
than Windows': fibers are unsupported, so the fiber job system isn't
available, threads aren't named, and the CPU topology isn't known, so threads
can't be pinned to cores. Linux can do all of them (`ucontext` or a small
context switch for fibers, `pthread_setname_np`, `/sys/devices/system/cpu` and
`pthread_setaffinity_np`). The workflow's `posix_` prefix is for what POSIX
alone provides, so these, which are Linux's own, go in `linux_` files, as
`gb/base`'s process memory does, with the build choosing them on Linux, and
their tests named the same way.

## Block-world and ImGui shaders compiled by the build

- **Layers:** CI, imgui, examples
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** `gb_embed_shader` in `CMake/GameBitsTargetCommands.cmake`,
  as render-check uses it; `assets/shaders/build-shaders.bat`; the shaders
  pasted into `gb/imgui/gen_imgui_shaders.cc` and `web_imgui_shaders.cc`

Block-world's shaders are compiled by a hand-run script into `.spv` files it
loads at run time, so a fresh checkout or worktree has none until the script is
run, and an edited shader is easy to leave stale. gb_imgui's are compiled by
hand, their SPIR-V pasted into `gen_imgui_shaders.cc` and their WGSL into
`web_imgui_shaders.cc`. Compile both with `gb_embed_shader`, as render-check
does, and remove the script.

CI builds block-world, and every project that links gb_imgui (including its
unit tests, which draw with the test backend) would then need `glslc`. Every CI
job already has it, as each installs the Vulkan SDK, `glslc` included, with
`.github/actions/vulkan-sdk` ([Graphics on Linux](worklog/linux_graphics.md)).

## Block-world on Linux

- **Layers:** CI, examples
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *Block-world and ImGui shaders compiled by the build*
- **Background:** [Graphics on Linux](worklog/linux_graphics.md)

Block-world builds and runs only on Windows, where everything else built on
Game Bits' graphics libraries also runs on Linux. Its code has nothing
Windows-specific, but its shaders are compiled by a Windows batch file, and it
has never been built on Linux. Build it there, add it to CI on Linux beside
render-check (`build-render-check-linux`), and update CLAUDE.md and README.md,
which say it doesn't build there. On lavapipe it is far too slow to play, but
its scripts can still run in CI, checking that it draws with no validation
errors.

## Devcontainers for projects built on Game Bits

- **Layers:** devcontainer
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Graphics on Linux](worklog/linux_graphics.md)'s
  Devcontainer; `.devcontainer/`

Game Bits' devcontainer builds, tests, runs, and debugs Game Bits itself, but a
project built on Game Bits also needs Game Bits checked out beside it, found
through `GB_DIR`, which a Codespace of the project doesn't have. Make Game
Bits' setup reusable by such a project, so its own devcontainer is small: an
image or devcontainer feature with the packages, the Vulkan SDK, and the
desktop's settings; a way to clone Game Bits beside the project (a Codespace
can be given access to a second repository); and VS Code settings for the
project's CMake projects. Each project then adds its own devcontainer, as an
item in its own backlog.

## Jobs on the web

- **Layers:** build, thread, job
- **Size:** large
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Web support](designs/web_support.md), which leaves it
  out; *Linux threads and fibers*

`gb_job` isn't built for the web: the only job system, `FiberJobSystem`, needs
fibers, which the web doesn't have, and its semaphore doesn't link without
threads. So a program that runs jobs can't run there. Neither example does.
Emscripten has fibers of its own, built on what JSPI is built on, for a build
with threads. A build without threads has neither, so jobs there would have to
run on the thread that waits for them. Design both before starting: a program
written against `JobSystem` should run in either build.

## Profiles on the web

- **Layers:** build, thread, profile
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Web support](designs/web_support.md), which leaves it
  out; `gb/thread/cpu_ticks.h`

`gb_profile` isn't built for the web, and neither are `gb_thread`'s
`cpu_ticks` files: they read the x86 timestamp counter, inline with
`__rdtsc()`, and its rate from `cpuid`, which no other processor has. Give
`ReadCpuTicks()` a second implementation on a steady clock, counting
nanoseconds, chosen by processor, and build both libraries for the web with
it. That also serves ARM, on any platform. In a browser the clock is coarsened
to 5 microseconds on a cross-origin isolated page and 100 otherwise, so
timings there are only that fine. About 30 of `gb_profile_test`'s tests start
threads, and are skipped in a build without them.

## Threads in the browser

- **Layers:** build, base, examples
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Web support](designs/web_support.md), which leaves it
  out

A web build can have threads (`GB_WEB_THREADS`), but only its tests under Node
use them: a program in a browser is built and checked without. Make a
threaded program work there.
- A thread started from the browser's main thread doesn't run until that
  thread returns to the browser, unless a pool of them was made at startup
  (`-sPTHREAD_POOL_SIZE`), and waiting on the main thread spins. Decide the
  pool's size, and say what a program must not wait for.
- The page must be served with cross-origin isolation headers. GitHub Pages
  can't set them, but a service worker can add them, with one reload on a
  first visit. A Claude artifact can't have them at all.
- Run a threaded program's pilot scripts in the headless browser, from a
  server that sends the headers.
- `WebGpuBackend` only touches WebGPU on the browser's main thread, and its
  factory methods fail on any other ([WebGPU render
  backend](worklog/webgpu_render_backend.md)). Resources created on another
  thread need their WebGPU objects made on the main thread, such as at the
  next `EndFrame()`.

## Clamp to border on the web

- **Layers:** build, render
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** *WebGPU render backend*
- **Background:** [WebGPU render backend](worklog/webgpu_render_backend.md),
  The gaps

WebGPU has no clamp to border, so `WebGpuBackend` fails to create a texture
with `SamplerAddressMode::kClampBorder`. Nothing in Game Bits uses it, but a
shadow map sampled with a white border is a common use. Emulate it: sample
clamped to the edge, and replace the result with the border color where the
coordinates are outside the texture. The sampler's options aren't known when
the shader is converted, so the shader needs to be told per binding (such as
with a constant the backend adds), which makes this more than a conversion
step.

## Wireframe on the web

- **Layers:** render
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** *WebGPU render backend*
- **Background:** [WebGPU render backend](worklog/webgpu_render_backend.md),
  The gaps

WebGPU has no wireframe fill, so `WebGpuBackend` fails to create a pipeline
with `RasterMode::kLine`. Nothing in Game Bits uses it. Emulate it by drawing
a line list, with each mesh's triangles turned into their three edges in an
index buffer of their own, kept beside the triangles'. Mesh pools and indirect
draws would need the same for every range, which may be reason to leave them
failing.

## Multi-draw on the web

- **Layers:** render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *WebGPU render backend*
- **Background:** [WebGPU render backend](worklog/webgpu_render_backend.md),
  Mesh pools and indirect draws

`WebGpuBackend` draws each indirect draw with a call of its own. Dawn has a
`multi-draw-indirect` feature that draws many in one call, which Chromium
doesn't expose to pages yet (SwiftShader in headless Chromium 158 doesn't
report it). When browsers do, use it where the device has it, as
`VulkanBackend` uses `multiDrawIndirect`, and report it in
`IsMultiDrawSupported()`.

## Three-channel narrow vertex values on the web

- **Layers:** render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *WebGPU render backend*
- **Background:** [WebGPU render backend](worklog/webgpu_render_backend.md),
  Pipelines

WebGPU has no vertex formats of three 8 or 16-bit channels, so
`WebGpuBackend` fails to create a pipeline whose vertex type has a
`ShaderValue` such as `kI8Norm3` or `kU16Vec3`. Nothing in Game Bits uses
them. Where the value isn't the vertex's last, it can be read with the
four-channel format, whose extra channel the shader ignores; the last one
can't, as it would read past the vertex. Do that, and say what remains.

## Frame limits in a browser

- **Layers:** base, game
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *Programs in the browser*
- **Background:** [Programs in the browser](worklog/programs_in_the_browser.md)

In a page, the display paces `Game`'s frames, and `kKeyMaxFps` doesn't apply:
the loop's limit sleeps, and a sleep in a page without threads spins on the
browser's main thread, so a 60 Hz display ran at 43 to 60 frames a second
with nearly every frame spent spinning. A program on a 144 Hz display runs at
144. Limit frames there in whole display frames instead: wait for animation
frames until the next frame is due (within about half a display frame), so a
limit of 60 on a 144 Hz display runs every second or third display frame.
Check it by hand on a display faster than 60 Hz, as headless Chromium only
has 60.

## Asset packages over 15 MB

- **Layers:** build
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *Files on the web*
- **Background:** [Files on the web](worklog/files_on_the_web.md)

`gb_pack_assets()` packs a page's assets into one `<name>.data`, and only warns
when it is over 15 MB, which a Claude artifact allows for each file beside its
page. Split them into packages of at most 15 MB each instead, by running
Emscripten's file packager once for each from the build, with each package's
loader given to the page with `--pre-js`, so a page can have any amount. A
single file over 15 MB still can't be beside an artifact, so say what happens
to one (such as failing the build, or splitting the file and joining it as it
loads).

## Stacks of a hung page

- **Layers:** tools
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *Programs in the browser*
- **Background:** `tools/run-program.sh`, `tools/run-page.js`

When a program on Linux is still running at its timeout, `run-program.sh`
saves the stack of each of its threads, showing where it is stuck. A page that
times out saves nothing. Chrome's debugger can pause a page and give its call
stack, WebAssembly functions included, by name in a Debug build (the DevTools
protocol's `Debugger.pause`, through Playwright's CDP session). Save that
stack to `hang-stacks.txt` before ending the page.

## SDL tests in the browser

- **Layers:** build, sdl, pilot
- **Size:** small
- **Feature workflow:** no
- **Depends on:** *Programs in the browser*
- **Background:** [Web platform](worklog/web_platform.md), whose tests use
  SDL's dummy video driver under Node

SDL's tests run under Node with SDL's dummy video driver, so SDL's real web
driver (the canvas, resizing it, and its events) is only checked by running
`templates/sdl` in a headless browser. Also run `gb_sdl_test` and
`gb_pilot_sdl_test` as pages, from `ctest`, through `tools/run-program.sh`'s
page mode, in the web CI jobs. GoogleTest's `main()` runs without returning to
the browser, which these tests don't need.

## Saved files on the web

- **Layers:** file
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Web support](designs/web_support.md), which leaves it
  out; [Files on the web](worklog/files_on_the_web.md)

What a program in a browser writes is kept in memory, and gone when its page
is closed. Keep it in the browser's private file storage, so a saved game or
ImGui's window layout is there the next time. That storage is only fast from a
worker, and its other form answers later, where every write in `gb_file`
returns at once, so decide how a write reaches it.

## Abseil and GoogleTest update

- **Layers:** third_party, build
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Web platform](worklog/web_platform.md)

Abseil's and GoogleTest's submodules are from September 2024. Update both to
their current releases, and fix what changes for the code that uses them.
- Abseil's headers use Emscripten's old version macros, which Emscripten 7
  deprecates, so a web build turns `-Wno-deprecated-pragma` on for every
  target. Remove that once the headers no longer need it.
- The pinned Abseil can't symbolize a stack trace on the web, and says so once
  in every test program, as it needs an Emscripten option that no longer
  exists.

This changes the Abseil that projects built on Game Bits see, so it needs a
follow-up in each of them.

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

## Render asserts per render system

- **Layers:** render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Linux and Clang](worklog/linux_clang.md);
  `gb/render/render_assert.h`

Render asserts are one setting for the whole program
(`SetRenderAssertEnabled()`), and creating a `RenderSystem` overwrites it with
its own debug option. So a test or program that enables them must do so after
creating the render system, and creating a second one changes them for the
first. Make them part of each `RenderSystem` (set from its debug option, or
its own option), so nothing has to be ordered around its constructor.

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
of small decorations at once. Draws from a mesh pool take an instance count
too: `DrawList::Draw(const MeshDraw&)` asserts a `MeshDraw` has one instance
today, and indirect draws pass whatever count each `MeshDraw` holds. Check by
hand in `examples/render-check` with a field of instanced quads.

## Mesh pool compaction

- **Layers:** render, render_vulkan
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Shared mesh buffers and indirect
  draws](worklog/shared_mesh_buffers.md)

A `MeshPool` never moves a mesh, so creating and destroying meshes of many
sizes leaves its free space in ranges too small to use, and the pool grows
instead. `MeshPool::GetStats()` shows how fragmented it is, and a program can
rebuild a fragmented pool by recreating its meshes in a new one. Moving meshes
within the pool instead (on the GPU, between frames) would change the offsets
in every `MeshDraw` a program keeps, so the pool would need to tell the
program which meshes moved, or the draws would need to be built on the GPU
from the meshes' current offsets. Measure how fragmented a pool gets in real
use (such as a voxel world rebuilding its chunks) before choosing.

## Indirect draw counts from the GPU

- **Layers:** render, render_vulkan
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Shared mesh buffers and indirect
  draws](worklog/shared_mesh_buffers.md)

`DrawList::DrawIndirect()` draws a count of `MeshDraw` values the program
gives on the CPU. Culling on the GPU (a compute pass that writes only the
visible chunks' draws) needs the count to come from a buffer the GPU writes
too: `drawIndexedIndirectCount()` (Vulkan 1.2). That needs compute passes,
which Game Bits doesn't have yet, so it starts with them: a compute shader
type, binding storage buffers it writes, and the barriers between it and the
draws that read them.

## Shadow map comparison samplers

- **Layers:** render, render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Render passes and targets](worklog/render_passes.md)

A depth target is sampled as one channel of depth, so a shadow map's shader
compares depths itself, one texel at a time. A comparison sampler
(`sampler2DShadow`, `SamplerOptions` with a compare op) compares in the
sampler, with filtering between texels for softer edges at no extra cost.

## Multisampled depth without storing it

- **Layers:** render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Render passes and targets](worklog/render_passes.md)

A multisampled pass stores its multisampled depth, though nothing reads it
after its resolve, to work around NVIDIA's driver (610.88) leaving garbage in
the resolved depth at some sizes when it isn't stored. Storing it writes about
66 MB a frame at 8x and 1080p. With a newer driver, try `eDontCare` again
(render-check's `scripts/passes.txt` resizes to a size that showed it), and
make the image transient as color's is if it passes.

## Multisampled targets that are kept

- **Layers:** render, render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Render passes and targets](worklog/render_passes.md)

A multisampled pass clears its targets: keeping either fails, as only the
resolved targets last beyond the pass. Keeping them means keeping the
multisampled images too, loading them as the pass begins, so a later pass can
draw over an earlier one's multisampled world (such as decals, or a second
camera).

## Pipelines created before their first draw

- **Layers:** render, render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Render passes and targets](worklog/render_passes.md)

A material type's pipeline for a pass of a new layout (its formats and sample
count) is created the first time it draws there, a stall of a few
milliseconds. A way to name the passes a material type draws in, as it is
created, would create them up front, as the frame's is.

## Several color targets per pass

- **Layers:** render, render_vulkan
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** [Render passes and targets](worklog/render_passes.md)

A pass has at most one color target. Deferred shading, and writing normals or
motion beside color for later passes, need several, each a fragment shader
output, with blend state per target.

## Floating point render targets

- **Layers:** render, render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Render passes and targets](worklog/render_passes.md)

Color targets are 8 bits a channel, so a pass can't hold light brighter than
white for HDR, bloom, or tone mapping. A half-float format (`kRgba16f`) would
be a new `TextureFormat`, for render targets at least.

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

## Temp roots with read-only folders deleted

- **Layers:** file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Web platform](worklog/web_platform.md)'s To confirm

On Linux and the web, as an ordinary user, a `LocalFileProtocol` that deletes
its root when it is destroyed (as `CreateTemp()` does) can't delete a
read-only folder in it with files, as POSIX lets nothing be removed from one.
It logs an error and leaves the folder behind. In `gb_file_test`,
`CopyFolderKeepsAttributes` and `MoveKeepsAttributes` leave one in the temp
folder on every run. Either the deletion gives each read-only folder write
permission before removing what is in it, or the tests clear the attribute
before they end.

## Unique temporary folders in file tests

- **Layers:** file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Web platform](worklog/web_platform.md)'s To confirm

`local_file_protocol_test.cc`'s `GetUnusedTempPath()` names folders from
`GenerateAlphaTestString()`, on `std::rand()`, which nothing seeds. So every
`gb_file_test` process picks the same names in the same order, and two run at
once (such as one test per process, in parallel) race to create the same
folder, and one fails. Naming them as `LocalFileProtocol`'s unique roots are
named, from `std::random_device`, would make them safe to run at once.

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
- With the validation layers' synchronization checks on
  (`VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true`), render-check reports a
  write-after-read hazard: `EndFrameUpdateImages()`'s layout transition, from
  the top of the pipe, against an earlier frame's fragment shader sampling the
  image. Its comment says no frame in flight uses an image it updates. Either
  that doesn't hold, or the check can't see why; waiting on the fragment
  shader would settle it.

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
top. Order the instances by their view order, and leave out an item that a
window of a later instance covers.

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

## Block-world clicks miss the ground

- **Layers:** block-world
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Shared mesh buffers and indirect
  draws](worklog/shared_mesh_buffers.md)

A pilot click on the ground near the bottom of block-world's view, well
within the 50 block reach, adds no block: `World::RayCast()` returns air at a
point below the visible ground, such as (23, 55, 96) from a camera at
(10, 10, 114) with the ground 10 blocks below it. Either the ray from
`Camera::CreateScreenRay()` doesn't match what is drawn at that pixel, or the
ray cast misses blocks it passes through. Find which, fix it, and add a block
edit to `scripts/chunk_pool.txt` (as a click on the ground), which today
leaves rebuilding an edited chunk's mesh to be checked by hand.

## Block-world chunk pool stats and memory

- **Layers:** block-world
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Shared mesh buffers and indirect
  draws](worklog/shared_mesh_buffers.md)

Two loose ends of drawing block-world's chunks from a mesh pool:
- "World Render Stats" counts the indirect draw as one draw call. On a GPU
  without `multiDrawIndirect`, the Vulkan backend makes it one call per chunk,
  so the count is wrong there. Show the calls the backend makes (from
  `VulkanBackend::IsMultiDrawSupported()`), or label the count as draw
  commands.
- Switching to drawing each chunk on its own destroys the pooled meshes but
  keeps the pool's buffers at their grown size (hundreds of MB after the
  farthest view), as a pool never shrinks. Create a new pool when switching
  back instead, and release the old one.

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
there, convert between them in `PilotSdl`. The same holds for a `resize` step's
size, which `PilotSdl` passes to `SDL_SetWindowSize()` in SDL's coordinates.

## Pilot polled mouse

- **Layers:** pilot_sdl
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Scripted runs](worklog/scripted_runs.md)

A program that polls the mouse (`SDL_GetMouseState()`) rather than reading
events still sees the real one in a pilot run, as SDL keeps that state itself
and SDL 2 has no way to set it, short of warping the user's real cursor. Only
ImGui, through `ImGuiInstance`, reads the pilot's mouse (`Pilot::GetMouse()`).
Nothing polls yet, so programs read the mouse from events. Once something
needs to poll, give it a Game Bits call that returns the pilot's mouse in a
run and SDL's otherwise.

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

## One format for config errors

- **Layers:** config
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Config reader with error locations](worklog/config_reader.md)

A config's syntax errors and `ConfigReader`'s errors read alike
(`hello.ozzy(12:5): devices[1].columns: must be 1 to 132`), but the format is
written three times: in `ParseError::FormatMessage()`, in
`ReadConfigFromText()`'s branch for an error with no location (which adds the
filename back, as `ParseError` drops it), and in `ConfigReader::Reject()`.
Only tests keep them alike. One function in `gb_config` that formats a source,
a `ConfigLocation` and a message, leaving out what isn't known, would serve
both, and remove the special branch.

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

## Windows frame wait spins less

- **Layers:** base
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** OZ-3 *App structure*
- **Background:** `PreciseSleepUntil` in `src/gb/base/win_sleep.cc`

`Game::GameLoop` waits for each frame with `PreciseSleepUntil`, which sleeps
until a platform's margin (`kSpinTime`) before the frame is due, then spins for
the rest, as the sleep can wake late. On Linux, a 500 us margin keeps frames as
evenly paced as a 1 ms one (the middle 98% within a few microseconds at 60
frames a second) at 2.5% of a core rather than 5.5%, while sleeping the whole
way spends 0.2% but spreads frames by a few hundred microseconds. Windows keeps
the 1 ms margin the loop had before, as its high resolution timer's lateness
hasn't been measured. Measure it on Windows (frame delta spread and process CPU
for a few hundred frames at 60 frames a second, over several margins), and
lower the margin as far as frames stay as even as they are with 1 ms.

The 500 us margin is in `gen_sleep.cc`, so macOS also gets it unmeasured; once
it is measured there too, a platform whose margin differs gets its own file.
Linux may need less margin if its sleep wakes closer to the time: sleeping to
an absolute time with `clock_nanosleep(TIMER_ABSTIME)` and lowering the
thread's timer slack (`prctl(PR_SET_TIMERSLACK)`), measured as above. The
generic spin also has no CPU pause hint, which `win_sleep.cc` has
(`YieldProcessor()`).

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

## File system errors as absl::Status

- **Layers:** gb_file
- **Size:** large
- **Feature workflow:** yes
- **Depends on:** nothing
- **Requested by:** OZ-3 *Disk tool failure reasons*
- **Background:** `file_system.h`, `file_protocol.h`, `raw_file.h`, and
  `file.h`

`FileSystem`'s calls, and the `FileProtocol` and `RawFile` calls behind them,
say only whether they failed (`bool`, an empty list, a `PathInfo` of
`kInvalid`, a null `File`, or a short count), so a caller can't tell a missing
file from a read-only one, a full disk, or a broken one, and can't pass the
reason on. Each call that can fail returns an `absl::Status` instead (or an
`absl::StatusOr` of what it returns now), with the code saying what kind of
failure it was (`kNotFound`, `kAlreadyExists`, `kPermissionDenied`,
`kResourceExhausted`, ...) and the message saying why, as the protocol has it.
A protocol's `Do` calls return the status, so a protocol of a project's own
(such as one over a disk image) can give its own reasons, and
`LocalFileProtocol` turns the `std::error_code` it already gets into one.
`RawFile`'s reads and writes keep their counts, with the failure behind them
available, so a short read or write can say why. This changes every caller of
`FileSystem`, in Game Bits and in every project, so the design settles how
callers move over, such as in steps by call, and each project adds its own
follow-up.

## Local moves that never replace

- **Layers:** gb_file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** `RenamePath()` in `local_file_protocol.cc`, and the
  `win_` / `gen_` split in the C++ style

`LocalFileProtocol` moves fail when anything is at the new path, but the check
comes before the rename, and `std::filesystem::rename()` replaces an existing
file. A file another process creates at the new path in between is
overwritten. Rename with the operating system's own no-replace move instead, so
the check and the move are one step: `MoveFileExW()` without
`MOVEFILE_REPLACE_EXISTING` in a `win_` file, and in the `gen_` file the
current check and rename (or `renameat2()` with `RENAME_NOREPLACE` where
there is a platform file for it). A move that only changes case must still
succeed on Windows, where the no-replace move sees the new name as taken by
the file itself.

## Links in local files

- **Layers:** gb_file
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** nothing
- **Background:** `LocalFileProtocol`'s class comment, and `RenamePath()` and
  `ListFolder()` in `local_file_protocol.cc`

`LocalFileProtocol` doesn't support links (symbolic links, junctions, and hard
links). It follows them, so a link appears as what it leads to, and links can
interfere with folder copying and deletion. Moving is affected too: a link at
the new path that leads to the file or folder being moved counts as the same
file, as a case-only rename does, so the move replaces the link instead of
failing. With two hard links on POSIX, `rename()` does nothing and reports
success, so both names remain. Decide what each operation does with a link
(most likely: it is an entry of its own, which is listed, copied, moved, and
deleted as a link, not as what it leads to), and test each one with links,
which on Windows needs developer mode to create.

## Read-only memory folders

- **Layers:** gb_file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [File attributes](worklog/file_attributes.md)

A read-only folder in `MemoryFileProtocol` prevents nothing, as on Windows. Add
a `ReadOnlyFlag` (off by default, as `kPreventDelete` is) where it acts as on
POSIX: nothing can be created, deleted, or moved in a read-only folder, so a
recursive delete fails inside one. This lets code meant to run on POSIX be tested against memory
files, including that folder copies set a folder's attributes only after
copying what is in it.

## Fewer system calls for local files

- **Layers:** gb_file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [File attributes](worklog/file_attributes.md)

`LocalFileProtocol::DoGetPathInfo()` makes four system calls for a file
(`std::filesystem::status`, `file_size`, the attributes, then
`last_write_time`), where `GetFileAttributesExW` returns all four at once. A
`GetLocalPathInfo()` in `platform_file.h` could return the type, size,
attributes, and modified time together, in one call on Windows. Folder copies also use `FileProtocol`'s
default `DoCopyFolder`, which lists each folder twice (files, then folders)
and queries every entry. An override that lists each folder once, copying
through `DoCopyFile` and setting the folder's attributes last, would do less.
Measure first: copying is usually bound by the file I/O itself.
