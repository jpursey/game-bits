# Graphics on Linux

Game Bits' SDL and Vulkan libraries build and run on Linux, as they do on
Windows, so graphics code can be developed and checked on Linux:
- A cloud session (which runs on Linux) builds render-check, runs its scripts,
  and checks the frames itself, with no GPU, as a local session does on
  Windows.
- CI runs render-check's scripts on Linux, the first CI job that draws
  anything.
- A GitHub devcontainer (such as a Codespace) can run graphical programs you
  interact with, viewed in the browser. This is for programs of ImGui's
  weight, such as render-check: software rendering is far too slow for
  block-world or anything like a game, which are out of scope.

On Windows, nothing changes but where CMake finds the Vulkan SDK.

## Behavior

- On Linux, every library builds, including `gb_sdl`, `gb_imgui_sdl`,
  `gb_pilot_sdl`, `gb_render_vulkan`, and `gb_render_vulkan_sdl`, given the
  Vulkan SDK and the packages CLAUDE.md lists. Without the Vulkan SDK, the
  Vulkan libraries are left out, as now.
- render-check and `templates/sdl` build and run on Linux, as an X11 or Wayland
  window. Block-world isn't built there (see Left for later).
- On a machine without a GPU, Vulkan runs on Mesa's lavapipe, a software
  driver, and a window needs an X server, which can be a virtual one (Xvfb).
- `tools/run-program.sh` runs a program with a pilot script on Linux, as
  `tools/run-program.ps1` does on Windows, in a virtual X server of its own so
  nothing appears on the desktop.
- `gb_base` gains a query for the process's memory, which render-check shows
  on every platform (it calls Windows directly today).
- No public API changes.

## Design

### What a trial build found

A throwaway build in a cloud session (Ubuntu 24.04, clang 18, 4 cores), with
SDL built on Linux and Vulkan from Ubuntu's packages:
- Every Game Bits library built with one error, and ctest passed. The error:
  Ubuntu's Vulkan headers (1.3.275) are older than the code needs, as
  `vulkan_backend.cc` uses `VK_KHR_load_store_op_none`. Windows CI uses SDK
  1.3.283. With 1.3.283's headers, everything built.
- `gb_pilot_sdl_test` passed both with no display and under Xvfb.
- render-check had two problems on Linux: it includes `<windows.h>` and
  `<psapi.h>` for its process memory, and `%lld` with an `int64_t` argument,
  which is `long` on Linux, fails `-Wformat`. Its other `%lld` already casts
  its argument to `long long`.
- Ubuntu's `glslc` (shaderc 2023.8) compiled render-check's shaders.
- Under Xvfb with lavapipe, render-check created its window, device, and
  swapchain, but failed its first frame: Ubuntu's validation layer (1.3.275)
  doesn't know `VK_KHR_load_store_op_none`, so it rejects the store op as an
  invalid value. With that extension's `EXT` form used instead, as a trial
  only, all seven render-check scripts passed, in 2 to 8 seconds each, and the
  frames matched what each script describes.

So everything Vulkan needs (headers, loader, validation layers, `glslc`) has to
come from one source, at least as new as Windows'.

### Vulkan from the SDK

Linux uses LunarG's Vulkan SDK, as Windows does, at the same version, which CI
sets once for every job (`VULKAN_SDK_VERSION`). It has the headers, loader,
validation layers, and `glslc` together, at matching versions. The version is
1.4.363.1, the latest: LunarG no longer offers Windows CI's 1.3.283 for Linux.

`tools/install-vulkan-sdk.sh <dir> <version>` installs it from LunarG's Linux
tarball, taking only the parts Game Bits uses (the headers, loader, validation
layer, and `glslc`, about 450 MB of the SDK's 1.9 GB), and each shell sources
its `setup-env.sh`, which sets `VULKAN_SDK`.

vulkan.hpp 1.4 marks results `[[nodiscard]]` and deprecates device layers and
the C form of the debug callback, so `vulkan_backend.cc` checks the results of
`resetCommandPool` and `resetFences`, stops passing layers to the device
(Vulkan has ignored them since 1.0), and takes vulkan.hpp's types in its
debug callback. The newer validation layer also checks that a semaphore
presenting waits on isn't signaled again before its image is acquired again,
which reusing one per frame in flight broke, so there is one per swapchain
image instead.

The alternatives:
- Ubuntu's packages: too old, as above, for both the headers and the layers,
  and their versions move with the distribution rather than with Game Bits.
- Submodules (Vulkan-Headers, Vulkan-Loader, Vulkan-ValidationLayers,
  shaderc): pinned like every other dependency, but the layers and shaderc
  (with glslang and SPIRV-Tools) are each a large build, far more than the rest
  of Game Bits together.

`gb_external_libraries()` defines the `Vulkan` target from CMake's own
`find_package(Vulkan)` on every platform, when `VULKAN_SDK` is set, which finds
the SDK through it on Windows and Linux alike, replacing the paths written for
the Windows SDK's layout. Without `VULKAN_SDK`, the Vulkan libraries are left
out, so a distribution's older Vulkan is never picked up by accident.
`gb_embed_shader` takes `glslc` from it too (`Vulkan_GLSLC_EXECUTABLE`), falling
back to the `PATH` as before.

Lavapipe comes from Mesa's `mesa-vulkan-drivers` package, the only part of
Vulkan that comes from Ubuntu. A machine with a GPU uses its own driver.

### SDL on Linux

SDL is already built from its submodule; the root `CMakeLists.txt` builds it
on Linux too. It needs the windowing headers at build time (it loads the
libraries at run time), from Ubuntu's packages: `libx11-dev libxext-dev
libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev
libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev libegl-dev
libdbus-1-dev libudev-dev`. With them, SDL builds both its X11 and Wayland
video drivers. Without them it still builds, with no way to open a window, so
CLAUDE.md lists them with the Linux commands.

SDL2's offscreen video driver has no Vulkan support, so a machine with no
display uses Xvfb (Ubuntu's `xvfb` package).

### Process memory

render-check shows the process's private memory, from
`K32GetProcessMemoryInfo`. It moves behind a small interface in `gb_base`,
with an implementation per platform:

```
// Returns the memory the process has committed for its own use, in bytes, or
// nothing where the platform can't report it.
std::optional<int64_t> GetProcessMemoryBytes();
```

- `win_process_memory.cc`: `PrivateUsage`, as render-check reads it today.
- `linux_process_memory.cc`: `RssAnon` plus `VmSwap` from
  `/proc/self/status`, the closest Linux measure (Linux commits lazily, so
  there is no exact match).
- `gen_process_memory.cc`: nothing, for any other platform.

`/proc` is Linux's, not POSIX's, so this is the first `linux_` file. The
workflow's C++ style gains the prefix beside `win_`, `posix_`, and `gen_`, and
the backlog's *Linux threads and fibers* uses it.

#### Names

- `GetProcessMemoryBytes()` (recommended): says whose memory and the unit.
- `GetPrivateMemoryBytes()`: Windows' term, but "private" means something
  narrower on Linux.
- `GetMemoryInUse()`: has no unit, and could be confused with render-check's
  GPU memory or `gb_alloc`'s allocators.

### Running programs on Linux

`tools/run-program.sh` takes the same options as `run-program.ps1` (`--exe`,
`--script` or `--steps`, `--arguments`, `--out-dir`, `--working-directory`,
`--timeout-secs`), and does the same: runs the program with `--pilot` and
`--pilot-output`, fails on a nonzero exit or a timeout, and prints the frames
saved, the log's errors, and the pilot's summary. Output goes to
`out/run/<program>/`.
- It runs the program in its own Xvfb (`xvfb-run`), so runs never appear on a
  desktop and work with no display at all.
- On a timeout, it saves each thread's stack to `hang-stacks.txt` with `gdb`
  first, as `dump-stacks.ps1` does on Windows; `tools/dump-stacks.sh` prints
  the same for any running program.

It is a bash script rather than the PowerShell one run with PowerShell on
Linux, which would be a new dependency for a ~100 line script, and
`dump-stacks.ps1` uses Windows' `dbghelp.dll`, so it needs a Linux version
either way.

### CI

The Linux jobs install the Vulkan SDK, lavapipe, Xvfb, and SDL's packages,
then build everything and run ctest under `xvfb-run`, so the SDL window tests
run. A `build-render-check` job builds render-check on Windows and Linux, as
its own project (as block-world's job does). On Linux, it also runs each of
its scripts with `run-program.sh`, and uploads the saved frames, so they can be
looked at from the run's page. A script fails on any error logged, including Vulkan
validation errors, so this checks rendering on every push to `main`.

Every job installs the SDK with a local composite action,
`.github/actions/vulkan-sdk`, cached by version: on Windows the whole SDK from
its installer, which also gives the Windows jobs `glslc` (see the backlog's
*Block-world and ImGui shaders compiled by the build*, which this unblocks),
and on Linux with `install-vulkan-sdk.sh`. No third-party action installs the
SDK at a pinned version on both platforms.

SDL's window tests pass both with no display (where SDL falls back to its
offscreen driver) and under Xvfb, which runs them through its X11 driver, as
programs run, so CI runs ctest under `xvfb-run`.

### Devcontainer

`.devcontainer/` defines an Ubuntu 24.04 container with everything a Linux
build needs (clang, CMake, Ninja, the SDK, SDL's packages, lavapipe, Xvfb, gdb)
and the devcontainers `desktop-lite` feature: a light desktop on a VNC server,
viewed in the browser through noVNC on a forwarded port. A program started
from the container's terminal appears on that desktop, and takes the mouse and
keyboard from the browser. The desktop is 1920x1080 in 24-bit color:
desktop-lite's default of 16-bit color fails the first frame Vulkan presents,
with an X `BadLength` error from `PutImage`, as Mesa's software swapchain sends
32-bit pixels.

In VS Code, the container installs CMake Tools and C/C++, and its settings set
up two CMake projects, Game Bits and render-check, each built with clang and
Ninja in the build directories the command line uses. So the IDE builds, runs,
debugs (gdb), and tests (the Testing view) as the commands do. The settings
live only in `devcontainer.json`, not in a `CMakePresets.json` or
`.vscode/settings.json`, which a local checkout's Visual Studio or VS Code
would also read. The container sets `GB_DIR` to its checkout, and `CC` and
`CXX` to clang, so the templates, and projects made from them, build there.

Lavapipe renders on the CPU, so this suits programs of ImGui's weight:
render-check's scripts ran at 30 to 45 frames a second at 1280x800 under Xvfb
on 4 cores, counting startup.

### To confirm

- Confirmed in CL1: the SDK's Linux tarball (1.4.363.1, as 1.3.283's is gone)
  runs on Ubuntu 24.04, and the `game-bits` cloud environment allows its
  download host, `sdk.lunarg.com`.
- Confirmed in CL1: no action installs the SDK at a pinned version on both
  platforms, so CI uses a local one.
- Confirmed in CL1: its validation layer accepts `VK_KHR_load_store_op_none`
  with lavapipe, and every render-check script passes with it, in a trial
  build of render-check with CL2's changes.
- Confirmed in CL3: `gdb` can attach to the program from the script as an
  ordinary user, where Ubuntu's `ptrace_scope` only lets a process trace its
  own descendants. The script starts the program from a subshell that `exec`s
  `dump-stacks.sh`, which `exec`s `gdb`, so `gdb` is the program's ancestor.
  CI's hang check passes on GitHub's Ubuntu runner.
- Confirmed in CL4: Vulkan through SDL works on the devcontainer's VNC server
  as it does on Xvfb, once its color depth is 24-bit. render-check takes clicks
  and keys from noVNC, and closes cleanly from its window's close button.
- Whether render-check is responsive enough to use from the browser in a
  Codespace (CL4, checked by hand).

### Left for later

Each becomes a backlog item when this feature is complete:
- Block-world on Linux. It is 3D and too heavy for software rendering, and its
  shaders are compiled by a Windows batch file (the backlog's *Block-world and
  ImGui shaders compiled by the build*).
- Devcontainers for projects built on Game Bits, which need Game Bits checked
  out beside them. CL4's setup may be reusable from Game Bits.

## CLs

The status is `[ ]` not started, `[~]` in progress (set by the CL's first
edit), or `[x]` submitted (set in the CL's own commit). Each CL is checked on
Windows by CI on its PR.

### CL1 [x] build, CI: SDL and Vulkan on Linux

Depends on: nothing.

- `gb_external_libraries()` defines `Vulkan` from `find_package(Vulkan)` on
  every platform; `gb_embed_shader` finds `glslc` in the SDK.
- The root `CMakeLists.txt` builds SDL on every platform.
- `tools/install-vulkan-sdk.sh` installs the SDK on Linux.
- `vulkan_backend.cc` builds with vulkan.hpp 1.4, and has a render finished
  semaphore per swapchain image, as 1.4's validation layer requires.
- CI: every job installs the SDK at 1.4.363.1 with
  `.github/actions/vulkan-sdk`. The Linux jobs also install SDL's packages,
  lavapipe, and Xvfb, and run ctest under `xvfb-run`.
- CLAUDE.md's Linux and Cloud sessions sections: the packages, installing the
  SDK, and that every library builds there. README.md's platform notes.
- This cloud environment: `sdk.lunarg.com` allowed, and a setup script that
  installs the packages and the SDK, so later sessions start with them (an
  environment change you approve, not a file in the repository).

**Verify**
- Standard checks (see CLAUDE.md), with the Windows build and ctest by CI.
- On Linux, every library builds in Debug and RelWithDebInfo, and ctest
  passes as an ordinary user, with and without `xvfb-run`.

### CL2 [x] base, examples: process memory, and render-check on Linux

Depends on: CL1.

- `gb/base/process_memory.h`, with `win_`, `linux_`, and `gen_`
  implementations, and the `linux_` prefix in the workflow's C++ style.
- render-check uses it, and casts its `%lld` argument, so it builds on Linux.
- `templates/sdl` builds on Linux.
- `.gitignore` ignores `bin/`, which holds only built programs now that SDL's
  and flatc's binaries are gone from it.
- README.md: building render-check on Linux (`bin/render_check`). README.md
  and CLAUDE.md: render-check and the templates build on Linux.
- CI: `build-render-check-windows` and `build-render-check-linux` build
  render-check in Release. The Linux packages move to a local composite
  action, `.github/actions/linux-packages`, which both Linux jobs use.

**Verify**
- Standard checks.
- Unit tests: the query returns a positive value on Windows and Linux, which
  grows when the test commits a large allocation.
- render-check builds on Linux and Windows, and shows its process memory.

### CL3 [x] tools, CI: running programs on Linux

Depends on: CL2.

- `tools/run-program.sh` and `tools/dump-stacks.sh`, as under Running programs
  on Linux.
- CLAUDE.md's Run an example, and README.md's RenderCheck section, gain the
  Linux commands. `gdb` joins the Linux packages.
- CI's `build-render-check-linux` job runs render-check's scripts, and uploads
  the frames. It also checks that a script that hangs times out with its
  stacks saved.

**Verify**
- Standard checks.
- Every render-check script passes on Linux under lavapipe, and the session
  checks the frames.
- A script that hangs (`wait until` a condition that never comes) times out,
  and `hang-stacks.txt` shows the main thread's stack.
- CI's `build-render-check-linux` job passes and its frames can be downloaded.

### CL4 [x] devcontainer: graphical programs in the browser

Depends on: CL3 (gdb in the Linux packages).

- `.devcontainer/devcontainer.json` and its Dockerfile, as under Devcontainer.
- `tools/install-linux-packages.sh` installs the Linux packages, for CI, the
  Dockerfile, and CLAUDE.md, which each listed them before. It replaces CI's
  `linux-packages` action.
- `.gitattributes` checks out shell scripts with LF line endings, on Windows
  too, so the container runs them from a Windows checkout.
- README.md: opening the container, viewing its desktop in the browser, and
  building, running, debugging, and testing from VS Code.

**Verify**
- The session builds and starts the devcontainer with Docker and the
  devcontainers CLI. In it, as its ordinary user, the build and ctest pass,
  and render-check's scripts pass with `run-program.sh`.
- render-check runs on the container's desktop, viewed through noVNC in a
  browser (Playwright), and responds to its mouse and keyboard.
- Checked by hand: the same in a GitHub Codespace, and from VS Code there:
  building and running the unit tests, and building, running, and debugging
  render-check.
