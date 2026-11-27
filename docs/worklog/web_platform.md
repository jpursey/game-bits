# Web platform

Game Bits builds for the web, as a third platform beside Windows and Linux: its
libraries compile to WebAssembly with Emscripten, and their unit tests run
under Node, in a cloud session and in CI. This is the foundation that programs
in a browser are built on. It covers the toolchain, the build, and every
library that doesn't open a window or draw, so it has nothing to see in a
browser yet. A window, input, the game loop, drawing with WebGPU, and files
each follow as a feature of their own: [Web support](../designs/web_support.md)
is the design for the whole project, with its scope and the decisions every
feature shares.

On Windows and Linux there is no change in behavior, apart from the new
`SupportsThreads()`.

## Behavior

- `emcmake cmake` configures a web build of Game Bits, or of a project built on
  it, with the Emscripten SDK that `tools/install-emsdk.sh` installs. It is
  built and tested with the same commands as any other build.
- A web build is either without threads, the default, or with them
  (`-DGB_WEB_THREADS=ON`). One build can't be both. A build without threads
  runs anywhere, including inside a Claude artifact. A build with threads only
  runs on a page served with cross-origin isolation headers.
- Every library builds for the web except `gb_render_vulkan`,
  `gb_render_vulkan_sdl`, `gb_job`, and `gb_profile`, which are left out, as
  the Vulkan libraries are on Linux without the Vulkan SDK. The job system
  needs fibers, and the profiler the x86 timestamp counter (`cpu_ticks.h`,
  which is left out of `gb_thread` with it).
- `ctest` runs each test program under Node. Tests that need what the web
  doesn't have are skipped there: death tests, and in a build without threads,
  tests that start threads.
- `gb_add_executable` makes a program that runs under Node (`node
  bin/<name>.js`), with the machine's own files and its arguments, so a command
  line program works as it does on any platform. A program in a browser is a
  later feature.
- `SupportsThreads()` (`gb/thread/thread.h`) says whether threads can be
  created, as `SupportsFibers()` does for fibers. It is false only in a web
  build without threads, where `CreateThread()` logs an error and returns null.
- What the web does less of than Windows, as Linux does today: there are no
  fibers, threads aren't named or pinned, and the process's memory isn't
  reported.
- CI builds and tests both web builds on every run.
- CLAUDE.md's Web and Cloud sessions sections, and README.md, give the
  commands.

## Design

### Names

- `web_` is the prefix for files that are the web's own, beside `win_`,
  `posix_`, `linux_`, and `gen_`. It names the platform, as the others do,
  where `wasm_` would name the processor and `emscripten_` the toolchain.
- `GB_WEB_THREADS` is the CMake option for a build with threads.
- `SupportsThreads()` follows `SupportsFibers()` in `gb/thread/fiber.h`.
- `GB_SKIP_WITHOUT_THREADS()` is the line that skips a test where there are
  no threads. `GB_REQUIRE_THREADS()` reads as if it failed the test, and
  `GB_THREADED_TEST()`, in place of `TEST()`, would need a form for each of
  GoogleTest's (`TEST_F`, `TEST_P`).
- Build trees are `out/build/web-Debug` and `out/build/web-threads-Debug`,
  beside `linux-clang-Debug`.

### Emscripten SDK

`tools/install-emsdk.sh <dir> <version>` clones Emscripten's `emsdk`
repository into `<dir>` and has it install and activate `<version>`: the
compiler, its tools, and the Node it runs tests with (24.19 for 7.0.1), about
1.6 GB. Each shell sources `<dir>/emsdk_env.sh`, as it does the Vulkan SDK's
`setup-env.sh`. CI sets the version once (`EMSDK_VERSION`) and caches the
install, with `.github/actions/emsdk`, as it does for the Vulkan SDK. The
`game-bits` cloud environment's setup script installs it in `/opt/emsdk`.

A web build is only checked on Linux, in sessions and CI. Emscripten also runs
on Windows, where its own instructions install it, but nothing here checks
that.

### Build

`emcmake cmake` sets CMake's toolchain file, which defines `EMSCRIPTEN`. The
root `CMakeLists.txt` and `GameBitsTargetCommands.cmake` do the rest, so a
project built on Game Bits needs nothing of its own:
- With `GB_WEB_THREADS`, everything is compiled and linked with `-pthread`,
  third-party code included. Game Bits' libraries pass it on to whatever links
  them, so a project's own targets get it too.
- Abseil's headers use Emscripten's old version macros, which Emscripten 7
  deprecates, and warnings are errors, so `-Wno-deprecated-pragma` is on for
  every target that uses Game Bits, until the backlog's *Abseil and GoogleTest
  update*.
- The Vulkan libraries are left out whether or not `VULKAN_SDK` is set, and
  so are `gb_job`, `gb_profile`, and `gb_thread`'s `cpu_ticks` files and their
  tests.
- An executable or test is a program for Node: it is linked to use the
  machine's own files and environment (`-sNODERAWFS=1`), to exit with `main()`'s
  result (`-sEXIT_RUNTIME=1`), and with memory that grows
  (`-sALLOW_MEMORY_GROWTH=1`). With threads, `main()` runs on a thread of its
  own (`-sPROXY_TO_PTHREAD`), so it can start threads and wait for them as it
  does on any platform.
- `gb_add_test` registers its test as `add_test(NAME ... COMMAND ...)`, which
  CMake runs through the toolchain's emulator (Node) when cross-compiling. The
  form it uses today names a file, which a web build doesn't have.
- `flatc` is built for the web as well, and the build runs it under Node,
  where it reads and writes the machine's files as the tests do.

### Tests

GoogleTest has no death tests on Emscripten, and defines `EXPECT_DEATH` only
where it does. Its own `EXPECT_DEATH_IF_SUPPORTED` runs a death test where
there are death tests and skips it elsewhere, so every `EXPECT_DEATH` becomes
that. `EXPECT_DEBUG_DEATH`, used in two files, has no such form, so those
tests are compiled only where `GTEST_HAS_DEATH_TEST` is defined.

SDL's web video driver needs a browser, so a test that makes a window fails
under Node (two of `gb_pilot_sdl_test`'s do). SDL's dummy video driver serves
them: with `SDL_VIDEODRIVER=dummy` in the environment, which a Node program
sees, every SDL test passes. `gb_add_test` sets it for each test in a web
build. SDL's real web driver is first used by *Programs in the browser*.

### Threads

```
// Returns true if threads can be created. This is false only in a web build
// without threads, where CreateThread() logs an error and returns null.
bool SupportsThreads();
```

- In `gb/thread/thread.h`, beside `CreateThread()`. The build picks the
  implementation: `gen_thread.cc`, on `std::thread`, wherever there are
  threads, and `web_thread.cc`, with none, for a web build without them, where
  `GetMaxConcurrency()` is 1.
- Code that can't work without a thread says so when it is created, with a
  `CHECK` on `SupportsThreads()`, rather than failing inside the standard
  library: `ThreadMessageDispatcher` and `gb_test`'s `ThreadTester`.
  `PollingMessageDispatcher`, which both examples use, needs none.
- A mutex or a condition variable works in a build without threads, as there
  is nothing to wait for.
- A test that starts a thread begins with `GB_SKIP_WITHOUT_THREADS();`
  (`gb/test/thread_tester.h`), which skips it where `SupportsThreads()` is
  false. `gb_test` depends on `gb_thread` for that, both in tier 0. About 60
  of the tests a web build runs do, most of them in `gb_test_test`,
  `gb_message_test`, and `gb_thread_test`.
  A test without the line that starts a thread stops at a `CHECK`, so a
  missing one is found the first time the test runs in a build without
  threads, which CI does.

**Brittleness:** a program that starts a thread works on every other platform
and fails in a web build without threads. The `CHECK`s make that failure name
its cause, and CI building both web builds catches it in Game Bits itself, but
a project only finds out by building for the web.

### 32 bits

`Queue` allocates each bucket's elements directly after its `Bucket`, whose
alignment is a pointer's. With 32-bit pointers that is less than a `uint64_t`
element needs, which a `static_assert` already catches (`gb_render`'s
`MeshPool` fails to compile). `Bucket` takes the alignment of its elements, if
that is more.

### To confirm

Trial builds with Emscripten 7.0.1, with and without threads, and with the
fixes below patched in roughly, found what CL2 to CL7 fix. They built `gb_job`
and `gb_profile` too, with CPU ticks read from a clock, before those were
left out. With threads, 19 of the 22 test programs passed under Node, and
only the five tests below failed. Without threads, everything compiled, and
the tests that failed were those five and the ones that start threads.
Recorded here as it was found:
- Every third-party library builds as it is: Abseil, RE2, GoogleTest,
  FlatBuffers (and `flatc`, which the build ran under Node), glm, libccd, and
  SDL2.
- Configuring takes about 150 seconds, mostly Emscripten compiling CMake's
  feature checks, and a full build is 866 steps.
- `AllocatorTest.SystemAllocatorAligns` fails: `std::aligned_alloc()` returns
  null for an alignment smaller than a pointer. (CL4)
- `LocalTimeTest` doesn't compile: Emscripten's standard library has no
  `std::chrono::current_zone()`. Whether `absl::LocalTimeZone()` finds the
  machine's zone under Node, and in a browser, is unchecked. (CL4)
- `SleepTest.SleepsOnSeveralThreadsAtOnce` fails: a sleep ends before the
  clock says its time has passed. (CL4)
- `LocalFileProtocolTest.ListSkipsNonUtf8Names` fails: Node gives a name that
  isn't UTF-8 with U+FFFD in place of its bad bytes, so it is listed. (CL5)
- `CommonProtocolTest.ReadOnlyFile` fails, as it does on Linux as root, which
  the trial ran as. (CL5 checks it as an ordinary user.)
- `LocalFileProtocolTest.CreateWithRootPath` fails without threads only.
  (CL5)
- `gb_pilot_sdl_test` stops at `screen is not defined`, in the two tests that
  resize a window. All 23 pass with `SDL_VIDEODRIVER=dummy`. (CL7)
- Without threads, `gb_job_test` doesn't link: `FiberJobSystem`'s semaphore
  (from `concurrentqueue`) calls `sem_timedwait()`, which Emscripten only has
  with threads. (`gb_job` is left out.)
- Emscripten's default stack is 64 KiB, for `main()` and each thread. Nothing
  overflowed it, but nothing deep was looked for. (CL1)
- Abseil reports "Symbolization unavailable" once in every test program: the
  pinned copy needs an Emscripten option that was removed in 4.0.14. It is
  harmless. (*Abseil and GoogleTest update*)

Not yet checked:
- A build on a Windows machine.
- `templates/cli` built for the web. (CL9)

## CLs

### CL1 [ ] build: Emscripten and a web build

Depends on: nothing.

- `tools/install-emsdk.sh`.
- The root `CMakeLists.txt` and `GameBitsTargetCommands.cmake`:
  `GB_WEB_THREADS`, the flags in [Build](#build),
  `add_test(NAME ... COMMAND ...)`, and the libraries left out of a web build.
- CLAUDE.md: a Web section with the install, configure, build, and test
  commands, saying which libraries don't build yet.
- The libraries that don't compile for the web yet still don't, so a web build
  as a whole fails until CL4.

**Verify**
- Standard checks, as `add_test` changes for every platform.
- Web builds with and without threads: the test programs that already build
  (`gb_collide_test`, `gb_config_test`, `gb_game_test`, `gb_image_test`,
  `gb_message_test`, `gb_parse_test`, `gb_pilot_script_test`,
  `gb_resource_test`, `gb_test_test`) pass under `ctest`, apart from tests
  that start threads in the build without them.
- A test program whose stack use is deepest (`gb_parse_test`) passes with the
  stack sizes chosen.

### CL2 [x] tests: death tests only where GoogleTest has them

Depends on: nothing.

- `EXPECT_DEATH` becomes `EXPECT_DEATH_IF_SUPPORTED` in the test files that
  use it. The tests using `EXPECT_DEBUG_DEATH` in `pilot_test.cc` are compiled
  only where there are death tests, and `sdl_glog_test.cc`'s, which also
  checks what a release build logs, splits on `NDEBUG` so that check runs
  everywhere.
- CLAUDE.md's Conventions say to use it.

**Verify**
- Standard checks. On Linux, the same number of tests run and pass as before,
  and a death test still fails when its statement doesn't die.

### CL3 [ ] container: queue buckets aligned for their elements

Depends on: nothing.

- `Queue::Bucket` in `queue.h`, as in [32 bits](#32-bits).

**Verify**
- Standard checks.
- Unit test: a queue of elements aligned to more than a pointer holds them at
  their alignment.
- Web: `gb_container_test` and `gb_render_test` pass (with CL1 and CL2).

### CL4 [ ] base: gb_base on the web

Depends on: CL1, CL2.

- `AllocAligned()` for an alignment smaller than a pointer.
- `web_local_time.cc`, if `absl::LocalTimeZone()` doesn't find the zone, and
  `local_time_test.cc` without `std::chrono::current_zone()` on the web.
- `PreciseSleepFor()` on the web: why it returns early by the clock, and
  `web_sleep.cc` if it needs one.
- workflow.md's C++ style: the `web_` prefix, with the first `web_` file.

**Verify**
- Standard checks.
- Web: `gb_base_test` passes with threads.

### CL5 [ ] file: local files under Node

Depends on: CL1, CL2.

- `GetNonUtf8Name()` says there is no such name on the web, and the tests that
  use one are skipped there.
- `LocalFileProtocolTest.CreateWithRootPath` without threads, and
  `CommonProtocolTest.ReadOnlyFile`, if it fails as an ordinary user.

**Verify**
- Standard checks.
- Web: `gb_file_test` and `gb_cli_test` pass, run as an ordinary user.

### CL6 [ ] thread, test, message: builds without threads

Depends on: CL1 to CL5.

- `SupportsThreads()` and `web_thread.cc`, as in [Threads](#threads).
- The `CHECK`s in `ThreadMessageDispatcher` and `ThreadTester`.
- `GB_SKIP_WITHOUT_THREADS()`, at the top of each test that starts a thread.

**Verify**
- Standard checks.
- Unit tests: `SupportsThreads()` is true, and `CreateThread()` works, wherever
  the tests already start threads.
- Web without threads: every test program builds, and passes with only the
  tests that start threads (and death tests) skipped. `CreateThread()` returns
  null and `GetMaxConcurrency()` is 1.
- Web with threads: every test program still passes.

### CL7 [ ] build: SDL's tests under Node

Depends on: CL1, CL2.

- `gb_add_test` sets `SDL_VIDEODRIVER=dummy` for each test in a web build.

**Verify**
- Standard checks.
- Web: `gb_sdl_test` and `gb_pilot_sdl_test` pass under `ctest`, with and
  without threads.

### CL8 [ ] CI: web builds

Depends on: CL1 to CL7.

- `.github/actions/emsdk`, and `EMSDK_VERSION` in `cmake.yml`.
- `build-web` and `build-web-threads` jobs, which build in Debug and run
  `ctest` as an ordinary user.
- The `game-bits` cloud environment's setup script installs the SDK with
  `tools/install-emsdk.sh` (proposed to the user, as it is outside the
  repository).
- CLAUDE.md: Cloud sessions lists the jobs and the SDK's place, and Checks say
  when a change is also checked in a web build.

**Verify**
- A `workflow_dispatch` run of the CMake workflow on the PR's branch passes,
  with both new jobs.
- A second run takes the SDK from the cache.

### CL9 [ ] templates, docs: a command line program on the web

Depends on: CL8.

- `templates/cli` builds for the web and runs under Node. Its README says how.
- README.md: the web among the platforms, the Emscripten SDK among the
  dependencies, and building for the web.

**Verify**
- Standard checks.
- `templates/cli` is configured with `emcmake cmake`, built, and run under
  Node with arguments and a file to read: its output matches the Linux
  build's.
