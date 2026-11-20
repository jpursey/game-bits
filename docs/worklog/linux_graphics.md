# Graphics on Linux

Game Bits' SDL and Vulkan libraries build and run on Linux, as they do on
Windows, so graphics code can be developed and checked on Linux:
- A cloud session (which runs on Linux) builds render-check, runs its scripts,
  and checks the frames itself, with no GPU, as a local session does on
  Windows.
- CI runs render-check's scripts on Linux, the first CI job that draws
  anything.
- A devcontainer (such as a GitHub Codespace) builds, tests, runs, and debugs
  Game Bits, from its terminal or VS Code, and shows graphical programs on a
  desktop in the browser. This is for programs of ImGui's weight, such as
  render-check: software rendering is far too slow for block-world or anything
  like a game.

On Windows, nothing changed but where CMake finds the Vulkan SDK, and which
SDK version CI installs. There is no public API change, apart from the new
`GetProcessMemoryBytes()`.

## Behavior

- On Linux, every library builds, including `gb_sdl`, `gb_imgui_sdl`,
  `gb_pilot_sdl`, `gb_render_vulkan`, and `gb_render_vulkan_sdl`, given the
  Vulkan SDK and the packages `tools/install-linux-packages.sh` installs.
  Without the Vulkan SDK, the Vulkan libraries are left out of the build.
- render-check and `templates/sdl` build and run on Linux, in an X11 or Wayland
  window. Block-world isn't built there yet (see the backlog's *Block-world on
  Linux*).
- On a machine without a GPU, Vulkan runs on Mesa's lavapipe, a software
  driver, and a window needs an X server, which can be a virtual one (Xvfb).
- `tools/run-program.sh` runs a program with a pilot script on Linux, as
  `tools/run-program.ps1` does on Windows, in a virtual X server of its own so
  nothing appears on a desktop. `tools/dump-stacks.sh` prints a running
  program's thread stacks, as `dump-stacks.ps1` does.
- `gb_base`'s `GetProcessMemoryBytes()` reports the process's memory on every
  platform, which render-check shows.
- CLAUDE.md's Linux, Run an example, and Cloud sessions sections, and README.md,
  give the commands.

## Structure

### Vulkan from the SDK

Everything Vulkan needs (headers, loader, validation layers, `glslc`) comes from
LunarG's Vulkan SDK, as on Windows, at the same version on both, which CI sets
once for every job (`VULKAN_SDK_VERSION`, 1.4.363.1). Ubuntu 24.04's Vulkan
packages (1.3.275) are too old: the code uses `VK_KHR_load_store_op_none`,
which neither their headers nor their validation layer know. Building the
loader, layers, and shaderc from submodules was set aside as far more build
than the rest of Game Bits together.

`tools/install-vulkan-sdk.sh <dir> <version>` installs it from LunarG's Linux
tarball, taking only the parts Game Bits uses (the headers, loader, validation
layer, and `glslc`, about 450 MB of the SDK's 1.9 GB). Each shell sources its
`setup-env.sh`, which sets `VULKAN_SDK`. The SDK is only built for x86_64.

`gb_external_libraries()` defines the `Vulkan` target from CMake's own
`find_package(Vulkan)` on every platform when `VULKAN_SDK` is set, so a
distribution's older Vulkan is never picked up by accident. `gb_embed_shader`
takes `glslc` from it too (`Vulkan_GLSLC_EXECUTABLE`), falling back to the
`PATH`.

vulkan.hpp 1.4 marks results `[[nodiscard]]` and deprecates device layers and
the C form of the debug callback, so `vulkan_backend.cc` checks the results of
`resetCommandPool` and `resetFences`, passes no layers to the device (Vulkan
has ignored them since 1.0), and takes vulkan.hpp's types in its debug
callback. The 1.4 validation layer also checks that a semaphore presenting
waits on isn't signaled again before its image is acquired again, so the
backend has a render finished semaphore per swapchain image, rather than per
frame in flight.

Lavapipe comes from Mesa's `mesa-vulkan-drivers` package, the only part of
Vulkan from Ubuntu. A machine with a GPU uses its own driver.

### SDL on Linux

The root `CMakeLists.txt` builds SDL from its submodule on every platform. It
needs the windowing headers at build time (it loads the libraries at run time),
from Ubuntu's packages, and with them builds both its X11 and Wayland video
drivers. Without them it still builds, with no way to open a window.

SDL2's offscreen video driver has no Vulkan support, so a machine with no
display uses Xvfb. SDL's window tests pass both with no display (where SDL
falls back to its offscreen driver) and under Xvfb, where they use its X11
driver, as programs do, so ctest runs under `xvfb-run`.

### Linux packages

`tools/install-linux-packages.sh` installs every Ubuntu 24.04 package a Linux
build needs: clang, CMake, Ninja, SDL's windowing headers, lavapipe, Xvfb, and
gdb, plus `curl` and `xz-utils` for `install-vulkan-sdk.sh`. CI, the
devcontainer's image, and CLAUDE.md all use it, so the list is in one place.
`.gitattributes` checks out shell scripts with LF line endings everywhere, so
they also run in the devcontainer from a Windows checkout.

### Process memory

`gb/base/process_memory.h` declares `GetProcessMemoryBytes()`: the memory the
process has committed for its own use, in bytes, or nothing where the platform
can't report it. It has an implementation per platform:
- `win_process_memory.cc`: `PrivateUsage` from `K32GetProcessMemoryInfo`.
- `linux_process_memory.cc`: `RssAnon` plus `VmSwap` from
  `/proc/self/status`, the closest Linux measure (Linux commits lazily, so
  there is no exact match).
- `gen_process_memory.cc`: nothing, for any other platform.

`/proc` is Linux's, not POSIX's, so this was the first `linux_` file, and the
workflow's C++ style has the prefix beside `win_`, `posix_`, and `gen_`.

### Running programs on Linux

`tools/run-program.sh` takes the same options as `run-program.ps1` (`--exe`,
`--script` or `--steps`, `--arguments`, `--out-dir`, `--working-directory`,
`--timeout-secs`), and does the same: runs the program with `--pilot` and
`--pilot-output`, fails on a nonzero exit or a timeout, and prints the frames
saved, the log's errors, and the pilot's summary. Output goes to
`out/run/<program>/`.
- It runs the program in its own Xvfb (`xvfb-run`), so runs never appear on a
  desktop and work with no display at all.
- On a timeout, it saves each thread's stack to `hang-stacks.txt` with gdb
  first. Ubuntu only lets a process trace its own descendants, so the script
  starts the program from a subshell that `exec`s `dump-stacks.sh`, which
  `exec`s gdb, making gdb the program's ancestor. It works as an ordinary
  user, with no `sudo`.

They are bash scripts rather than the PowerShell ones run with PowerShell on
Linux, which would be a new dependency, and `dump-stacks.ps1` uses Windows'
`dbghelp.dll`, so it needed a Linux version either way.

### CI

Every job installs the SDK with a local composite action,
`.github/actions/vulkan-sdk`, cached by version: on Windows the whole SDK from
its installer, which also gives the Windows jobs `glslc`, and on Linux with
`install-vulkan-sdk.sh`. No third-party action installs the SDK at a pinned
version on both platforms. The Linux jobs install the packages with
`tools/install-linux-packages.sh`.
- `build-linux-debug` and `build-linux-release` build everything and run ctest
  under `xvfb-run`, as an ordinary user.
- `build-render-check-windows` and `build-render-check-linux` build
  render-check as its own project, in Release. On Linux, the job also runs
  each of its scripts with `run-program.sh` on lavapipe, and uploads the saved
  frames as the run's `render-check-frames` artifact. A script fails on any
  error logged, including Vulkan validation errors, so this checks rendering
  on every push to `main`. It also checks that a script that never ends times
  out with its stacks saved.

### Devcontainer

`.devcontainer/` defines an Ubuntu 24.04 container, for a GitHub Codespace or
VS Code Dev Containers, on an x86_64 host:
- Its image (`Dockerfile`) runs `install-linux-packages.sh` and
  `install-vulkan-sdk.sh`, and sets the SDK's build environment (`VULKAN_SDK`,
  `PATH`, and CMake's and pkg-config's paths) for every process, so the Linux
  commands work without sourcing `setup-env.sh`. It registers the SDK's loader
  with the dynamic linker, and installs its validation layer's manifest where
  the loader looks by default, rather than setting `LD_LIBRARY_PATH` and
  `VK_ADD_LAYER_PATH`, so programs find them however they are started,
  including from the desktop, whose environment is cleared (below).
- It sets `CC` and `CXX` to clang, and `GB_DIR` to the checkout, so the
  templates, and projects made from them cloned beside it, build against it.
- `updateContentCommand` checks out the submodules (blobless, so only the
  checked-out commit's files are fetched), so Codespace prebuilds include
  them.
- It lets gdb attach to the user's programs, and asks for 4 cores.

The devcontainers `desktop-lite` feature runs a desktop (fluxbox) on a VNC
server, viewed in the browser through noVNC on the forwarded Desktop port. A
program started from the container's terminal appears on it, and takes the
mouse and keyboard from the browser.
- The desktop is 1920x1080 in 24-bit color. At desktop-lite's default of 16,
  the first frame Vulkan presents fails with an X `BadLength` error from
  `PutImage`, as Mesa's software swapchain sends 32-bit pixels.
- desktop-lite starts the desktop through `sudo`, which clears the environment,
  so its own terminal runs programs but lacks the build environment. README.md
  says to build in VS Code's terminal.
- The desktop is dark. `fluxbox-init` gives fluxbox the `bora_black` style:
  its other dark styles set an image as the background, which pops up an error
  with no wallpaper tool. `gtk-settings.ini` gives the desktop's programs,
  such as its terminal (Tilix), GTK's dark Adwaita theme: the system's GTK
  settings name Yaru, which isn't installed, and GTK drops the dark variant
  when it falls back from a missing theme, so these name Adwaita.
  `tilix.gschema.override` sets Tilix's own variant, which it chooses itself,
  and is compiled when desktop-lite installs Tilix. desktop-lite only writes
  its own fluxbox settings when there are none, so the image's take their
  place.

In VS Code, the container installs CMake Tools and C/C++, and its settings set
up two CMake projects, Game Bits and render-check, each built with clang and
Ninja in the build directories the command line uses, so the IDE builds, runs,
debugs (gdb), and tests as the commands do. The settings live only in
`devcontainer.json`, not in a `CMakePresets.json` or `.vscode/settings.json`,
which a local checkout's Visual Studio or VS Code would also read. VS Code's
submodule detection is off, so Source Control shows only Game Bits' own
changes. CMake Tools' Testing view runs a test only once it is built (see the
backlog's *Tests registered with their programs' paths*).

### Confirmed

- render-check's scripts run at 30 to 45 frames a second at 1280x800 under Xvfb
  on 4 cores, and about 45 on the devcontainer's desktop in the cloud session.
  In a 4-core Codespace, viewed in the browser, it draws about 25 frames a
  second, which is responsive enough to use.
- render-check takes clicks and keys from noVNC, and closes cleanly from its
  window's close button.
- CI's hang check passes on GitHub's Ubuntu runner, as an ordinary user.
- In a Codespace, from VS Code: both CMake projects configure (with the
  [Unspecified] kit, which takes clang from `CC` and `CXX`), render-check
  builds, runs, and stops at breakpoints, and the unit tests pass from the
  CMake panel and the Testing view.

### Building blocks

- `GetProcessMemoryBytes()` (`gb/base/process_memory.h`).
- `tools/install-linux-packages.sh` and `tools/install-vulkan-sdk.sh`, for any
  Linux machine, CI job, or container that builds Game Bits.
- `tools/run-program.sh` and `tools/dump-stacks.sh`, the Linux counterparts of
  the PowerShell tools.
- `.github/actions/vulkan-sdk`, which installs the SDK at a pinned version on
  Windows and Linux.
- `.devcontainer/`, whose image a project built on Game Bits could start from
  (see the backlog's *Devcontainers for projects built on Game Bits*).
