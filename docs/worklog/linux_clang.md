# Linux and Clang

Game Bits builds and passes its tests on Linux with clang, as well as on Windows
with MSVC, so it can be developed and checked in cloud sessions (which run on
Linux) and on more than one platform. It is the first step toward building
everywhere with clang, including clang-cl on Windows later.

The Windows build doesn't change: MSVC with Ninja, the same configurations, and
the same CI job. Everything here is portable C++ fixes, CMake that only applies
off Windows, and a second CI job.

There is no change in behavior on Windows. On Linux, the libraries that don't
need SDL or Vulkan build and pass their tests; those that do aren't built yet.

## Behavior

- On Linux, `cmake -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`
  configures, builds, and tests every library except those needing SDL or
  Vulkan (`gb_sdl`, `gb_imgui_sdl`, `gb_pilot_sdl`, `gb_render_vulkan`,
  `gb_render_vulkan_sdl`) and the examples. Those are left out of the build
  where SDL or the Vulkan SDK isn't available, rather than failing it.
- Platform-specific behavior comes from the existing `gen_` implementations,
  which nothing has built until now. Where they do less than Windows (fibers
  are unsupported, so the fiber job system isn't available; threads aren't
  named or pinned to cores), the tests already account for it.
- Tests that assume Windows file system behavior (names that ignore case, open
  files that can't be moved) are split so each platform checks its own.
- No public API changes.

## Design

### Compilers and platforms

- Linux is built with clang only. GCC isn't a goal, and isn't checked.
- The clang in CI is the one `ubuntu-latest` provides (18 today); this
  session's is also 18.
- Code stays portable as the workflow's C++ style requires: no new `#ifdef`
  around portable code. Nothing here needs a Linux-specific implementation;
  `gen_` covers everything that builds (CL3 adds `gen_local_path_encoding.cc`).
  Linux implementations where `gen_` falls short are left to the backlog (see
  below).

### Build system

- `flatc`: Windows keeps the checked-in `bin/flatc.exe`. Elsewhere,
  `FLATBUFFERS_FLATC_EXECUTABLE` is left unset, so flatbuffers' own
  `BuildFlatBuffers.cmake` builds `flatc` from the submodule and uses that
  target.
- MSVC-only options become conditional on MSVC, such as `/bigobj` on
  `validated_context_test.cc`.
- SDL2 is a prebuilt Windows binary in `third_party`, and Vulkan comes from the
  Vulkan SDK through `VULKAN_SDK`. `gb_external_libraries()` only defines the
  `sdl2` and `Vulkan` targets where they exist (SDL2 on Windows; Vulkan where
  `VULKAN_SDK` is set), and each module that needs one is only added when its
  target exists. On Windows both always exist, so nothing changes.

### Conformance fixes

What clang rejects that MSVC accepts, found by building with clang 18:
- `gb/base/weak_ptr.h`: bare `nullptr_t` instead of `std::nullptr_t`.
- `gb/base/unicode.cc`: `uint16_t` without `<cstdint>`.
- `gb/container/array.h`: `using Base::const_iterator` (and the other
  dependent types) needs `typename`, and `Array` reaches the base's protected
  `data`.
- `gb/parse/parse_types.h`: `TokenIndex`'s defaulted `operator<=>` compares
  `uint32_t` bit fields, which clang won't narrow to the `int` they promote to.
- `gb/file/memory_file_protocol.cc`, `gb/file/test_protocol.cc`:
  `std::clamp(position, 0LL, ...)` mixes `long long` with `int64_t`, which is
  `long` on Linux.
- `gb/imgui/imgui_instance.h`: uses `DrawList` without including
  `gb/render/draw_list.h`.

The `parse_types.h` and `array.h` errors were also in the backlog's *clang-cl
build* item, which CL1 drops them from.

### Tests on Linux

With the fixes above, a throwaway build in a cloud session built everything
but the SDL modules, and 7 of about 940 test cases failed:
- `LocalFileProtocolTest.MoveChangesCase`: assumes names ignore case. Linux
  file systems don't, so a move to a name differing only in case is a move to a
  different name.
- `LocalFileProtocolTest.MoveOpenFile`: assumes an open file can't be moved,
  which Windows enforces and Linux doesn't.
- `LocalFileProtocolTest.CreateWithRootPath`: `std::filesystem` throws
  converting the lone-surrogate path the test builds, which is only meaningful
  for UTF-16 (Windows) paths. CL3 found that `LocalFileProtocol` itself throws
  on Linux for a path that isn't valid UTF-8, as it converts paths with
  `std::filesystem`, which throws for an invalid one.
- `LocalFileProtocolTest.RecursiveDeleteFolderFailure`: assumes an open file
  can't be deleted, which Windows enforces and Linux doesn't.
- `CommonProtocolTest.ReadOnlyFile` (local files): the session ran as root,
  which ignores read-only permissions. It passes as an ordinary user.
- `ProfilerMutexTest.ContendedUnlockIsCountAndWait`: reads a wait of 7996 ns
  where it expects 8000 ± 2, apparently from converting between time and CPU
  ticks at a different tick rate.
- `RenderPassTest.DrawingAViewBeforeItsPassIsDrawnFails`: a death test that
  doesn't die.

Where the behavior is genuinely the platform's (case, moving or deleting open
files), the test moves to a `win_` test file, and what holds on every platform
stays in the shared one. `LocalFileProtocol` converts paths with
`local_path_encoding.h` rather than `std::filesystem`, implemented once for
Windows (UTF-16) and once in `gen_` (bytes taken as UTF-8), so a path that
isn't valid fails or is skipped on every platform. Its tests build an invalid
name in each platform's own encoding, in the `win_` and `gen_` test files. The
rest are investigated in their own CLs.

### CI

A second job in `.github/workflows/cmake.yml` on `ubuntu-latest`: configure
with clang, build Debug, and run ctest. It runs as an ordinary user, so the
read-only test means something. It has the same triggers as the Windows job
(pushes to `main`, and by hand).

### To confirm

- The read-only test passes as an ordinary user. CL3 found it does, running
  `gb_file_test` as a new user in the session, and CL5's CI job confirms it.
- `gb_sdl_test` passed on Linux, though `gb_sdl` names the Windows SDL2
  binaries: probably nothing in the test calls SDL. CL2 makes the question
  moot by leaving `gb_sdl` out where SDL2 isn't available.
- The two investigated failures (profiler, render pass) are real bugs or test
  assumptions, not something in the session. CL4 finds out.
  - CL2 found the profiler failure is intermittent: 1 run in 5 on Linux, so
    the test's result depends on something it doesn't control. If that is real
    time, the test breaks the workflow's rule that time comes from something
    the test controls, and CL4 fixes the test.

### Left for later

Each becomes a backlog item when this feature is complete:
- Linux implementations where `gen_` does less than Windows: fibers (for the
  fiber job system), thread names, and CPU topology and core pinning. These
  need a name prefix for Linux files alongside `win_` and `gen_`.
- SDL and Vulkan on Linux, then the examples and `tools/run-program` there.
  This adds a dependency on a Linux SDL2 and Vulkan SDK, so it needs a
  decision on where they come from.
- clang-cl on Windows: the existing *clang-cl build* item, which this makes
  much smaller.

## CLs

The status is `[ ]` not started, `[~]` in progress (set by the CL's first
edit), or `[x]` submitted (set in the CL's own commit). Each CL is checked on
Windows by CI on its PR.

### CL1 [x] base, container, parse, file, imgui: clang conformance fixes

Depends on: nothing.

- The fixes listed under Conformance fixes. They are small and the same kind
  of change in each library, so they are one CL rather than five.
- The *clang-cl build* backlog item drops the errors this fixes.
- No visible change.

**Verify**
- Standard checks (see CLAUDE.md), with the Windows build and ctest by CI.
- These files compile with clang 18 on Linux (the full Linux build needs CL2).

### CL2 [x] build: configure and build off Windows

Depends on: CL1.

- `flatc` from source off Windows; `/bigobj` only for MSVC.
- `sdl2` and `Vulkan` targets only where available, and the modules needing
  them only added when they are.
- No change on Windows.

**Verify**
- CI's Windows build and ctest pass, building the same targets as before.
- On Linux with clang, configure and build succeed in Debug and
  RelWithDebInfo, with every library but the SDL and Vulkan ones.
- ctest on Linux: everything passes except the failures under Tests on Linux.

### CL3 [x] file: local file tests on Linux

Depends on: CL2.

- Split the local file tests that check Windows behavior (case-insensitive
  names, moving or deleting open files) so each platform checks its own.
- `LocalFileProtocol` doesn't throw for a path that isn't valid UTF-8 on
  Linux: `Create` and `CreateTemp` fail, and listing skips it quietly, as on
  Windows. Path conversion moves to `local_path_encoding.h`, with `win_` and
  `gen_` implementations.
- Confirm the read-only test passes as an ordinary user.

**Verify**
- Standard checks.
- `gb_file_test` passes on Linux (as an ordinary user) and on Windows.

### CL4 [ ] profile, render: remaining Linux test failures

Depends on: CL2.

- Find and fix the cause of the profiler wait-time mismatch and the render
  death test. If either turns out to be a real bug, it may become its own CL.

**Verify**
- Standard checks.
- ctest passes in full on Linux.

### CL5 [ ] CI and docs: Linux job

Depends on: CL3, CL4.

- A Linux job in `cmake.yml` (clang, Debug, ctest), as under CI.
- CLAUDE.md gains the Linux commands (configure, build, test), and its Cloud
  sessions section says a cloud session builds and tests on Linux itself,
  leaving Windows to CI. README.md's platform notes say Linux builds the
  non-graphics libraries.

**Verify**
- Both CI jobs pass on the PR's branch.
- The Linux commands in CLAUDE.md work as written in a fresh session.
