# Web demos

render-check and block-world run in a browser for anyone, without building
anything:
- **GitHub Pages:** a workflow builds both for the web in Release on each push
  to `main`, and publishes them at `https://jpursey.github.io/game-bits/`, with
  a page listing them. README.md links to it.
- **Claude artifacts:** `tools/package-artifact.sh` gathers a page and the
  files beside it into a folder that a session publishes with the Artifact
  tool, to share by its link.

Each demo's page is what a visitor needs: a title, a loading message while
its files download, the canvas filling the browser window, and a plain message
where the browser can't run it. Both demos' pilot scripts run in headless
Chromium in CI, as render-check's do on Linux.

Block-world gets there in steps that start now, before the WebGPU backend is
finished: its shaders are compiled by the build, as render-check's are, and it
builds, runs, and is checked on Linux, natively (Vulkan on lavapipe), in CI.
Then it picks its backend per platform, as render-check does after *WebGPU
render backend*'s CL8, and builds for the web with its assets packed by
*Files on the web*'s `gb_pack_assets()`. [Web
support](../designs/web_support.md) is the design for the whole project; this
is its last feature, and its Done is this feature's.

On Windows, block-world draws exactly as before, from shaders built into it
rather than `.spv` files a script made; render-check doesn't change.

## Behavior

- **On the web:** `render_check.html` and `block_world.html` are built for the
  web like any page, with block-world's `block_world.data` beside it holding
  its fonts and textures. Both are ordinary Game Bits pages: their pilot
  scripts run in `tools/run-program.sh`, and a visitor plays them by hand.
- **Block-world everywhere:**
  - Its shaders (`block.vert`, `block.frag`, `block_pool.vert`) move to
    `examples/block-world/shaders/` and are built into it with
    `gb_embed_shader()`, as render-check's are. A fresh checkout builds and
    runs it with no other step, on every platform.
    `assets/shaders/build-shaders.bat` is no longer needed for it.
  - It builds and runs on Linux, natively, with Vulkan, and CI runs its
    scripts there on lavapipe.
- **The page around a program** (`CMake/GameBitsShell.html`), for every page:
  - Its title is the program's name in words (`block_world` is "Block
    World").
  - A message shows what is loading (the program, then a page's packed
    assets, with their progress) until `main()` starts.
  - The canvas fills the browser window, so SDL's window is the window's size
    and follows it. A tool that needs the program's own window size adds
    `canvas=fixed` to the address, which `tools/run-page.js` does, so pilot
    scripts' `resize` steps work as now.
  - Where the browser lacks JSPI, a message says the page needs a browser that
    has it (Chrome 137, Firefox 153, Safari 27, or later), instead of a
    failure in the console.
  - If the program stops with an error, the last lines it logged are shown
    over the canvas, so a visitor sees "no WebGPU" (logged by the backend)
    rather than a still canvas.
  - The canvas takes focus when the page loads and when it is clicked, and
    right-clicking it opens no menu, as block-world turns the camera with the
    right button.
- **`tools/package-artifact.sh <page.html> <out-dir>`** copies a page and its
  `.js`, `.wasm`, and `.data` into `<out-dir>`, checks they fit an artifact
  (page at most 16 MB, each other file at most 15 MB), and refuses a build
  with threads, which an artifact can't run. It prints what to publish.

## Design

### Names

- *Web demos* is the feature's name; "demo" is a page published for people to
  play, of an example that already exists. "Showcase" and "gallery" say less
  about what a visitor does.
- `tools/package-artifact.sh`, beside `tools/run-program.sh`. "Package" is
  what it does; "export" would read as converting the program, and "publish"
  as doing the publishing, which the Artifact tool does.
- `.github/workflows/pages.yml`, as GitHub names the feature, beside
  `cmake.yml`.
- `examples/web-demos/index.html` is the page listing the demos on Pages. It
  is an example of how the demos are found, not a program, so it is beside
  them rather than in `CMake/`.
- `canvas=fixed` in a page's address keeps the canvas at the window size the
  program asks for. `size=program` and `resize=0` say less about what is
  fixed.
- block-world's backend choice follows render-check's: `BlockWorldBackend`,
  with `vulkan_block_world_backend.cc` and `webgpu_block_world_backend.cc`.

### Block-world on Linux

Built on Linux with clang, as CI builds render-check
(`-DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++`), block-world compiled and linked with no errors
and no warnings, with `-Werror` on, as it is today: nothing in it is
Windows-only. With its three shaders compiled to `assets/shaders/*.spv` by
hand (what `build-shaders.bat` does on Windows), both scripts passed on
lavapipe under `tools/run-program.sh`: `walk.txt` in 33 s, and
`chunk_pool.txt` in 465 s (647 frames). Its frames show the world, the GUI, and
the camera as on Windows, at about 440 ms of CPU a frame for about a million
triangles.

So *Block-world on Linux* is what falls out of compiling its shaders in the
build: CL1 does that, and CL2 adds it to CI and the docs.

### Block-world's shaders

```
# examples/block-world/CMakeLists.txt
gb_embed_shader(block_world shaders/block.vert shaders/block_vert.h
  kBlockVertexShader)
gb_embed_shader(block_world shaders/block.frag shaders/block_frag.h
  kBlockFragmentShader)
gb_embed_shader(block_world shaders/block_pool.vert shaders/block_pool_vert.h
  kBlockPoolVertexShader)
```

- `WorldResources::Create()` makes its shader code with
  `CreateShaderCode(kBlockVertexShader.GetBytes())` and so on, in place of
  `LoadShaderCode("asset:/shaders/block.vert.spv")`. In a web build the same
  names hold WGSL (*WebGPU render backend*'s CL1).
- The three GLSL files move from `assets/shaders/` to
  `examples/block-world/shaders/`, beside the code that uses them, as
  render-check's are. `assets/shaders/` keeps gb_imgui's two sources, whose
  SPIR-V is pasted into `gen_imgui_shaders.cc`, and `build-shaders.bat`, which
  only they then need. This is block-world's half of the backlog's
  *Block-world and ImGui shaders compiled by the build*, which this plan
  renames *ImGui shaders compiled by the build*, for the half that stays.
- On Windows the same SPIR-V is embedded that `build-shaders.bat` wrote
  (`glslc` with no options, where `gb_embed_shader()` adds `-Werror`, which
  these compile under), so it draws the same. Building it needs `glslc`, as
  render-check does, which every machine with the Vulkan SDK has.

### Block-world in a page

A scratch copy of block-world, with `VulkanBackend` swapped for `gb_render`'s
test backend (the one change it needed), built for the web in Release with
Emscripten 7.0.1 and its assets preloaded, ran `walk.txt`'s steps in headless
Chromium under `tools/run-program.sh`: title, then playing (the world built in
about a second), then walking, 213 frames, exiting with code 0. So in a web
build without threads it needs nothing it lacks:
- No `gb_job`, threads, or `ThreadMessageDispatcher`: its messages go through a
  `PollingMessageDispatcher`, and it builds its chunks on the main thread.
- Its loop is `Game`'s, which waits for the next frame in a page (*Programs in
  the browser*).
- It finds `assets` by name from its current folder, `/` in a page, and writes
  ImGui's layout to `game:/block-world-ui.ini`, in memory there (*Files on the
  web*).
- What is Vulkan's: `SdlVulkanWindow`, `VulkanBackend`, `SDL_WINDOW_VULKAN`,
  and `ImGui_ImplSDL2_InitForVulkan`, all in `BlockWorld::Init*()`.

What it draws with, which the WebGPU backend must have: a texture array (its
blocks), a storage buffer read in a vertex shader (chunk positions), a mesh
pool drawn with an indirect draw, depth, and ImGui. *WebGPU render backend*'s
CL5, CL6, and CL7 cover each, and CL8's render-check draws each.

**Backend choice.** `BlockWorldBackend` (`block_world_backend.h`), as
render-check's `RenderCheckBackend`:

```
class BlockWorldBackend {
 public:
  // Flags for SDL_CreateWindow(): SDL_WINDOW_VULKAN for Vulkan, none for
  // WebGPU.
  static uint32_t GetWindowFlags();

  // Creates the backend for the platform, drawing into the window, taking
  // what it needs from the game's context (Vulkan's window takes its SDL
  // events from the context's message system).
  static std::unique_ptr<gb::RenderBackend> Create(gb::Context& context);

  // Initializes ImGui's SDL support for the backend.
  static void InitImGui(SDL_Window* window);
};
```

All static, as block-world needs nothing of a backend after creating it (its
`render_backend_` member is unused, and goes). The two files are picked with
`if (EMSCRIPTEN)` in its `CMakeLists.txt`, with `gb_render_vulkan` and
`gb_render_vulkan_sdl` or `gb_render_webgpu`.
- ImGui's SDL backend in Game Bits' copy of ImGui has no `InitForOther()`. The
  WebGPU file uses `ImGui_ImplSDL2_InitForOpenGL(window, nullptr)`, which does
  the same (`InitForMetal()` does too), with a comment saying why. render-check
  needs the same in *WebGPU render backend*'s CL8.
- `gb_pack_assets(block_world "${GB_DIR}/assets" fonts textures)` packs its
  assets (2,665,844 bytes), as *Files on the web* plans.

**Mouse look in a page.** Holding the right button turns the camera, with
`SDL_SetRelativeMouseMode()`, which in a page locks the pointer at the next
input event after it is asked (*Programs in the browser*'s SDL findings):
- The right button's own press is the event that asks, so the lock comes with
  the first movement after it, not the press.
- `SDL_WarpMouseGlobal()`, which puts the cursor back where the drag started,
  does nothing in a page. The cursor reappears where the lock left it, which
  is where it was, as the browser doesn't move it while locked.
- Escape leaves pointer lock, as the browser requires; block-world doesn't
  quit on Escape, so nothing else happens.
- The page's context menu would open on the right button, so the shell stops
  it (see [The page](#the-page)).

None of these can be checked headless (Playwright's mouse in pointer lock
jumps), so they are left to the owner in a real browser.

### The page

`CMake/GameBitsShell.html` is the page around every program in a web build.
Each change below is for every page, and only on the web:
- **Title:** `thisProgram` with each `_` a space and each word capitalized.
- **Loading:** a line of text over a dark background, from Emscripten's
  `Module.setStatus()`, which it calls with what is loading ("Downloading
  data... (1200000/2665844)" while a package downloads), and hidden from the
  last `preRun` step, when `main()` is about to start.
- **Canvas size:** by default, CSS sizes the canvas to the window (`100vw` by
  `100vh`), which SDL's driver follows for a resizable window: the canvas's
  pixels, `SDL_GetWindowSize()`, and SDL's resize events follow the browser
  window. With `canvas=fixed` in the address, the canvas has no CSS size, and
  keeps the size the program gives its window, as every page does today.
  `tools/run-page.js` always adds it, so a script's window size and `resize`
  steps are as on Linux.
- **JSPI:** before the program's script loads, the shell checks for
  `WebAssembly.Suspending`. Without it, the message names the browsers that
  have it, and the program isn't loaded.
- **Errors:** the shell keeps the last 20 lines of stderr. When the program
  exits with a code other than 0, or aborts, it shows them over the canvas.
  WebGPU is the backend's to check: it logs that there is no adapter, which
  then shows. A page without drawing (such as `templates/sdl`) needs no
  WebGPU.
- **Input:** the canvas is focused when the page loads and on each click, so
  keys reach it, as they must in an iframe (a Claude artifact) after a click.
  Its `contextmenu` event is stopped.

`gbFiles`, `gbExit`, and `Module.FS`, which tools use, don't change.

**Brittleness:** a tool that opens a page without `canvas=fixed` gets the
viewport's size instead of the program's. `run-page.js` is the only one, and
it adds it in one place.

### GitHub Pages

`.github/workflows/pages.yml`:
- Runs on each push to `main` that changes more than docs, and by hand
  (`workflow_dispatch`). One run at a time (`concurrency: pages`), so a newer
  push replaces a run still deploying.
- One job on Ubuntu: the Vulkan SDK (`glslc`, `spirv-opt`), `naga` (*WebGPU
  render backend*'s `.github/actions/naga`), and the Emscripten SDK
  (`.github/actions/emsdk`); then render-check and block-world, each built for
  the web in Release as its own project (as CI builds them natively).
- The site is a folder: `index.html` (from `examples/web-demos/index.html`),
  `render-check/` (`render_check.html`, `.js`, `.wasm`), and `block-world/`
  (`block_world.html`, `.js`, `.wasm`, `.data`). It is uploaded with
  `actions/upload-pages-artifact` and published with `actions/deploy-pages`,
  with the `pages: write` and `id-token: write` permissions they need.
- `examples/web-demos/index.html` lists each demo with a line on what it is
  and how to play it (block-world: WASD to walk, hold the right button and
  move to look, left click to add a block, a right click to remove one), and
  what a browser needs.
- GitHub Pages serves `.wasm` as `application/wasm` and compresses it, which a
  `.wasm` needs to compile while it downloads. Block-world's Release `.wasm`
  is 2.1 MB, 0.8 MB compressed (see [Sizes](#sizes)).

What the owner does once: in the repository's Settings, Pages, set "Build and
deployment" to "GitHub Actions". The repository is public, so Pages needs no
paid plan. Until then the workflow fails at its deploy step, so it is added
with the README link only once the owner has, or the job is skipped while the
`github-pages` environment doesn't exist.

README.md gains a "Try it in a browser" section near the top, with the two
links, what a browser needs (Chrome 137 or later on Windows, macOS, or
ChromeOS; others as they ship JSPI and WebGPU), and the controls.

### Claude artifacts

An artifact is a page published with the Artifact tool: an HTML file, plus
files beside it given in its `files` map, each published at the path the page
references. So a Game Bits page is published as it is: the page as the
artifact's file, and its `.js`, `.wasm`, and `.data` as files beside it.

`tools/package-artifact.sh <dir>/<name>.html <out-dir>`:
- Copies `<name>.html` and whichever of `<name>.js`, `<name>.wasm`, and
  `<name>.data` exist into `<out-dir>`.
- Fails if the page is over 16 MB or any other file over 15 MB, naming it, or
  if the build has threads (its `.js` starts workers, which it detects by
  Emscripten's pthread code in it), as an artifact isn't cross-origin
  isolated and can't run one.
- Prints the files and the media type each needs (`application/wasm`,
  `text/javascript`, and `application/octet-stream` for `.data`, which the
  Artifact tool infers for none).

A session then publishes it with the Artifact tool: the page as `file_path`,
and the other files in `files`, each with its media type. It is private until
the owner shares it, so publishing one is the owner's call each time, and the
CL that adds the script publishes one only with the owner's go-ahead (through
the orchestrator).

### Sizes

Measured with the scratch copy of block-world above (Release, `-O3` as CMake's
Release gives, Emscripten 7.0.1), with the test backend in place of a render
backend:

| File | Bytes | Compressed (gzip -9) |
|---|---|---|
| `block_world.html` | 751 | |
| `block_world.js` | 163,595 | 41,085 |
| `block_world.wasm` | 2,068,980 | 798,002 |
| `block_world.data` (fonts and textures) | 2,665,844 | about 1.7 MB |

The WebGPU backend and Dawn's bindings add to the `.wasm` what they are: the
whole experiment page of *WebGPU render backend*, with the bindings, was
201 KB at `-O1`, so the backend should add well under 1 MB. render-check
links the same libraries with more of its own code and no assets, so its
`.wasm` should be 2 to 3 MB. Both are far under an artifact's 15 MB a file;
CL3 and *WebGPU render backend*'s CL8 measure them.

### Checks

- **CI:** *WebGPU render backend*'s CL9 runs render-check's seven scripts in
  headless Chromium. This feature adds, beside it:
  - `build-block-world-linux`: block-world natively on Ubuntu (Release,
    clang), its two scripts on lavapipe, its frames uploaded. `chunk_pool.txt`
    takes about 8 minutes there, so it gets a 15 minute timeout, and the job
    runs beside the others, within CI's 20 minutes.
  - `build-block-world-web`: block-world for the web (Release), and `walk.txt`
    in headless Chromium, its frames uploaded. `chunk_pool.txt` would take far
    longer on SwiftShader than on lavapipe for no more coverage than the
    native run gives, so only `walk.txt` runs there.
  - The Pages workflow builds both demos on each push to `main`, so one that
    no longer builds for the web is found there too.
- **Left for the owner, by hand** (sessions and CI have neither a GPU nor
  Windows' browsers):
  - Both demos on Pages and as an artifact, in Chrome on a real GPU: that they
    load, draw as in CI's frames, and how fast.
  - Block-world's mouse look in a page: pointer lock with the right button,
    and the cursor after it.
  - Block-world on Windows after CL1 (its shaders built in) and CL3 (its
    backend chosen per platform), as CLAUDE.md always leaves to the owner.

### To confirm

Checked in this plan's experiments, on Linux in a cloud session:
- Block-world builds with clang, natively in Release, with no errors or
  warnings, and draws on lavapipe with both scripts passing (times above).
  (CL1, CL2)
- Block-world compiles for the web in Release with only its backend swapped,
  and its pilot steps pass in headless Chromium with a non-drawing backend:
  so nothing but the backend and its assets stand in its way. (CL3)
- The sizes above. (CL3, CL5)
- With the canvas's CSS size set to the window, SDL's canvas followed the
  viewport (1000x600, 1400x800, 640x480), with no errors; without it, it kept
  the program's 1280x720. (CL4)
- Game Bits' copy of ImGui has no `ImGui_ImplSDL2_InitForOther()`. (CL3)

Not checked:
- Block-world drawing with the WebGPU backend, which isn't finished. (CL3)
- A page and its files published as an artifact: whether the Artifact tool
  takes `.data` as `application/octet-stream`, and whether the page's own
  `<html>` is kept as it is. The design's findings say a `.wasm` beside an
  artifact's page loads. (CL5, with the owner)
- GitHub Pages serving the demos, and its compression. (CL6, after the owner
  turns Pages on)
- A `resize` step with the canvas sized by CSS (CL4 keeps tools on
  `canvas=fixed`, so none needs it).
- Block-world's frames on SwiftShader against lavapipe's, and how long
  `walk.txt` takes there. (CL3)
- Mouse look and pointer lock by hand. (The owner)

## CLs

CL1 and CL2 can start now, in order. CL4 can start once *Files on the web*'s
CL1 has merged, and runs beside the rest. CL3 waits for *WebGPU render
backend*'s CL8, CL5 for CL3, and CL6 and CL7 for CL3 and *WebGPU render
backend*'s CL9.

### CL1 [ ] examples: block-world's shaders compiled by the build

Depends on: nothing.

- `examples/block-world/shaders/`: `block.vert`, `block.frag`, and
  `block_pool.vert`, moved from `assets/shaders/`.
- `examples/block-world/CMakeLists.txt`: `gb_embed_shader()` for each, as in
  [Block-world's shaders](#block-worlds-shaders).
- `world_resources.cc`: shader code from the embedded data.
- README.md's "Running the BlockWorld example": no shader step.
- `docs/backlog.md`: *ImGui shaders compiled by the build* no longer depends
  on *Web demos* (its block-world half is done).
- A change to block-world on Windows: the same shaders, built in.

**Verify**
- Standard checks don't build block-world; it is built on Linux as in
  [Block-world on Linux](#block-world-on-linux).
- Linux: before this CL, with the `.spv` files compiled by hand, and after,
  from a checkout with none, `walk.txt` and `chunk_pool.txt` pass and save
  frames that are byte for byte the same, apart from the timing text (FPS
  and CPU per frame), which a second run of the same build shows varies on
  its own.
- A `workflow_dispatch` run passes, with `build-block-world` building it on
  Windows.

### CL2 [ ] CI: block-world on Linux

Depends on: CL1.

- `cmake.yml`: `build-block-world-linux`, as in [Checks](#checks), uploading
  `block-world-frames`.
- CLAUDE.md and README.md: block-world builds and runs on Linux, and how; the
  Cloud sessions job list.
- `docs/backlog.md`: *Block-world on Linux* comes out (this is it).

**Verify**
- A `workflow_dispatch` run passes, with the new job's artifact holding both
  scripts' frames, which match a cloud session's run of the same scripts.

### CL3 [ ] examples: block-world in the browser

Depends on: CL1, *WebGPU render backend* CL8, *Files on the web* CL1.

- `BlockWorldBackend` and its two files, as in
  [Block-world in a page](#block-world-in-a-page); `block_world.cc` without
  its Vulkan names.
- `gb_pack_assets()` for its fonts and textures.
- README.md: building block-world for the web.
- A change to block-world on Windows: its backend is created through
  `BlockWorldBackend`, as before.

**Verify**
- Standard checks. Linux: both scripts pass, with frames byte for byte as
  before, apart from the timing text.
- Web: block-world builds in Release; `walk.txt` passes with
  `tools/run-program.sh`, and its frames match the Linux frames (the same
  world, GUI, and camera, differing in multisampled edges and timing text).
  `chunk_pool.txt` passes too, run once with a long timeout, and its time is
  recorded.
- Sizes of the Release `.wasm` and `.data`, against [Sizes](#sizes).

### CL4 [ ] build: the page for a visitor

Depends on: *Files on the web* CL1.

- `CMake/GameBitsShell.html`, as in [The page](#the-page), with its comment.
- `tools/run-page.js`: `canvas=fixed` in every page's address.
- CLAUDE.md's Web section: what a page shows a visitor, and `canvas=fixed`.
- Web only: nothing changes on Windows or Linux.

**Verify**
- Web: `templates/sdl`'s CI run and render-check's scripts pass as before.
- A scratch page with packed assets, opened with a Playwright snippet: the
  title, the loading message with its progress while `.data` downloads, then
  none; the canvas follows the viewport as it is resized; with
  `canvas=fixed`, it keeps the program's size; an exit with code 1 shows the
  last lines of stderr; a right click opens no menu.
- The JSPI message, with the page's script replaced by one where
  `WebAssembly.Suspending` is undefined.

### CL5 [ ] tools: Claude artifacts

Depends on: CL3, CL4.

- `tools/package-artifact.sh`, as in [Claude artifacts](#claude-artifacts).
- CLAUDE.md's Web section: packaging and publishing an artifact.

**Verify**
- It packages render-check and block-world from Release web builds, and
  fails, naming the file, on a file over 15 MB (a padded `.data`) and on a
  build with threads.
- With the owner's go-ahead (asked through the orchestrator), the session
  publishes block-world as a private artifact, and reports what the Artifact
  tool took. The owner opens it and plays it.

### CL6 [ ] CI: the demos on GitHub Pages

Depends on: CL3, CL4, *WebGPU render backend* CL9.

- `.github/workflows/pages.yml` and `examples/web-demos/index.html`, as in
  [GitHub Pages](#github-pages).
- README.md's "Try it in a browser".
- The owner turns Pages on first (see [GitHub Pages](#github-pages)).

**Verify**
- A `workflow_dispatch` run of the Pages workflow on `main` (after merge,
  started by the orchestrator) publishes the site. A session opens the
  published `index.html` and both demos with Playwright in headless Chromium,
  and each draws its first frames with no errors.

### CL7 [ ] CI: block-world in a headless browser

Depends on: CL3, *WebGPU render backend* CL9.

- `cmake.yml`: `build-block-world-web`, as in [Checks](#checks), uploading
  `block-world-web-frames`.
- CLAUDE.md's Cloud sessions lists the job.

**Verify**
- A `workflow_dispatch` run passes, with the new job's frames matching
  CL3's.
