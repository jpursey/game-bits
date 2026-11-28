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
| Web platform | Every CL merged | [worklog](../worklog/web_platform.md) |
| Programs in the browser | In progress | [worklog](../worklog/programs_in_the_browser.md) |
| WebGPU render backend | In progress | [worklog](../worklog/webgpu_render_backend.md) |
| Files on the web | In progress | [worklog](../worklog/files_on_the_web.md) |
| Web demos | In progress | [worklog](../worklog/web_demos.md) |

Each depends on the one above it, apart from *Files on the web*, which only
needs *Programs in the browser*. A feature's row changes in the PR that starts
or finishes it, and its worklog's CL marks say how far it is.

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
- **The display paces frames in a page.** A `Game`'s loop waits for the
  browser's next frame (`gb/base/frame_wait.h`), which is when the browser
  shows what was drawn, so the game's frame rate limit is ignored there
  (*Frame limits in a browser*). A piloted run doesn't wait: it hands control
  back to the browser between frames and carries on at once. Neither call
  does anything on Windows, on Linux, or under Node.
- **WebGPU for drawing.** `RenderBackend` is already abstract, and ImGui draws
  through it. WebGL 2 has no storage buffers, which `gb_render` needs. The
  bindings are Dawn's for Emscripten (the `emdawnwebgpu` port), which come
  with the Emscripten SDK at a version it pins.
- **`RenderBackend` and `gb_render`'s API don't change for the web.** What
  WebGPU can't do, the backend does another way where it can: indirect draws
  are one call each, 4-bit color textures are expanded, and a depth target is
  sampled through a copy. Otherwise creating the resource fails, with an
  error: clamp to border, wireframe fill, and 3-channel vertex values
  narrower than 32 bits, each a backlog item. A change to the API instead
  needs the user's approval (see
  [How the project is run](#how-the-project-is-run)).
- **A page's assets are one file beside it.** A program names its assets in
  CMake (`gb_pack_assets()`), and a web build packs them into `<name>.data`,
  which the page fetches before `main()` into its file system, so every read
  returns at once, without threads, and nothing in `gb_file` changes. Building
  them into the `.wasm` instead would grow it by their size, against the
  15 MB a Claude artifact allows each file. On Windows and Linux the command
  only checks that the assets are there.
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
pipeline that declare different bindings would disagree. So a binding's WGSL
number depends only on its own GLSL number `n`: a sampler is at `2n + 1`, and
everything else at `2n`. `naga` converts without validating, which leaves a
texture and its sampler on one binding, a CMake script
(`CMake/GameBitsWgslBindings.cmake`) renumbers them, and `naga` reads the
result back, which validates it. `gb_embed_shader` runs all of it, so a
project's CMake doesn't change.

gb_imgui's shaders are SPIR-V pasted into its source. Their WGSL is pasted
into a web file of its own (`web_imgui_shaders.cc`), which changes nothing for
a project that links `gb_imgui`, where compiling them in the build would make
each need `glslc`.

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
    the Emscripten SDK, the shader tools above, the browser CI runs
    pages in, and GitHub's own actions for publishing to Pages;
  - takes something listed as in scope out of it, or the reverse.
- **Smaller cross-platform changes go ahead,** and are listed below with
  their PRs, for the user to read afterwards.
- **The session reports** when a feature finishes, and when it is stopped.
- **Agents share the account's usage limit.** When it runs out, every agent
  stops until it resets, and the session then tells each to carry on from
  what it pushed. So an agent commits and pushes its work in progress often.

What only the user can do:
- Approve each change to the `game-bits` cloud environment, which installs
  the SDKs sessions build with. It installs the Emscripten SDK, and
  `spirv-opt` with the Vulkan SDK. It doesn't install `naga`, which takes
  three minutes to build: a session that converts shaders installs it for
  itself.
- Say when each feature is complete, which is when its worklog's plan becomes
  a summary.
- Turn GitHub Pages on for the repository, for *Web demos*: in its Settings,
  under Pages, set "Build and deployment" to "GitHub Actions". The workflow
  that publishes the demos is added once that is done.
- Say yes to each demo published as a Claude artifact, which is private
  until shared.
- Try block-world's mouse look in a real browser: holding the right button
  locks the pointer, which a headless browser can't check.
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
| `SupportsThreads()` is added, and code that needs a thread checks it | Web platform | [71](https://github.com/jpursey/game-bits/pull/71) |
| `AllocAligned()` allows an alignment smaller than a pointer in `gen_aligned_memory.cc` | Web platform | [66](https://github.com/jpursey/game-bits/pull/66) |
| Tests that need a file name that isn't UTF-8 are skipped where there is none | Web platform | [69](https://github.com/jpursey/game-bits/pull/69) |
| CI builds and tests the web builds (`build-web`, `build-web-threads`), and Checks say when a session builds them too | Web platform | [73](https://github.com/jpursey/game-bits/pull/73) |
| `tools/run-program.sh` reports a program's timeout as it does a page's | Programs in the browser | [78](https://github.com/jpursey/game-bits/pull/78) |
| `templates/sdl` becomes a `gb::Game` that takes the pilot flags | Programs in the browser | |
| CI builds the templates on Windows and Linux (`build-templates`) | Programs in the browser | |
| `tools/install-vulkan-sdk.sh` also installs `spirv-opt`, and `gb_wgsl_bindings_test` runs on every platform | WebGPU render backend | [79](https://github.com/jpursey/game-bits/pull/79) |
| render-check chooses its backend through an interface of its own (`RenderCheckBackend`) | WebGPU render backend | |
| `gb_pack_assets()` checks that each named asset is there | Files on the web | |
| `templates/sdl` reads a file from its `assets` folder as it starts | Files on the web | |
| Block-world's shaders are compiled by the build, with `gb_embed_shader()` | Web demos | |
| CI builds block-world on Linux and runs its scripts (`build-block-world-linux`) | Web demos | |
| Block-world chooses its backend through an interface of its own (`BlockWorldBackend`) | Web demos | |
| gb_imgui's font atlas is clamped to its edge, not to a transparent border, which draws the same pixels | WebGPU render backend | [83](https://github.com/jpursey/game-bits/pull/83) |
| `Game`'s loops call the frame wait each frame, which does nothing outside a page | Programs in the browser | [80](https://github.com/jpursey/game-bits/pull/80) |

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
- **WebGPU in headless Chromium,** found while planning the render backend:
  - Resolving a multisampled frame straight into an sRGB view of the canvas's
    texture left linear values in the canvas on SwiftShader. Resolving into
    an sRGB texture and copying that to the canvas gives Vulkan's bytes.
  - The canvas has a new, transparent texture after every return to the
    browser, so a frame is drawn whole elsewhere and copied in at its end.
  - A depth texture can't be sampled as GLSL's `sampler2D` converts, and
    depth can't be resolved, so each depth target has an `r32float` copy.
  - A page's runtime stays alive while any WebGPU object is held, so the page
    only exits once the backend has released them all.
  - SwiftShader allows 256 layers in a texture array, where `gb_render`
    allows 2048.
- **Block-world** builds with clang as it is, with no errors or warnings,
  and both its scripts pass on Linux on lavapipe once its shaders are
  compiled. With only its backend swapped for one that draws nothing, it
  builds for the web and its script's steps pass in headless Chromium: it
  uses no threads and no `gb_job`. Its Release `.wasm` was 2.1 MB (0.8 MB
  compressed) and its assets 2.7 MB.
- **SDL's canvas follows its CSS size,** so a page whose canvas is sized to
  the browser window gets a window of that size, and resize events with it.
- **A web CI job** takes about 8 minutes. Installing the Emscripten SDK takes
  about 15 seconds, and 5 from the cache (463 MB).
- **Trial builds** of the libraries are recorded in
  [Web platform](../worklog/web_platform.md).
