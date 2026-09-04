# Game Bits

This is a shared C++ library defining common functionality used for desktop or command line applications. It is designed as a cross-platform library, with platform-specific details separated from public interfaces and headers.

This library is almost entirely self contained with all dependencies being brought in via submodules, with the exception being a dependency on the Vulkan SDK for graphics which must be installed on the machine. It currently only works on Windows, and is built with Visual Studio 2022 Community and CMake.

It is used by other projects via direct inclusion based on the "GB_DIR" environment variable being set to this directory, and Game Bits specific CMake commands (see CMake/GameBitsTargetCommands.cmake for details)

See README.md for full context (note: its "Getting started" section is stale -- it still describes Visual Studio 2019 and the batch files, and its library list predates `gb_collide` and `gb_config`).

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
  bin/          -- Compiled binary files used by compilation or execution
  out/          -- Generated output from building locally. This is transient
                   and can get deleted at any time.
  assets/       -- Assets used at runtime by examples
  CMake/        -- Custom CMake rules used by all Game Bits modules in src/
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

These match the `x64-Debug` and `x64-Release` configurations in CMakeSettings.json, so the IDE picks up whatever the command line configures and vice versa. Note that Visual Studio's "x64-Release" is `RelWithDebInfo`, not `Release`.

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

Do not build through a generated Visual Studio solution (`-G "Visual Studio 17 2022"`) instead: it is roughly twenty times slower, and it makes the compiler crashes below happen constantly rather than rarely.

MSVC 14.44 intermittently crashes (`fatal error C1001`, occasionally `LNK1127`) in the optimizer, in a different translation unit each time. This is a toolchain problem, not an error in the code, and it only shows up in optimized (`Release` / `RelWithDebInfo`) builds -- Debug builds are reliable. Re-run the exact same command and the failing target compiles.

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

```
clang-format -i <files>              # format in place
clang-format --dry-run -Werror <files>   # check only
```

Style comes from `src/.clang-format` (Google style); clang-format finds it automatically for any file under `src/`. Only format files you actually touch: about 18 existing files were formatted with an older clang-format and would otherwise churn unrelated lines.

## Build system

- Each library or executable is defined by a `CMakeLists.txt` in its own directory using the `gb_add_library` / `gb_add_executable` / `gb_add_win_executable` commands from `CMake/GameBitsTargetCommands.cmake`.
- New source and test files must be added to their module's `CMakeLists.txt` (`<target>_SOURCE` and `<target>_TEST_SOURCE`) or they will not be compiled.
- Defining `<target>_TEST_SOURCE` automatically creates a `<target>_test` executable that links GoogleTest/GoogleMock and registers a ctest test of the same name.
- `<target>_DEPS` is for other CMake targets in the build (including `absl::*`); `<target>_LIBS` is for external libraries (Vulkan, SDL).
- Library tiers are described in README.md. A library may depend freely on lower tiers, minimally within its own tier, and never on a higher tier.

## Conventions

- Coding guidelines: Generally follows the Google C++ style guide (https://google.github.io/styleguide/cppguide.html).
- Formatting strictly driven by clang-format in Google style via src/.clang-format
- All Game Bits code is in the "gb" namespace.
- Every file starts with the four line MIT copyright comment used everywhere in the tree, with the year the file was created.
- Headers use include guards of the form `GB_<DIR>_<FILE>_H_` (not `#pragma once`), and end with `}  // namespace gb` followed by `#endif  // GB_<DIR>_<FILE>_H_`.
- Include order: the file's own header first, then C/C++ standard headers in angle brackets, then third-party and Game Bits headers in quotes (`"absl/..."`, `"gtest/gtest.h"`, `"gb/..."`), with blank lines between groups.
- Sections in a file are separated by //===== blocks (extending to column 80) surrounding descriptive text: One line section description, and if necessary further description in additional paragraphs.
- Sections within a class or between groups of related functions are separated by //---- blocks (otherwise the same as above).
- All comments are // style (not /// or /*...*/)
- Unit tests live next to the code they test as `<file>_test.cc`, are written with GoogleTest/GoogleMock inside `namespace gb { namespace { ... } }`, and use the shared helpers in `gb_test` (`src/gb/test`) for threading and other cross-cutting test support.
- Prefer Abseil (and other Google open source libraries already vendored in third_party/) over hand-rolled utilities.
- C++20, built with both MSVC and clang-cl.
- Files in the working tree use CRLF line endings (git `core.autocrlf` is true); leave them that way.

## Don't
- Don't add new dependencies without asking.
- Don't add or modify code outside src/gb/ without asking.
- Don't generate or build Visual Studio solutions; build with Ninja as described above.
- Don't reformat files you aren't otherwise changing.
