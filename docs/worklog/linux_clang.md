# Linux and Clang

Game Bits builds and passes its tests on Linux with clang, as well as on Windows
with MSVC, so it can be developed and checked in cloud sessions (which run on
Linux) and on more than one platform. It is the first step toward building
everywhere with clang, including clang-cl on Windows later.

The Windows build didn't change: MSVC with Ninja, the same configurations, and
the same CI job. Everything here is portable C++ fixes, CMake that only applies
off Windows, platform-specific code where the platforms differ, and a Linux CI
job. There is no public API change.

## Behavior

- On Linux, `cmake -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`
  configures, builds, and tests every library except those needing SDL or
  Vulkan (`gb_sdl`, `gb_imgui_sdl`, `gb_pilot_sdl`, `gb_render_vulkan`,
  `gb_render_vulkan_sdl`) and the examples, which need them. Those are left
  out of the build where SDL or the Vulkan SDK isn't available, rather than
  failing it. `gb_render` itself builds, and is tested against its fake
  backend. CLAUDE.md's Linux section has the commands.
- Platform-specific behavior on Linux comes from the `gen_` implementations.
  Where they do less than Windows (fibers are unsupported, so the fiber job
  system isn't available; threads aren't named or pinned to cores), the tests
  already account for it.
- `LocalFileProtocol` handles a path that isn't valid in the operating system's
  encoding the same way on every platform: `Create` and `CreateTemp` fail for
  such a root, and listing skips such a name without logging an error. On
  Linux that is a name that isn't valid UTF-8; on Windows, one with a lone
  UTF-16 surrogate.
- Tests of Windows file system behavior (names that ignore case, open files
  that can't be moved or deleted) only run on Windows. Linux allows both, so
  there is nothing to check there.
- Tests of read-only files need an ordinary user, as root ignores read-only
  permissions. CI's runner is one; a cloud session runs its tests as one
  (see CLAUDE.md's Cloud sessions).

## Structure

### Build system

- `flatc`: Windows uses the checked-in `bin/flatc.exe`. Elsewhere,
  `FLATBUFFERS_FLATC_EXECUTABLE` is left unset, so flatbuffers'
  `BuildFlatBuffers.cmake` builds `flatc` from the submodule and uses that
  target.
- `gb_external_libraries()` only defines the `sdl2` and `Vulkan` targets where
  they exist (SDL2 on Windows, from its prebuilt binaries; Vulkan on Windows
  where `VULKAN_SDK` is set). Each module that needs one is only added when its
  target exists (`if (TARGET sdl2)`, `if(TARGET Vulkan)`), so on Windows
  nothing changed.
- MSVC-only options are conditional on MSVC, such as `/bigobj` on
  `validated_context_test.cc`.
- Implicit conversion warnings are off for every Game Bits target and anything
  that links one, mainly for third-party headers: C4244, C4267, and C4800 on
  MSVC, and clang's counterparts on clang and clang-cl, including clang 21's
  `-Wcharacter-conversion` where clang has it. `gb_imgui` also turns off
  clang's `-Wnontrivial-memcall`, which ImGui's headers fail (see the
  backlog's *ImGui update*).

### Portable C++

What clang rejected that MSVC accepted, fixed in place: `std::nullptr_t` in
`weak_ptr.h`, `<cstdint>` in `unicode.cc`, `typename` on dependent base types
and access to the base's protected `data` in `container/array.h`, a defaulted
`operator<=>` over bit fields in `parse/parse_types.h`, `std::clamp` mixing
`long long` with `int64_t` (`long` on Linux) in the file protocols, and a
missing include in `imgui_instance.h`.

### Local file paths

`gb/file/local_path_encoding.h` (in `gb::internal`) converts between UTF-8
paths and `std::filesystem::path`, in place of `std::filesystem`'s own
conversions, which throw for a path that isn't valid:
- `LocalPathToUtf8()` returns an empty string for a path that can't be
  converted, and `Utf8ToLocalPath()` an empty path for a string that isn't
  valid UTF-8.
- `GetNonUtf8Name()` returns a name the operating system accepts but that
  can't be converted, for tests of how such names are handled.
- `win_local_path_encoding.cc` converts to and from UTF-16;
  `gen_local_path_encoding.cc` takes the path's bytes as UTF-8.

The local file tests are shared, and use these rather than their own
conversions. The Windows-only ones are in `win_local_file_system_test.cc`
(`WinLocalFileSystemTest`), which includes no Windows headers, as their macros
rename `FileSystem::MoveFile` and `CopyFile`.

### Test fixes

Two tests failed on Linux for reasons that weren't Linux's:
- `ProfilerMutexTest`: Abseil's cycle clock rate is measured at startup where
  the system doesn't report it, as in a VM, and varies slightly between runs.
  The tests made their waits by truncating times to whole cycles, and expected
  the times they started from. They now expect the time of the whole cycles
  they record.
- `RenderPassTest.DrawingAViewBeforeItsPassIsDrawnFails`: it enabled render
  asserts before creating its `RenderSystem`, whose constructor sets them from
  its debug option, so the death test never died in any Debug build. It now
  enables them after, as the other render tests do, and `render_assert.h`'s
  usage example says so.

### CI

`.github/workflows/cmake.yml` has a `build-linux` job beside the Windows one,
with the same triggers. It configures with clang, builds Debug (so render
asserts and death tests run), and runs ctest, as an ordinary user. It runs on
`ubuntu-latest`, as the Windows jobs run on `windows-latest`, so a newer clang's
warnings are fixed as they arise.

### Developing on Linux

CLAUDE.md's Linux section gives the commands to configure, build, and test, and
its Cloud sessions section says a cloud session builds and tests there itself,
leaving Windows to CI.
