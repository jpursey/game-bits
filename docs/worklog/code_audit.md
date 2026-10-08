# Code audit

Every Game Bits library is read through in tier order, looking for bugs,
missing tests, and optimizations that leave the public API unchanged. There is
no change in behavior beyond the bugs fixed.

## Design

Each library is one or more CLs, lower tiers first, so that a library is
audited after everything it depends on. For each one:
- Read the code and its tests.
- Write a failing test for each bug before fixing it.
- Add tests for untested behavior and edge cases.
- Make optimizations that are measurable, or plainly wasteful as written
  (needless copies, allocations, or locking).

Anything found that would change the public API, or is too large for the
audit, becomes its own backlog item instead, named in the library's CL.

Code that can't be unit tested (rendering, windows, input) is read for bugs
only, and any fix is checked by hand in an example.

The audit can stop and resume between libraries: each CL records what was found
in its library, and the next CL starts from the first `[ ]`.

## CLs

The status is `[ ]` not started, `[~]` in progress (set by the CL's first
edit), or `[x]` submitted (set in the CL's own commit).

### CL1 [x] test: audit gb_test

Depends on: nothing.

- `ThreadTester::RunThenSignal()` notified its signal directly when its
  function returned, so it crashed (Abseil's debug check on a second
  `Notify()`) if the signal had already been notified: by `Signal()`, by
  `Complete()` while the function was still running, or by another
  `RunThenSignal()` on the same signal. It now signals through `Signal()`,
  which only notifies once.
- `GenerateTestString()` and `GenerateAlphaTestString()` had no tests.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: `RunThenSignal()` on a signal that was already signaled, and the
  size, character range, and distinct adjacent characters of both test string
  generators.

### CL2 [ ] base: audit gb_base

Depends on: CL1.

### CL3 [ ] alloc: audit gb_alloc

Depends on: CL2.

### CL4 [ ] container: audit gb_container

Depends on: CL2.

### CL5 [ ] thread: audit gb_thread

Depends on: CL2.

### CL6 [ ] profile: audit gb_profile

Depends on: CL2.

### CL7 [ ] file: audit gb_file

Depends on: CL2.

### CL8 [ ] sdl: audit gb_sdl

Depends on: CL2.

### CL9 [ ] parse: audit gb_parse

Depends on: CL2.

### CL10 [ ] config: audit gb_config

Depends on: CL9.

### CL11 [ ] collide: audit gb_collide

Depends on: CL2.

### CL12 [ ] game: audit gb_game

Depends on: CL2.

### CL13 [ ] image: audit gb_image

Depends on: CL7.

### CL14 [ ] job: audit gb_job

Depends on: CL3, CL5.

### CL15 [ ] message: audit gb_message

Depends on: CL2.

### CL16 [ ] resource: audit gb_resource

Depends on: CL7.

### CL17 [ ] render: audit gb_render

Depends on: CL16.

### CL18 [ ] render: audit gb_render_vulkan and gb_render_vulkan_sdl

Depends on: CL17.

### CL19 [ ] imgui: audit gb_imgui and gb_imgui_sdl

Depends on: CL17.
