# Programs in the browser

A windowed program built on Game Bits (`gb_add_win_executable`) builds for
the web as a page: `<name>.html`, `<name>.js`, and `<name>.wasm`. Opened in a
browser, SDL's window is the page's canvas, the program gets keyboard and mouse
input from it, and its game loop runs as the blocking loop it is on every
platform, returning to the browser once a frame. A session or CI runs a page
in headless Chromium with `tools/run-program.sh`, as it runs a program on
Linux: with arguments and a pilot script, and the program's log, exit code, and
saved files brought back out. [Web support](../designs/web_support.md) is the
design for the whole project, with its scope and the decisions every feature
shares.

Nothing draws yet: drawing with `gb_render` is the next feature, *WebGPU
render backend*. Here `templates/sdl` is the program that proves it, and it
runs in a headless browser in CI. Programs in a browser are built and checked
without threads only.

On Windows and Linux, `Game`'s loops call the new frame wait, which does
nothing there, and `templates/sdl` becomes a `Game` that takes a pilot script
and quits on Escape.

## Behavior

- In a web build, `gb_add_win_executable` makes a page in `GB_BIN_DIR`
  (`<name>.html`, `.js`, `.wasm`), from Game Bits' own shell page: a canvas
  and the few lines that start the program. `gb_add_executable` and tests stay
  programs for Node.
- A page's arguments come from its address: each `arg` in the query string is
  one argument, in order, so `sdl_template.html?arg=--pilot&arg=/in/s.txt` runs
  it with `--pilot /in/s.txt`. `argv[0]` is the page's name.
- A program's files are in memory: its file system starts empty, apart from
  files a tool gives the page before `main()` (see [Page](#page)), and is gone
  when the page closes. Assets are *Files on the web*.
- Its stdout goes to the browser's console as messages, and its stderr (the
  log) as errors.
- `WaitForNextFrame()` (`gb/base/frame_wait.h`) waits, in a page, until the
  browser's next animation frame. That is when the page's input is delivered
  and what was drawn is shown, and it paces frames to the display. It returns
  whether it waited. Elsewhere, including under Node, it returns false at
  once. `YieldBetweenFrames()` returns to the browser once without waiting for
  the display, and elsewhere does nothing.
- `Game`'s loop calls `WaitForNextFrame()` as each frame begins. Where it
  waited, the display paces frames, and the loop doesn't limit them to
  `kKeyMaxFps` as well. A piloted `Game` calls `YieldBetweenFrames()` after
  each frame, so a script runs as fast as the program can, as it does
  elsewhere. A program with a loop of its own calls one of them each frame.
- When `main()` returns or `exit()` is called, the page stops: its canvas
  stays as it was last drawn. A crash or failed `CHECK` stops it the same way.
- What SDL gives a program in a page (see [SDL](#sdl)):
  - `SDL_CreateWindow` makes the canvas the window's size, and
    `SDL_SetWindowSize` resizes it, with SDL's window events. The page's own
    size doesn't change the window.
  - Keyboard, text, mouse motion, buttons, and the wheel arrive as events.
    The keyboard reaches the page only while it has focus: in an iframe (as a
    Claude artifact is), only once the user has clicked in it.
  - Relative mouse mode (`SDL_SetRelativeMouseMode`) locks the pointer at the
    next click or key press, as a browser allows it only from one.
  - `SDL_HideWindow` marks the window hidden, and takes the keyboard focus,
    but the canvas stays on the page.
  - `SDL_Delay` returns to the browser for at least 4 ms.
- `tools/run-program.sh --exe <dir>/<name>.html` runs a page, with the same
  options as for a program, in headless Chromium with WebGPU on SwiftShader,
  and saves the same files to `out/run/<name>/`. `--press-keys` presses real
  keys in a page instead of running a script.
- `templates/sdl`, on every platform, is a `Game` with the pilot flags, and
  Escape quits it. It builds for the web, and CI runs it there.

## Design

### Names

- `WaitForNextFrame()` waits for the platform's next frame where it paces
  them. `WaitForDisplay()` names what a browser waits for, but not what it
  means for the caller, and `AwaitFrame()` reads as JavaScript's `await`.
  Neither "Sleep" (`PreciseSleepFor()` waits on time) nor "Present" (drawing's
  word) fits. Recommended: `WaitForNextFrame()`.
- `YieldBetweenFrames()` returns to the platform without waiting.
  `YieldToPlatform()` doesn't say when it is called, and `ReturnToBrowser()`
  names one platform in shared code. It is close to `std::this_thread::yield()`
  in meaning, which it doesn't collide with. Recommended:
  `YieldBetweenFrames()`.
- `gb/base/frame_wait.h` declares both, with `gen_frame_wait.cc` and
  `web_frame_wait.cc` beside it.
- `gb_set_web_page_properties()` makes an executable a page, beside
  `gb_set_web_program_properties()`, which makes one a program for Node. A
  "page" is what a browser opens. "App" would collide with SDL's app events,
  and "html" names one of its three files.
- `CMake/GameBitsShell.html` is the page's shell, beside
  `CMake/GameBits.manifest`, named for Emscripten's `--shell-file`.
- `tools/run-program.sh` runs a page too, rather than a new script, as it
  takes the same options and saves the same files. Its browser half is
  `tools/run-page.js`, which Node runs with Playwright.
- `--press-keys` presses real keys; `--keys` would read as the pilot's key
  steps.

### Page

`gb_set_target_properties()` makes every executable it is given a program for
Node, as now, unless it is a windowed one (its `WIN32_EXECUTABLE` property,
which `add_executable(... WIN32 ...)` sets on every platform), which it makes a
page with `gb_set_web_page_properties()`. So a project's `add_executable(WIN32)`
followed by `gb_set_target_properties()` gets a page too. Nothing in Game Bits'
own build (`src/`) is windowed, so only examples, templates, and projects make
pages.

A page is linked with:
- `SUFFIX .html` and `--shell-file CMake/GameBitsShell.html`, which make
  Emscripten write the page from the shell beside its `.js` and `.wasm`.
- `-sJSPI`, so `main()` can wait for the browser (see
  [Frame wait](#frame-wait)). Only pages have it: a program for Node needs a
  flag to run JSPI in Node 24 (`--experimental-wasm-jspi`, without which it
  aborts as it loads).
- `-sENVIRONMENT=web`, without `-sNODERAWFS`, so its file system is in memory.
- `-sEXIT_RUNTIME=1`, `-sALLOW_MEMORY_GROWTH=1`, and `-sSTACK_SIZE=1MB`, as a
  program for Node has.
- `-sINVOKE_RUN=0` and `-sEXPORTED_RUNTIME_METHODS=FS,callMain`, so the shell
  starts `main()` once the files are in place, and a tool can read files back
  after it exits.

A web build with threads makes pages too, built with `-pthread` as everything
there is, but without `-sPROXY_TO_PTHREAD`, as SDL's window must be on the
browser's main thread. Nothing checks them (see *Threads in the browser*).

The shell is a page with a canvas whose id is `canvas` (SDL's driver uses
`#canvas`) and a script that sets Emscripten's `Module` before the program's
script loads:
- `arguments`: every `arg` in the page's query string, in order
  (`URLSearchParams.getAll('arg')`). An address holds any text, encoded, so
  there is no quoting to get wrong. `thisProgram` is the page's name.
- `print` and `printErr`: `console.log` and `console.error`, so stdout and
  stderr stay apart.
- `onRuntimeInitialized`: writes each file in `globalThis.gbFiles`, if a tool
  set it (a map from a path to its text or bytes), into the file system,
  creating its folders, then calls `callMain(arguments)`.
- `onExit` and `onAbort`: set `globalThis.gbExit` to the exit code, or to the
  abort's message, so a tool knows the program has stopped and how.

Nothing else: no styling beyond a black background and a canvas without a
border, no status text, no fullscreen button. A tool reads files back after
the program exits with `Module.FS`, from its own script, so the shell has no
part in it.

**Brittleness:** a project can't give a page its own shell or link options
without editing the target after Game Bits' command. That waits for a project
that needs it (*Web demos* may).

### Frame wait

```
// Waits until the platform's next frame, where the platform paces frames: in
// a browser page, this returns to the browser until its next animation frame,
// which delivers the page's input, shows what was drawn, and paces frames to
// the display. Returns whether it waited, in which case the caller doesn't
// limit its frame rate as well.
//
// Elsewhere, including a program run under Node, this returns false at once.
//
// In a page, this must be called from code that main() runs, not from a
// callback the browser makes (such as SDL's event handlers), which can't wait.
bool WaitForNextFrame();

// Returns to the platform once between frames, without waiting for the
// display, where the platform needs it: in a browser page, so the page stays
// responsive while frames run as fast as they can (as a piloted run's do).
// Elsewhere, this does nothing.
void YieldBetweenFrames();
```

- In `gb_base` (tier 0), as `Game`, `templates/sdl`, and any loop need it, and
  it needs nothing above `gb_base`.
- `gen_frame_wait.cc` returns false and does nothing, for Windows and Linux.
- `web_frame_wait.cc` awaits `requestAnimationFrame` for the first, and a
  `MessageChannel` message for the second, each in an `EM_ASYNC_JS` function,
  called only if `emscripten_has_asyncify()` (2 with `-sJSPI`, 0 without).
  An `EM_ASYNC_JS` function links without JSPI, and
  `emscripten_has_asyncify()` says at run time whether it can be called, so
  the wait is in `gb_base` itself. A unit test under Node links it and gets
  false, and only a page, linked with JSPI, waits. The orchestrator's proposal
  instead installed the wait from a source file only pages are compiled with;
  this needs no such file, and nothing for a project to forget.
- While `main()` waits, the browser runs its event handlers, including SDL's,
  which queue events, call SDL's event filter (`PilotSdl`'s), and return. They
  run only at the wait, so nothing else in the program sees them mid-frame.
- Cost: in a page, a `MessageChannel` round trip is about 13 us (2000 frames
  in 27 ms). `scheduler.yield()` costs the same, but isn't in Safari.
  `setTimeout(0)` and `SDL_Delay(0)` are clamped to 4 ms, so 2000 frames would
  take 8.4 s. Waiting for the display would take 33 s at 60 Hz.

**Brittleness:** a loop that never calls either function freezes its page: no
input arrives and nothing is shown. `Game`'s loops call them, so only a
program with its own loop (as `templates/sdl` was) must remember to, and it is
found the first time it runs in a browser. Calling one from a browser callback
traps, which is also found at once.

### Game

```
// GameLoop, as each frame begins:
const bool paced = WaitForNextFrame();
absl::Time now = clock_->Now();
// ... the frame limit as now, but only sleeping if !paced ...

// PilotLoop, after each EndFrame():
YieldBetweenFrames();
```

- In a page, the display paces frames, and `kKeyMaxFps` doesn't apply. The
  loop's limit sleeps with `Clock::SleepUntil`, which in a page without threads
  can't return to the browser, so it spins on the browser's main thread. With
  the default 60 frames a second on a 60 Hz display, measured in headless
  Chromium: the limit spent 9.5 to 16.7 ms of each frame spinning, which is
  nearly all of it, and one run fell to 43 frames a second, as the spin ran
  past the next animation frame. Without the limit, the same loop ran at 60.3
  frames a second, spending 17 us a frame after the wait. On a 144 Hz display,
  the limit would spin about 10 ms of every 16.7 ms frame to hold 60, and
  without it the program runs at 144. That is what a browser game does, and
  what `Update()`'s delta time is for.
- A limit below the display's rate in a page (waiting for whole animation
  frames until the frame is due) is the backlog's *Frame limits in a browser*.
- The pilot's loop doesn't wait for the display, as a piloted frame doesn't
  wait on real time anywhere. It yields so the page stays responsive: a
  piloted run in a browser a user is watching would otherwise freeze it until
  the script ended, and a long run would have Chromium offer to kill the page.
- On Windows and Linux nothing changes: `WaitForNextFrame()` returns false, so
  the limit runs as it does now, with whatever `Clock` the context gives.

**Brittleness:** a page's frame rate depends on the display, which a program
that counted on 60 frames a second gets wrong. Every Game Bits program reads
its delta time.

### SDL

Checked with a `Game` in a page, in headless Chromium 158, with Playwright's
keyboard and mouse:
- The pinned SDL2's Emscripten driver works as it is, and `sdl2_main` links
  and does nothing, as SDL doesn't rename `main()` there.
- `SDL_CreateWindow` (800x600, resizable) gives an 800x600 canvas, the window
  and pixel sizes 800x600, and keyboard and mouse focus.
- `SDL_SetWindowSize(640, 360)` makes the canvas 640x360 at once, and queues
  `SDL_WINDOWEVENT_SIZE_CHANGED`, so the pilot's `resize` step works. The
  browser window's own size doesn't reach the program, as the canvas has no
  CSS size. Sizing the canvas with CSS, which SDL follows for a resizable
  window, is for *Web demos* to decide.
- Keys, text, mouse motion, buttons, and the wheel arrive, with positions in
  canvas pixels.
- In an iframe, keys reach the page only after a click in it. Before that they
  go to the page around it.
- `SDL_SetRelativeMouseMode(SDL_TRUE)` returns 0 and requests the pointer lock
  at the next input event, as browsers require. After a click, the canvas had
  the lock, also inside an iframe, and motion came with relative movement.
  Playwright's mouse in pointer lock gives jumps (-200,-150, then back) that a
  real mouse wouldn't, so mouse look is only checked by hand, in *Web demos*.
- `SDL_HideWindow` clears `SDL_WINDOW_SHOWN` and SDL's keyboard focus (a
  frame later, SDL's hidden and focus lost events), but the canvas is still
  shown and keys still arrive. A pilot's `hide window` works, as `PilotSdl`
  gives its own input and drops the user's either way.
- `SDL_Delay` waits with `emscripten_sleep` where JSPI is linked, which
  returns to the browser: 4.2 ms for `SDL_Delay(1)`, and 4 ms for
  `SDL_Delay(0)`, as browsers clamp nested timers.
- `PilotSdl` and a pilot script work in a page: key, move, click, resize, run,
  and expect steps did what they do on Linux, and real keys pressed meanwhile
  were dropped.

Left as limits, none of which this feature's programs need:
- Escape exits pointer lock in a browser, and a real browser may keep the key
  from the page while it does, so a program that quits on Escape and locks the
  pointer can't use Escape for both.
- `SDL_WarpMouseInWindow` is unsupported.
- The window can't be hidden, moved, or made fullscreen except from a user
  gesture.
- High-DPI (`SDL_WINDOW_ALLOW_HIGHDPI`) scales the canvas by the device pixel
  ratio. No Game Bits program asks for it, and it isn't checked.

### Running a page

`tools/run-program.sh` runs a page when `--exe` names a `.html` file, with
the same options:
- It serves the page's folder from localhost on a free port (Node's `http`,
  with `.wasm` as `application/wasm`), and opens the page in Chromium with
  Playwright, headless, under `xvfb-run -a`, with the flags that give WebGPU a
  device on SwiftShader (`--enable-unsafe-webgpu --use-angle=swiftshader
  --enable-unsafe-swiftshader --ignore-gpu-blocklist --enable-gpu
  --enable-features=Vulkan --use-vulkan=swiftshader`), so the next feature
  needs nothing more.
- The script is put in the page's file system at `/pilot/script.txt` through
  `gbFiles`, and the page is given `--pilot /pilot/script.txt --pilot-output
  /pilot/out`, after `--arguments`, in its address.
- Console messages go to `<out-dir>/stdout.txt`, and console errors and page
  errors to `stderr.txt`, so the log's summary is printed as it is for a
  program.
- It waits for `gbExit`. An exit code is the run's; an abort is a failure
  (code 134, as a program that aborts gets on Linux), with its message. After
  `--timeout-secs` the run fails with the page's state left as it was.
  There are no thread stacks to save: a page's stack could be read through
  Chrome's debugger, which is the backlog's *Stacks of a hung page*.
- After the program exits, every file in `/pilot/out` is copied to
  `<out-dir>` with `Module.FS`, from the tool's script.
- `--press-keys "<key>..."` presses each key (by Playwright's names, such as
  `Escape`) with the real keyboard, once the page has run for a second, and
  runs the program without a script, as a pilot drops the user's input. It is
  for pages only. `--working-directory` is refused for a page, as a page's
  current directory is in its own file system.

Cloud sessions have Playwright 1.63.0 and its Chromium, which
`require('playwright')` finds from any directory (through `NODE_PATH`). CI
installs the same version, with its Chromium and the libraries it needs, in a
composite action `.github/actions/playwright` (`npm install
playwright@<version>` into a folder of its own, then `npx playwright install
--with-deps chromium`), cached by version, with `PLAYWRIGHT_VERSION` set once
in `cmake.yml`. It is a dependency of CI only, for web tools, which the owner
has agreed to.

**Brittleness:** `run-page.js` and the shell agree on `gbFiles`, `gbExit`, and
`Module.FS`, by name. Both are in Game Bits, and the template's CI run
fails if either changes alone.

### templates/sdl

The template becomes the smallest `Game` with an SDL window: render-check's
`main()` without drawing. Its `main()` runs a command line with the pilot
flags (`gb_cli`, `gb_pilot_script`), its `Game` makes the window and a
`PilotSdl` in `Init()`, and `Update()` reads SDL's events, returning false on
`SDL_QUIT` or Escape. Its loop is `Game`'s, so it gets the frame wait. On
Windows and Linux it behaves as now, apart from Escape and the flags. As
nothing builds either template in CI today, the CL that rewrites it adds a
job that builds both on Windows and Linux.

The orchestrator's design kept the template's own loop. It becomes a `Game`
because a pilot script is the only way to drive a page from a session, and
the template is the only windowed program that builds for the web until the
WebGPU backend: as a `Game` with a pilot, it checks the script, arguments,
exit codes, and the piloted loop in a browser in CI, and it starts a new
program the way render-check and block-world are built.

What waits for *WebGPU render backend*: `save` steps (the template draws
nothing, so a `save` fails), saved frames coming back through `/pilot/out`
(which this feature builds, and checks with a throwaway page), and WebGPU's
present and read back with the frame wait.

### To confirm

Checked in a spike outside the repository: a `Game` with an SDL window, the
pilot flags, `PilotSdl`, and a choice of frame waits, built with this
repository's web build (Emscripten 7.0.1) as a page from a shell like the
one above, run in headless Chromium 158 (Playwright 1.63.0) under `xvfb-run`
with the WebGPU flags:
- `EM_ASYNC_JS` links without JSPI, and `emscripten_has_asyncify()` is 0
  without it and 2 with `-sJSPI`. Without JSPI, Node 24 runs the program;
  with it, Node aborts as it loads unless given `--experimental-wasm-jspi`.
  (CL2)
- Arguments from the query string reach `main()`, files given before `main()`
  are read (the pilot script), files the program writes are read back with
  `Module.FS` after it exits, and exit codes arrive in `onExit`: 0, 1 for a
  failed pilot step, and 2 for a bad flag. A failed `CHECK` aborts, which a
  page error reports, without `onExit`. (CL1, CL4)
- Frame waits and limits: as in [Frame wait](#frame-wait) and [Game](#game).
  (CL2, CL3)
- SDL: as in [SDL](#sdl). (CL5)

Not checked:
- A page in a web build with threads: that it links with `-sJSPI` and
  `-pthread`. (CL1)
- `onAbort` setting `gbExit`: the spike saw the abort only as a page error.
  (CL1)
- A 144 Hz display, which headless Chromium doesn't have: the figures above
  are reasoned from the 60 Hz ones. (Checked by hand in *Web demos*.)
- A page as a Claude artifact. The design's findings say JSPI and pointer lock
  work there. (*Web demos*)
- Installing Playwright and its Chromium on a GitHub runner, and its cache.
  (CL6)
- A Release page's size. The spike's Debug `.wasm` was 36 MB, over an
  artifact's 15 MB a file. (*Web demos*)

## CLs

### CL1 [ ] build: windowed programs are pages

Depends on: *Web platform* CL1.

- `GameBitsTargetCommands.cmake`: `gb_set_web_page_properties()`, used by
  `gb_set_target_properties()` for a windowed executable, as in
  [Page](#page). The file's header comment says which executables are pages.
- `CMake/GameBitsShell.html`.
- CLAUDE.md's Web section: a windowed program is a page, and what its address
  takes.
- Unused in Game Bits' own build, which has no windowed program.

**Verify**
- Standard checks (the CMake change is for every platform).
- Web without threads: `templates/sdl` builds as a page, with its three files
  in `templates/sdl/bin`. A scratch page (in the session's scratchpad, built
  the same way) opened with a Playwright snippet gets its arguments from the
  address and a file from `gbFiles`, and sets `gbExit` to its exit code, and to
  the message of a failed `CHECK`.
- Web with threads: `templates/sdl` links.
- Web: `ctest` passes as before, so tests are still programs for Node.

### CL2 [~] base: the frame wait

Depends on: *Web platform* CL4.

- `frame_wait.h`, `gen_frame_wait.cc`, and `web_frame_wait.cc`, as in
  [Frame wait](#frame-wait), and `frame_wait_test.cc`.

**Verify**
- Standard checks.
- Unit test: `WaitForNextFrame()` returns false, and `YieldBetweenFrames()`
  returns, on Linux, Windows (CI), and under Node.
- Web: `gb_base_test` passes, so a Node program links the JSPI functions
  without JSPI.

### CL3 [ ] game: loops wait for the next frame

Depends on: CL2.

- `Game::GameLoop()` and `Game::PilotLoop()`, as in [Game](#game), and the
  class comment: in a page, the display paces frames, and `kKeyMaxFps` doesn't
  apply.
- A cross-platform change: one call each frame, which does nothing off the
  web.

**Verify**
- Standard checks. `gb_game_test`'s loop tests pass unchanged, on every
  platform and under Node.
- Web, with a scratch page (as in CL1) whose `Game` logs its frame rate: 60
  frames a second with `kKeyMaxFps` at 60, under 0.1 ms a frame after the
  wait, and a piloted run of 2000 frames in under a second.

### CL4 [ ] tools: run a page in a headless browser

Depends on: CL1.

- `tools/run-program.sh` and `tools/run-page.js`, as in
  [Running a page](#running-a-page). The script's header comment says how a
  page is run.
- CLAUDE.md's "Run an example": running a page, and what differs from a
  program (no thread stacks, `--press-keys`, files in memory).

**Verify**
- A scratch page (as in CL1) is run with `--steps`, `--script`, `--arguments`,
  and `--press-keys`: its log is in `stdout.txt` and `stderr.txt`, a file it
  writes to `/pilot/out` comes back to `out/run/<name>/`, and the tool's
  result follows its exit code, a failed `CHECK`, and a page that never exits
  (`--timeout-secs 5`).
- Linux: `render-check`'s scripts still pass as before.

### CL5 [ ] templates: templates/sdl as a Game, in the browser

Depends on: CL1 to CL4.

- `templates/sdl/src/main.cc` and its `CMakeLists.txt`, as in
  [templates/sdl](#templatessdl), and its README (or the template section of
  README.md): building it for the web, and running it with
  `tools/run-program.sh`.
- A cross-platform change: the template is a `Game` with the pilot flags, and
  Escape quits it.
- CI: nothing builds either template today. A `build-templates` job, on
  Windows and Linux (a matrix, as `build-render-check` is), builds
  `templates/cli` and `templates/sdl` in Release, each as its own project with
  `-DGB_DIR` set to the checkout, as `build-block-world` builds block-world.
  CLAUDE.md's Cloud sessions lists the job.

**Verify**
- Standard checks, and the template builds on Linux and runs there with
  `tools/run-program.sh --steps 'hide window; wait 30; key A; resize 640 360;
  wait 2'`, exiting cleanly.
- A `workflow_dispatch` run of the CMake workflow on the PR's branch passes,
  with `build-templates` building both templates on Windows and Linux.
- Web: the template builds as a page, and the same steps pass in the headless
  browser, with the pilot's summary in its log. A script whose step fails
  (`expect error "none" 1`) fails the run, and `--press-keys Escape` ends it
  with code 0.
- SDL in a page, as [SDL](#sdl) lists, with the template's log: window and
  canvas size, resize, keys and mouse.

### CL6 [ ] CI: the template in a headless browser

Depends on: CL5, *Web platform* CL8.

- `.github/actions/playwright`, and `PLAYWRIGHT_VERSION` in `cmake.yml`.
- A `build-web-template` job, beside CL5's `build-templates`, which builds
  the templates natively: it builds `templates/sdl` for the web (in Debug,
  with `.github/actions/emsdk`), and runs it with `tools/run-program.sh`: the
  steps from CL5, a failing script (expected to fail), and `--press-keys
  Escape`. It uploads `out/run/sdl_template` as an artifact.
- CLAUDE.md's Cloud sessions lists the job.

**Verify**
- A `workflow_dispatch` run of the CMake workflow on the PR's branch passes,
  with the new job, and a second run takes Playwright and its Chromium from
  the cache.

### CL7 [ ] docs: programs in the browser

Depends on: CL6.

- README.md: a windowed program on the web, how to build and open a page (any
  static server, such as `python3 -m http.server`, as a page can't be opened
  from a `file:` address), and the limits in [SDL](#sdl).
- CLAUDE.md's Web section, as the feature leaves it.

**Verify**
- The commands in each doc are run as written, in a cloud session.
