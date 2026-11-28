# Web support

Game Bits runs in a web browser, as a third platform beside Windows and Linux.
A program built on it is compiled to WebAssembly with Emscripten, draws with
WebGPU, and is opened as a page: from a web server, or as a Claude artifact
shared by its link.

This is the design for the whole project, which is several features. Each
feature has its own plan in `docs/worklog/` once it starts, and is a backlog
item until then. This doc records what holds for all of them: what is in and
out, the decisions they share, how the project is run, and where it stands.

## Done

The project is done when:
- render-check and block-world run in Chrome, from a page on GitHub Pages and
  as a Claude artifact, and are checked in a headless browser in sessions and
  CI, as render-check is on Linux today.
- A project built on Game Bits gets the same from `emcmake cmake` and Game
  Bits' CMake commands, with nothing web-specific of its own beyond its
  assets.
- Every library built for the web passes its unit tests there, in CI.

## Status

| Feature | Status | Plan |
|---|---|---|
| Web platform | In progress (CL1 to CL3 of 9 merged) | [worklog](../worklog/web_platform.md) |
| Programs in the browser | Not started | [backlog](../backlog.md#programs-in-the-browser) |
| WebGPU render backend | Not started | [backlog](../backlog.md#webgpu-render-backend) |
| Files on the web | Not started | [backlog](../backlog.md#files-on-the-web) |
| Web demos | Not started | [backlog](../backlog.md#web-demos) |

Each depends on the one above it, apart from *Files on the web*, which only
needs *Programs in the browser*. A feature's row changes in the PR that starts
or finishes it.

## Scope

In:
- **Web platform:** the Emscripten SDK, the build, and every library that
  doesn't open a window or draw, with its unit tests run under Node.
- **Programs in the browser:** a windowed program is a page, with SDL's window
  as its canvas, keyboard and mouse input, the game loop, and pilot scripts
  run in a headless browser.
- **WebGPU render backend:** `gb_render_webgpu`, shaders converted from the
  SPIR-V the build already makes, and ImGui and render-check drawing with it.
- **Files on the web:** a program's assets, packed with it when it is built.
- **Web demos:** render-check and block-world on GitHub Pages and as Claude
  artifacts. Block-world's own shaders are compiled by the build for this,
  which is the block-world half of the backlog's *Block-world and ImGui
  shaders compiled by the build*, and block-world is made to compile with
  clang, which is most of *Block-world on Linux*.

Out, each a backlog item where it is worth doing later:
- **Jobs and profiles.** `gb_job` and `gb_profile` aren't built for the web,
  as the Vulkan libraries aren't. The job system needs fibers, and the
  profiler the x86 timestamp counter. No other library or example uses
  either. (*Jobs on the web*, *Profiles on the web*)
- **Threads in a browser.** A web build can have threads
  (`GB_WEB_THREADS`), and its tests run under Node, so code that uses threads
  is tested as 32-bit WebAssembly. But a program in a browser is only built
  and checked without them: the demos need none, and a Claude artifact can't
  have them. (*Threads in the browser*)
- **Files that are kept.** What a program writes lasts until its page is
  closed. Block-world only writes ImGui's window layout. (*Saved files on the
  web*)
- **Browsers other than Chrome.** Chromium is what sessions and CI can run.
  Nothing here is made to fail in Firefox or Safari, but neither is checked,
  and Safari before 27 has no JSPI.
- **Touch, game controllers, sound, and phones.** Keyboard and mouse on a
  desktop browser only. Game Bits has no sound on any platform.
- **Building for the web on a Windows machine.** Emscripten runs there, but
  only Linux is checked, in sessions and CI.
- **The WebGPU backend outside a browser** (on Dawn, for Windows and Linux).
- **Third-party updates:** *SDL3*, *ImGui update*, and *Abseil and GoogleTest
  update* are independent of the web, and none is needed.
- **Projects built on Game Bits.** Each needs its own follow-up to build for
  the web.

## Decisions

- **Emscripten, wasm32.** It is the only complete C++ toolchain for the web,
  and SDL, Abseil, and Dawn's WebGPU bindings all target it. Pointers and
  `size_t` are 32 bits. The version is pinned (7.0.1), as the Vulkan SDK's is.
- **Threads are chosen per build, and the default is none.** Threads on the
  web need `SharedArrayBuffer`, which a browser only gives a page served with
  cross-origin isolation headers (`Cross-Origin-Opener-Policy: same-origin`
  and `Cross-Origin-Embedder-Policy: require-corp`). A Claude artifact isn't
  served that way, and GitHub Pages can't set headers. A build without threads
  runs anywhere. One build can't be both.
- **The game loop stays a blocking loop.** A browser tab can't block, and
  WebGPU only presents a frame when the thread returns to the browser. JSPI
  (JavaScript Promise Integration) lets a blocking call hand control back to
  the browser, so `Game::Run()`, `CleanUp()`, exit codes, and the render
  backend's waits stay as they are on every platform. It is on by default in
  Chrome 137, Firefox 153, and Safari 27.
- **SDL2 stays.** The pinned 2.32.10 builds for the web as it is. Neither SDL2
  nor SDL3 supports drawing from any thread but the browser's main one, so
  `main()` runs there.
- **WebGPU for drawing.** `RenderBackend` is already abstract, and ImGui draws
  through it. WebGL 2 has no storage buffers, which `gb_render` needs. The
  bindings are Dawn's for Emscripten (the `emdawnwebgpu` port), which come
  with the Emscripten SDK at a version it pins.
- **`RenderBackend` and `gb_render`'s API don't change for the web.** What
  WebGPU can't do (wireframe fill, clamp to border, drawing many indirect
  draws in one call) the backend does another way where it can, and otherwise
  fails to create, with an error. A change to the API instead needs the
  user's approval (see [How the project is run](#how-the-project-is-run)).
- **Names:** `web_` is the prefix for files that are the web's own, beside
  `win_`, `posix_`, `linux_`, and `gen_`. It names the platform, as the
  others do, where `wasm_` would name the processor and `emscripten_` the
  toolchain.

## Shaders

A browser takes only WGSL, and the build makes SPIR-V from GLSL with `glslc`.
A web build converts each shader's SPIR-V to WGSL with two tools, which only a
web build runs:
- `spirv-opt`, from the Vulkan SDK, splits each of GLSL's combined image
  samplers into a texture and a sampler
  (`--split-combined-image-sampler`). WGSL has no combined ones.
- `naga` (`naga-cli`, installed with Rust's `cargo` at a pinned version)
  converts the result to WGSL.

Every shader of render-check, block-world, and gb_imgui (19 in all) converts
this way, and Chromium's WebGPU accepts each result. Tint, Dawn's own
compiler, also converts SPIR-V, but nothing ships it as a tool: Dawn's
releases have its libraries only, so it would have to be built.

A texture and its sampler can't share a binding in WGSL. `spirv-opt` can
renumber them (`--resolve-binding-conflicts`), but it moves every later
binding up by one for each sampler a shader declares, so two shaders of one
pipeline that declare different bindings would disagree. *WebGPU render
backend*'s plan settles a numbering that depends only on a binding's own
number (such as the texture at twice its number and the sampler one after).

gb_imgui's shaders are SPIR-V pasted into `imgui_instance.cc`. Their WGSL is
pasted beside them, which changes nothing for a project that links
`gb_imgui`, where compiling them in the build would make each need `glslc`.

## How the project is run

This project is run differently from the feature workflow in
[workflow.md](../workflow.md), at the user's direction, from when this design
is approved until the project is done:
- **One session runs it.** It writes each feature's plan from its backlog
  item, hands each CL to an agent, reviews what comes back, and merges it.
  This doc's [Status](#status) and each worklog's CL marks are its record, so
  another session can pick the project up from them.
- **An agent implements each CL,** on its own branch, as one PR against
  `main`, following workflow.md as any cloud session does: it builds and
  checks the CL, runs `/simplify` and `/code-review`, and writes the PR's
  description and verification comment. CLs that don't depend on each other
  run at the same time.
- **The session reviews and merges.** It reads the PR against its CL in the
  plan, has the agent fix what it finds, starts CI on the PR's branch, and
  squash merges once every job passes. It doesn't wait for the user. A PR
  that doesn't build for Windows, or fails a test there, isn't merged.
- **The user approves what changes Game Bits for everyone.** The session
  stops and asks before a PR that:
  - changes what a public API does, or its signature, on Windows or Linux
    (adding to one doesn't count);
  - changes how shared code works, where a fix to a defect in it, or moving a
    platform's part into a platform file, doesn't count;
  - adds a dependency: a submodule, an SDK, or a tool the build runs, beyond
    the Emscripten SDK, the shader tools above, and the browser CI runs
    pages in;
  - takes something listed as in scope out of it, or the reverse.
- **Smaller cross-platform changes go ahead,** and are listed below with
  their PRs, for the user to read afterwards.
- **The session reports** when a feature finishes, and when it is stopped.

What only the user can do:
- Approve each change to the `game-bits` cloud environment, which installs
  the SDKs sessions build with.
- Turn GitHub Pages on for the repository, for *Web demos*.
- Open a demo in a browser on a real GPU. Sessions and CI draw with a
  software one (SwiftShader), which shows what is drawn but not how fast.
- Run block-world on Windows after its shaders change, as CLAUDE.md's Checks
  already leave to the user.

## Cross-platform changes

Changes outside the web's own files, as they are planned and then merged.

| Change | Feature | PR |
|---|---|---|
| `gb_add_test` registers tests as `add_test(NAME ... COMMAND ...)` | Web platform | [63](https://github.com/jpursey/game-bits/pull/63) |
| `EXPECT_DEATH` becomes `EXPECT_DEATH_IF_SUPPORTED` in every test | Web platform | [62](https://github.com/jpursey/game-bits/pull/62) |
| `Queue`'s buckets take their elements' alignment, and allocate aligned for elements aligned to more than `std::malloc()` gives | Web platform | [64](https://github.com/jpursey/game-bits/pull/64) |
| `SupportsThreads()` is added, and code that needs a thread checks it | Web platform | |
| `AllocAligned()` allows an alignment smaller than a pointer in `gen_aligned_memory.cc` | Web platform | |
| Tests that need a file name that isn't UTF-8 are skipped where there is none | Web platform | |
| Block-world's shaders are compiled by the build | Web demos | |

## Findings

- **A Claude artifact** was checked with a page of feature tests, published
  as an artifact and opened in Chrome 158 on Windows: WebAssembly (also from a
  `.wasm` file beside the page), JSPI, a WebGPU device and canvas (with
  `indirect-first-instance`), Web Workers, pointer lock, and private file
  storage all work. `SharedArrayBuffer` doesn't, and the page isn't
  cross-origin isolated. An artifact's page is at most 16 MB, and each file
  beside it 15 MB.
- **Headless Chromium in a cloud session** (158, from Playwright, under
  `xvfb-run`) makes a WebGPU device on SwiftShader with
  `--enable-unsafe-webgpu --use-angle=swiftshader --enable-unsafe-swiftshader
  --ignore-gpu-blocklist --enable-gpu --enable-features=Vulkan
  --use-vulkan=swiftshader`, and none without flags. The same page reported
  the same features as Chrome on Windows.
- **WebGPU in browsers:** Chrome on Windows, macOS, and Android; Firefox on
  Windows and Apple-silicon Macs; Safari 26. Linux is partial.
- **A blocking loop, SDL2, and WebGPU together** were checked with one small
  page outside the repository, built with `-sJSPI` and
  `--use-port=emdawnwebgpu` against the SDL2 that a web build makes, and run
  in headless Chromium as above. A loop of 90 frames that waited for
  `requestAnimationFrame` each frame ran at 57 frames a second, `SDL_PollEvent`
  delivered key presses and mouse moves, and a frame copied from the canvas's
  texture to a buffer read back exactly what the shader drew. The waits for
  the adapter, the device, and the read back were `wgpuInstanceWaitAny()` with
  a timeout, which needs the instance's `TimedWaitAny` feature. Emscripten
  7.0.1's bindings are Dawn's release of 5 November 2026.
- **The Emscripten SDK's cache is root's** where root installed it, so a build
  run as another user fails to link. Sessions build as root and run tests as
  an ordinary user.
- **Trial builds** of the libraries are recorded in
  [Web platform](../worklog/web_platform.md).
