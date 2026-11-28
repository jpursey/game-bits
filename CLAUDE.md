# Game Bits

This is a shared C++ library defining common functionality used for desktop or command line applications. It is designed as a cross-platform library, with platform-specific details separated from public interfaces and headers.

This library is almost entirely self contained with all dependencies being brought in via submodules, with the exception being a dependency on the Vulkan SDK for graphics which must be installed on the machine. It is developed on Windows, built with Visual Studio 2022 Community and CMake, and also builds on Linux with clang, apart from block-world (see Linux below).

It is used by other projects via direct inclusion based on the "GB_DIR" environment variable being set to this directory, and Game Bits specific CMake commands (see CMake/GameBitsTargetCommands.cmake for details)

See README.md for full context.

## Workflow

@docs/workflow.md

The workflow above is shared by Game Bits and every project built on it, which each import it from here. Everything below is specific to Game Bits.

## Directory Structure

This is a CMake project, starting at the root. The directory structure is as follows:
```
  src/          -- All source code for the library itself is under here
    gb/         -- All Game Bits original source code is in here
    stb/        -- Implementation files for the header-only STB library
  third_party/  -- Third-party code used by GameBits. These are either manually 
                   copied or unaltered git submodules to the external repository
                   (forks are avoided)
  examples/     -- Full runnable sample projects
  templates/    -- Starter projects that can be copied to start something new
  bin/          -- Example programs, built here locally (not checked in)
  out/          -- Generated output from building locally. This is transient
                   and can get deleted at any time.
  assets/       -- Assets used at runtime by examples
  CMake/        -- Custom CMake rules used by all Game Bits modules in src/
  tools/        -- Scripts used while developing, such as running a program
                   and capturing its window
  docs/         -- The shared workflow, feature plans (worklog/), designs for
                   projects of several features (designs/), and the backlog
```

## Commands

Everything is driven directly by CMake using the Ninja generator, which is exactly what Visual Studio's "open a local folder" CMake integration does (see CMakeSettings.json). Command line builds, IDE builds, and CI all use the same build.

### Developer environment (once per shell)

CMake and Ninja need an x64 MSVC developer environment; nothing below works without it.

```
# PowerShell
Import-Module "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath "C:\Program Files\Microsoft Visual Studio\2022\Community" -DevCmdArguments "-arch=x64 -host_arch=x64" -SkipAutomaticLocation

# cmd
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

To also run Debug test binaries, add the debug CRT to PATH in the same shell (see Test below).

### Configure

These match the `x64-Debug` and `x64-Release` configurations in CMakeSettings.json, so the IDE picks up whatever the command line configures and vice versa. Note that Visual Studio's "x64-Release" is `RelWithDebInfo`, not `Release`. Game Bits builds `RelWithDebInfo` with Release's inlining (`/Ob2` rather than CMake's `/Ob1`), for the whole build including the project using it (see the root `CMakeLists.txt`), so it runs as fast as Release.

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B out/build/x64-Debug
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -B out/build/x64-Release
```

Configuring takes about 5 seconds. CMake re-runs it automatically when a `CMakeLists.txt` changes.

If configure or build fails with a missing `cl.exe`, `rc.exe`, or Windows SDK path, the tree's cache is left over from an older Visual Studio or Windows SDK version. Delete the build tree and configure again (in the IDE this is "Delete Cache and Reconfigure").

### Build

```
cmake --build out/build/x64-Debug
cmake --build out/build/x64-Debug --target gb_base_test
```

A full build is 530 steps and takes about 35 seconds on a 32 core machine; incremental builds take a few seconds.

Game Bits code (everything under `src/gb`) compiles with warnings as errors (`/WX` on MSVC, `-Werror` on Clang). Third-party code does not.

Do not build through a generated Visual Studio solution (`-G "Visual Studio 17 2022"`) instead: it is roughly twenty times slower.

### Test

Tests are GoogleTest binaries registered with ctest, one per library (`gb_base_test`, `gb_parse_test`, ...).

```
ctest --test-dir out/build/x64-Debug --output-on-failure
ctest --test-dir out/build/x64-Debug --output-on-failure -R gb_base_test
```

To use GoogleTest flags, run the binary directly:

```
out/build/x64-Debug/src/gb/base/gb_base_test.exe --gtest_filter=ContextTest.*
```

Debug binaries link the non-redistributable debug CRT, which is not on PATH even in a developer shell, so every test exits with `0xc0000135` (DLL not found) until it is added:

```
# PowerShell
$env:PATH = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.44.35112\debug_nonredist\x64\Microsoft.VC143.DebugCRT;" + $env:PATH

# bash
export PATH="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC/14.44.35112/debug_nonredist/x64/Microsoft.VC143.DebugCRT:$PATH"
```

The `14.44.35112` version directory changes with Visual Studio updates. Only Debug needs this; the release CRT is already in System32.

### Format

The reference clang-format is the one that ships with Visual Studio (currently 19.1.5), which is also what Visual Studio's Format Document uses:

```
# PowerShell
$clangFormat = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe"
& $clangFormat -i <files>                  # format in place
& $clangFormat --dry-run -Werror <files>   # check only

# bash
CLANG_FORMAT="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang-format.exe"
"$CLANG_FORMAT" -i <files>
"$CLANG_FORMAT" --dry-run -Werror <files>
```

A `clang-format` on PATH may be a different version, and versions disagree on a few constructs (18 and 19 differ on bare `{ ... }` scope blocks), so use this one. Style comes from `src/.clang-format` (Google style); clang-format finds it automatically for any file under `src/`. Every file under `src/` passes the check, so formatting a touched file only changes the lines you edited.

### Run an example

A session can run an example with a script of steps, and look at the frames it saves. The example runs the script itself, so this works for any program that takes the pilot flags from `gb_pilot_script` (render-check and block-world do). `-Arguments` passes the program's other flags, such as block-world's `--state PlayState`. README.md says how to build each one (`bin/render_check.exe` in "Running the RenderCheck example").

```
# PowerShell
.\tools\run-program.ps1 -Exe bin\render_check.exe -Script examples\render-check\scripts\textures.txt
.\tools\run-program.ps1 -Exe bin\render_check.exe -Steps 'hide window; wait 30; click "Reset peaks"; wait 2; save peaks'

# bash, on Linux
tools/run-program.sh --exe bin/render_check --script examples/render-check/scripts/textures.txt
tools/run-program.sh --exe bin/render_check --steps 'hide window; wait 30; click "Reset peaks"; wait 2; save peaks'
```

On Linux, `tools/run-program.sh` takes the same options, spelled as `--exe`, `--timeout-secs`, and so on, and runs the program in a virtual X server of its own, so it needs no display (see Linux below).

`tools/run-program.sh` runs a page (a windowed program in a web build, `<name>.html`; see Web below) the same way, in headless Chromium through `tools/run-page.js`, with Playwright: `tools/run-program.sh --exe <dir>/<name>.html --steps 'hide window; wait 30'`. Its stdout and stderr are its console's messages and errors, and the frames it saves are copied out of its in-memory file system once it has stopped. What differs from a program: `--working-directory` can't be given, a hung page's stacks aren't saved, a page that aborts exits with code 134, and `--press-keys "A Escape"` presses real keys once the page has run for a second, instead of running a script (which would ignore them). Chromium is started with WebGPU on SwiftShader, so a page can draw without a GPU. Cloud sessions have Playwright and its Chromium already.

`-Steps` is a script written inline, with `;` between steps. `src/gb/pilot/script/pilot_script.h` lists the steps: they wait a number of frames or for a condition the program names, press and hold keys, move, click, and drag the mouse (on an ImGui item by its label, too), resize the window, run the program's commands, and save frames. Time is counted in frames on a fixed time step, so a run is the same every time. The user's real keyboard and mouse don't reach the program, and `hide window` keeps it off the desktop. The saved frames (`<name>.png`), and the program's stdout and stderr, go to `out\run\<program>\`. Read the PNGs to see what was drawn.

The program exits once the script ends, so every run also checks that it shuts down cleanly. The run fails if the program exits with an error code, which it does when a step fails or anything logs an error (unless the script expects it), or is still running 60 seconds after it started (`-TimeoutSecs`). In that case the stack of each of its threads is saved to `hang-stacks.txt` there first, showing where it is stuck. `tools\dump-stacks.ps1 -ProcessId <id>` prints the same for any running program, such as one the user says has hung, as does `tools/dump-stacks.sh <id>` on Linux, with gdb. Ubuntu lets a process debug only its own descendants, so for any other program `dump-stacks.sh` needs `sudo`; `run-program.sh` starts its program so that it doesn't.

### Linux

Game Bits also builds and passes its tests on Linux with clang, and render-check and the templates build there too, though block-world doesn't yet. It needs Ubuntu 24.04's packages for clang, CMake, and Ninja, SDL's windowing headers (without them SDL builds, but can't open a window), and to run graphical programs, Mesa's Vulkan drivers (including lavapipe, which renders on the CPU where there is no GPU) and a virtual X server (where there is no display), and gdb, which saves a hung program's stacks. `tools/install-linux-packages.sh` installs them, as CI and the devcontainer do:

```
sudo tools/install-linux-packages.sh
```

Vulkan comes from LunarG's Vulkan SDK, as on Windows, as Ubuntu's Vulkan packages are older than Game Bits needs. `tools/install-vulkan-sdk.sh` installs the parts of it Game Bits uses, once. Then each shell that builds or runs Game Bits sources its `setup-env.sh`, much as the developer environment on Windows, which sets `VULKAN_SDK`. Without it, the Vulkan libraries (`gb_render_vulkan`, `gb_render_vulkan_sdl`) are left out of the build.

```
tools/install-vulkan-sdk.sh ~/vulkan-sdk 1.4.363.1
source ~/vulkan-sdk/1.4.363.1/setup-env.sh
```

Then:

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -S . -B out/build/linux-clang-Debug
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -S . -B out/build/linux-clang-RelWithDebInfo
cmake --build out/build/linux-clang-Debug
xvfb-run -a ctest --test-dir out/build/linux-clang-Debug --output-on-failure
out/build/linux-clang-Debug/src/gb/base/gb_base_test --gtest_filter=ContextTest.*
```

A full build is about 990 steps, about 8 minutes on 4 cores. Test binaries have no `.exe`. `xvfb-run -a` runs the tests in a virtual X server, so SDL's window tests use its X11 driver, as programs do; where there is a display already, ctest runs on its own. Run tests as an ordinary user: root ignores read-only permissions, so `CommonProtocolTest.ReadOnlyFile` fails as root.

### Web

Game Bits also builds for the web with Emscripten, on Linux, and its tests run under Node. A web build is either without threads, the default, which runs anywhere, or with them (`-DGB_WEB_THREADS=ON`), which in a browser only runs on a page served with cross-origin isolation headers. One build can't be both. `tools/install-emsdk.sh` installs the Emscripten SDK, with the Node it runs programs with (about 1.6 GB), once. Then each shell that builds or runs a web build sources its `emsdk_env.sh`, which puts `emcmake` on the PATH and sets `EMSDK_NODE` to the SDK's Node (it puts that on the PATH too, but only where there is no `node` there already):

```
tools/install-emsdk.sh ~/emsdk 7.0.1
source ~/emsdk/emsdk_env.sh
```

Then, with `emcmake`, which configures with Emscripten's toolchain:

```
emcmake cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B out/build/web-Debug
emcmake cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DGB_WEB_THREADS=ON -S . -B out/build/web-threads-Debug
cmake --build out/build/web-Debug --target gb_parse_test
ctest --test-dir out/build/web-Debug --output-on-failure -R gb_parse_test
"$EMSDK_NODE" out/build/web-Debug/src/gb/parse/gb_parse_test.js --gtest_filter=LexerTest.*
```

Configuring takes about 3 minutes, mostly Emscripten compiling CMake's feature checks, and a full build (about 850 steps) 3 to 5 more on 4 cores. A program is `<name>.js`, beside its `<name>.wasm`, which Node runs with the machine's own files, environment, and arguments, and ctest runs each test that way, with `EMSDK_NODE`. SDL's web video driver needs a browser, so ctest runs each test with `SDL_VIDEODRIVER=dummy`. A test that resizes a window (in `gb_pilot_sdl_test`) needs it set by hand when run directly too: `SDL_VIDEODRIVER=dummy "$EMSDK_NODE" out/build/web-Debug/src/gb/pilot/sdl/gb_pilot_sdl_test.js`. `templates/cli` builds for the web the same way (see `templates/README.md`). A windowed program (`gb_add_win_executable`) is a page instead: `<name>.html`, beside its `.js` and `.wasm`, which a browser opens from a web server (not a `file:` address), with SDL's window as its canvas and its file system in memory. Each `arg` in the page's address is one of `main()`'s arguments, in order (`sdl_template.html?arg=--pilot&arg=/pilot/script.txt`), and its stdout and stderr go to the browser's console. `CMake/GameBitsShell.html`, the page around it, says what else a tool can give it. A web build leaves out `gb_render_vulkan` and `gb_render_vulkan_sdl`, `gb_job`, which needs fibers, and `gb_profile`, which needs the x86 timestamp counter (`cpu_ticks.h`, left out of `gb_thread` with it). Only a web build has `gb_render_webgpu`, the render backend there (`WebGpuBackend`), which draws into the page's canvas through Dawn's bindings for Emscripten (the `emdawnwebgpu` port, downloaded into the Emscripten SDK's cache the first time it builds), so a program that draws with it is a page. Its unit tests run under Node, which has no WebGPU; what it draws is checked by running a page with `tools/run-program.sh`.

The whole tree builds for the web, with and without threads, and every test program passes under ctest. In a build without threads, the tests that start threads are skipped.

A web build converts each shader `gb_embed_shader` embeds to WGSL, which WebGPU takes, with `spirv-opt` from the Vulkan SDK (which `tools/install-vulkan-sdk.sh` installs, so source its `setup-env.sh` here too) and `naga`, which Rust's `cargo` builds (about three minutes). `tools/install-naga.sh` installs it, once, and its `bin` goes on the PATH. Game Bits' own build embeds no shaders, so only a project that does (such as render-check) needs them:

```
tools/install-naga.sh ~/naga 30.0.2
export PATH=~/naga/bin:$PATH
```

### Checks

Every change is checked as follows:
- It builds cleanly (warnings are errors) in both Debug and Release.
- `ctest` passes in Debug.
- Touched files pass the clang-format check (see Format above).
- A change to anything a web build compiles (Game Bits' code, its third-party code, or the CMake files) is also checked in both web builds. A session on Linux (a cloud session, or the devcontainer) builds it without threads and with them, and `ctest` passes in each (see Web above). A session on Windows leaves them to CI, which checks both web builds on every run either way.
- New and changed behavior has unit tests. Code that can't be unit tested (rendering, windows, input) is checked in an example, such as `examples/render-check`. The session builds it, runs it with `tools\run-program.ps1`, or `tools/run-program.sh` on Linux (see Run an example above), and checks the saved frames itself. Only what the steps can't reach is left for the user to check by hand, and the hand-off says exactly what that is.

### Cloud sessions

A cloud session (see Cloud sessions in the workflow) runs on Linux, where it builds and tests with the Linux commands above, leaving Windows to CI:
- Its clone starts with empty submodules. `git submodule update --init` checks them out.
- Its environment (`game-bits`) installs the packages, the Vulkan SDK, and the Emscripten SDK above when it starts, with the Vulkan SDK in `/opt/vulkan-sdk` and the Emscripten SDK in `/opt/emsdk`. Each shell command is a new shell, so every command that configures, builds, or runs starts with `source /opt/vulkan-sdk/1.4.363.1/setup-env.sh &&`. Where they are missing (such as in another environment), it installs them as above.
- It runs as root, so it runs the tests as an ordinary user it adds (see Linux above), who owns the build tree: `useradd -m gbtest` once, then `chown -R gbtest out/build/linux-clang-Debug` and `su gbtest -c "xvfb-run -a ctest --test-dir out/build/linux-clang-Debug --output-on-failure"` from the clone. A web build is the same: it configures and builds as root, after `source /opt/emsdk/emsdk_env.sh &&`, since Emscripten keeps the libraries it compiles in its own cache in `/opt/emsdk`, which is root's, so linking as another user fails. Then `gbtest` owns the tree and runs its tests (`chown -R gbtest out/build/web-Debug`, then `su gbtest -c "ctest --test-dir out/build/web-Debug --output-on-failure"`), which needs nothing from the SDK on its PATH.
- It formats with the same clang-format version from PyPI, installed in a virtual environment in the session's scratchpad: `python3 -m venv <dir>`, then `<dir>/bin/pip install clang-format==19.1.5`, then `<dir>/bin/clang-format` as above.
- CI (`.github/workflows/cmake.yml`) runs nine jobs at once, in about 20 minutes (the Windows builds and tests): `build-windows-release` and `build-windows-debug` build and test on Windows, `build-block-world` builds block-world on Windows in Release, `build-render-check-windows` and `build-render-check-linux` build render-check in Release (on Linux, it also runs each of its scripts, and uploads the frames they save as the run's `render-check-frames` artifact), `build-linux-debug` and `build-linux-release` build and test with clang, as an ordinary user, and `build-web` and `build-web-threads` build with Emscripten in Debug, without threads and with them, and run the tests under Node. Every job but the web ones installs the Vulkan SDK with `.github/actions/vulkan-sdk`, at the workflow's `VULKAN_SDK_VERSION`, and the other Linux jobs install the packages above with `tools/install-linux-packages.sh`. The web jobs install the Emscripten SDK with `.github/actions/emsdk`, at the workflow's `EMSDK_VERSION`. It runs by itself for pushes to `main` that change more than docs. On a PR's branch it runs only when started by hand (a `workflow_dispatch` run of the CMake workflow on that branch), which the session does as the workflow's Cloud sessions describes. The PR's verification comment says that anything only an example run on Windows can check, such as block-world, is left for the user.

## Local sessions

These add to Local sessions in the workflow. A cloud session follows Cloud sessions there instead: it works in its own clone, so it needs no worktree, and its PRs reach `main` when the user merges them.

Projects build against this checkout as it stands (see Game Bits in the workflow), so Game Bits code is never changed in it directly:
- A Game Bits session works in its own git worktree, and builds and tests there (see Worktrees below).
- Once the user approves a change and it is committed on the worktree's branch, the session lands it on `main` from the main checkout: `git merge --ff-only <branch>`, or a cherry-pick if `main` has moved. Projects only ever build reviewed code.
- The only direct edits in the main checkout are the docs changes other projects' sessions may make (a backlog item, or the workflow). They are committed as soon as the user approves them, so they never block landing.
- Several Game Bits sessions can run at once, and each lands its own commits.

### Worktrees

Create the worktree from the local `main`, not `origin/main`, which is behind whenever the user hasn't pushed. Then switch the session into it (EnterWorktree with its `path`; EnterWorktree's `name` form branches from `origin/main`):

```
git worktree add -b <branch> .claude/worktrees/<branch> main
```

A new worktree has no submodules checked out, and configuring fails with "does not contain a CMakeLists.txt file" until they are. Initialize each one from the main checkout's copy, so almost nothing is downloaded. Run it once per submodule listed in `.gitmodules`, with the paths written out; the session refuses a shell loop over them:

```
git submodule update --init --reference <main checkout>/third_party/<name> -- third_party/<name>
```

The exception is a submodule marked `shallow = true` in `.gitmodules` (SDL), which git refuses to use as a reference ("reference repository ... is shallow"). Initialize it without `--reference`, which fetches only its pinned commit:

```
git submodule update --init -- third_party/SDL
```

The worktree has its own `out/`, so its first build in each configuration is a full build.

To land the change, leave the worktree first, keeping it (ExitWorktree with `keep`), since a session in a worktree can't run git against the main checkout. Then merge from the main checkout as above. If the change adds a submodule or moves one to another commit, run `git submodule update --init -- third_party/<name>` in the main checkout too, as projects build against it and a merge doesn't update submodules.

Once landed, remove the worktree from the main checkout. First check that `git status` in the worktree is clean, and that no shell is still inside it. `--force` is needed because it has submodules:

```
git worktree remove --force .claude/worktrees/<branch>
git branch -d <branch>
```

If removal fails with "Permission denied", git has already unregistered the worktree. Delete the leftover folder, then run `git worktree prune`.

## Build system

- Each library or executable is defined by a `CMakeLists.txt` in its own directory using the `gb_add_library` / `gb_add_executable` / `gb_add_win_executable` commands from `CMake/GameBitsTargetCommands.cmake`.
- New source and test files must be added to their module's `CMakeLists.txt` (`<target>_SOURCE` and `<target>_TEST_SOURCE`) or they will not be compiled.
- Defining `<target>_TEST_SOURCE` automatically creates a `<target>_test` executable that links GoogleTest/GoogleMock and registers a ctest test of the same name.
- `gb_embed_data(<target> <file> <header> <name>)` builds a file's bytes into a target as a `gb::EmbeddedData` (see `gb/base/embedded_data.h`), for data the program needs with nothing to open at run time.
- `gb_embed_shader(<target> <file> <header> <name>)` does the same for a GLSL shader, compiled to SPIR-V with `glslc` from the Vulkan SDK, for `RenderSystem::CreateShaderCode()`. In a web build, it embeds WGSL converted from the SPIR-V instead (see Web above), with GLSL's binding `n` at WGSL binding `2n + 1` for a sampler and `2n` for anything else; `gb_wgsl_bindings_test` tests that numbering on every platform. render-check's and block-world's shaders are built this way; gb_imgui's are not yet (see the backlog).
- `<target>_DEPS` is for other CMake targets in the build (including `absl::*`); `<target>_LIBS` is for external libraries (Vulkan).
- On Windows, every executable these commands define embeds `CMake/GameBits.manifest`, which makes its code page UTF-8, so `main()`'s arguments are UTF-8. A DLL gets its host's code page, so nothing else relies on it: code converts to and from UTF-16 when it calls Windows (see README.md).
- A module picks its platform implementations (see C++ style in the workflow) in an `if (WIN32) ... else() ... endif()` block at the top of its `CMakeLists.txt`, which sets `<target>_internal_SOURCE` and `<target>_internal_TEST_SOURCE` for the main lists to include, as in `src/gb/base/CMakeLists.txt`.
- Library tiers are described in README.md. A library may depend freely on lower tiers, minimally within its own tier, and never on a higher tier.

## Conventions

These add to the C++ style in the workflow.
- Formatting strictly driven by clang-format in Google style via src/.clang-format
- All Game Bits code is in the "gb" namespace. Anything in a header that only
  implements the public API (such as a `FooImpl` behind a `using Foo = ...`
  alias) goes in a nested `internal` namespace.
- Every file starts with the four line MIT copyright comment used everywhere in the tree, with the year the file was created.
- Headers use include guards of the form `GB_<DIR>_<FILE>_H_` (not `#pragma once`), and end with `}  // namespace gb` followed by `#endif  // GB_<DIR>_<FILE>_H_`.
- Include order: the file's own header first, then C/C++ standard headers in angle brackets, then third-party and Game Bits headers in quotes (`"absl/..."`, `"gtest/gtest.h"`, `"gb/..."`), with blank lines between groups.
- Unit tests live next to the code they test as `<file>_test.cc`, are written with GoogleTest/GoogleMock inside `namespace gb { namespace { ... } }`, and use the shared helpers in `gb_test` (`src/gb/test`) for threading and other cross-cutting test support.
- Death tests use `EXPECT_DEATH_IF_SUPPORTED` (or `ASSERT_DEATH_IF_SUPPORTED`), not `EXPECT_DEATH`, which GoogleTest defines only where it has death tests (not on the web). `EXPECT_DEBUG_DEATH` has no such form, so it goes inside `#ifdef GTEST_HAS_DEATH_TEST`, unless the test checks more in a release build than that the statement survives: then the test splits on `NDEBUG` instead, running the statement and its checks in release and `EXPECT_DEATH_IF_SUPPORTED` in debug (as `sdl_glog_test.cc` does).
- A test that starts a thread, directly or through the code it tests (such as a `ThreadTester` or a `ThreadMessageDispatcher`), begins with `GB_SKIP_WITHOUT_THREADS();` (`gb/test/thread_tester.h`), which skips it where `SupportsThreads()` is false: in a web build without threads. A fixture whose every test starts one may put it in `SetUp()` instead, if nothing that starts a thread is constructed before `SetUp()` runs (such as a `ThreadTester` member). Without it, the test stops at a `CHECK` in such a build, or aborts in the standard library if it constructs a `std::thread` itself.
- C++20, built with both MSVC and clang-cl.

## Skills

`.claude/skills/` holds skills on how to use parts of Game Bits, for sessions here and in projects built on it: `gb-context` (`Context`, `ValidatedContext`, and contracts) and `gb-parse` (lexers, grammars, and parse results). A change to what one describes updates the skill in the same CL.

## Don't
- Don't generate or build Visual Studio solutions; build with Ninja as described above.
