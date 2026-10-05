# Game Bits

This is a shared C++ library defining common functionality used for desktop or command line applications. It is designed as a cross-platform library, with platform-specific details separated from public interfaces and headers.

This library is almost entirely self contained with all dependencies being brought in via submodules, with the exception being a dependency on the Vulkan SDK for graphics which must be installed on the machine. It currently only works on Windows, and is built with Visual Studio 2022 Community and CMake.

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
  bin/          -- Compiled binary files used by compilation or execution
  out/          -- Generated output from building locally. This is transient
                   and can get deleted at any time.
  assets/       -- Assets used at runtime by examples
  CMake/        -- Custom CMake rules used by all Game Bits modules in src/
  docs/         -- The shared workflow, feature plans (worklog/), and the
                   backlog
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

### Checks

Every change is checked as follows:
- It builds cleanly (warnings are errors) in both Debug and Release.
- `ctest` passes in Debug.
- Touched files pass the clang-format check (see Format above).
- New and changed behavior has unit tests. Code that can't be unit tested (rendering, windows, input) is checked by hand in an example, which the user runs.

## Sessions

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

The worktree has its own `out/`, so its first build in each configuration is a full build.

To land the change, leave the worktree first, keeping it (ExitWorktree with `keep`), since a session in a worktree can't run git against the main checkout. Then merge from the main checkout as above.

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
- `<target>_DEPS` is for other CMake targets in the build (including `absl::*`); `<target>_LIBS` is for external libraries (Vulkan, SDL).
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
- C++20, built with both MSVC and clang-cl.

## Don't
- Don't add or modify code outside src/gb/ without asking.
- Don't generate or build Visual Studio solutions; build with Ninja as described above.
