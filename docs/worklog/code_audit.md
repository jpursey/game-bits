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

New APIs may be added where a fix or test needs one. Anything found that would
break an existing public API, or is too large for the audit, becomes its own
backlog item instead, named in the library's CL.

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

### CL2 [x] base: audit WeakPtr and CallbackScope

Depends on: CL1.

- A `WeakLock` taken while `InvalidateWeakPtrs()` was waiting for existing
  locks blocked until it finished, rather than being null as documented. A
  thread that already held a lock and took another (directly, by copying it,
  or by one scoped callback calling another) deadlocked with the invalidating
  thread. New locks are now null as soon as invalidation starts.
- A moved-from `WeakScope` crashed in its destructor (and in
  `InvalidateWeakPtrs()`), as it dereferenced its moved-away data. A moved
  scope also kept pointing at the instance it was constructed with, so
  `WeakScope` is no longer movable. Nothing moved one, and any move crashed.
- A `WeakPtr` to a base class that isn't at the start of the scope's object
  (multiple inheritance) locked to the wrong address, as the pointer was cast
  through `void*`. Supporting it would make every `WeakPtr` half again as
  large, so it is a documented requirement instead, which `GetWeakPtr()`
  CHECKs.
- `WeakPtrTest.ThreadAbuse` and `CallbackScopeTest` waited on real time
  (sleeps). They now synchronize on signals and counts.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: locking (and copying a lock) while another thread invalidates,
  scoped callbacks calling each other while the scope is destroyed, and a
  `WeakPtr` to a base class at a different address (CHECK failure).

### CL3 [x] base: audit Context and ValidatedContext

Depends on: CL2.

- `Context` move assignment overwrote the target's values without destroying
  the owned ones or freeing their names, and self-move deadlocked. Each value
  now owns its name, so no path that removes values can leak one.
- Setting a value to the pointer it already held (`SetPtr()` then
  `SetOwned()`, or the reverse) kept the old ownership, contrary to the
  comment, leaking the value or deleting one the caller still owned.
- `ValidatedContext::Complete()` keeps a shared context alive until the
  `ValidatedContext` is destroyed or reassigned. This is left as it is, as
  `Assign()` depends on it when the same context is assigned again with new
  constraints, which now has a test.
- `ContextTest.ThreadAbuse` and `TypeInfoTest` waited on real time.
- `ThreadTester::WaitForLoopCount()` is new, waiting until a `RunLoop()` has
  run its function a number of times. It replaces the counters and signals
  the stress tests in CL2 and this CL each wrote by hand.

**Verify**
- Standard checks.
- Unit tests: move construction and assignment, ownership changes on the same
  pointer, assigning the same shared context again, and `WaitForLoopCount()`
  (across threads, with several waiters, and when the count isn't reached).

### CL4 [x] base: audit the rest of gb_base

Depends on: CL1.

- `Allocator::New()` and `TsAllocator`'s constructor took their arguments by
  value, so a constructor taking a reference bound to a temporary copy, and
  `NewArray()` moved the same arguments into every element, leaving all but
  the first moved-from.
- `TsAllocator` hid `Allocator::Alloc(size)`, as overriding `Alloc(size,
  align)` hides the other overload. Every other subclass does too, and fixing
  that in `Allocator` breaks subclasses, so it is the backlog item *Allocator
  overrides*.
- `StdAllocator` and `StdGlobalAllocator` didn't compile once a container used
  them.
- `ToUtf16()` and `ToUtf8()` threw on invalid input (utfcpp's checked
  conversions), rather than returning an empty string as documented.
- `IsPlatformSupported()` was defined outside `namespace gb`, so the declared
  function had no definition.
- `GetWindowsError()` kept the trailing line break from `FormatMessage()` in
  the middle of the message, and returned "no message" for any error whose
  message has inserts (such as `ERROR_WRONG_DISK`).
- `ScopedCall` was copyable, so a copy called the function twice.

**Verify**
- Standard checks.
- Unit tests: `Allocator` helpers with reference and move-only arguments,
  `StdAllocator` and `StdGlobalAllocator` in standard containers, invalid
  UTF-8 and UTF-16 conversions, `IsPlatformSupported()`, `GetWindowsError()`,
  and `ScopedCall`.

### CL5 [ ] alloc: audit gb_alloc

Depends on: CL4.

### CL6 [ ] container: audit gb_container

Depends on: CL4.

### CL7 [ ] thread: audit gb_thread

Depends on: CL4.

### CL8 [ ] profile: audit gb_profile

Depends on: CL4.

### CL9 [ ] file: audit gb_file

Depends on: CL4.

### CL10 [ ] sdl: audit gb_sdl

Depends on: CL4.

### CL11 [ ] parse: audit gb_parse

Depends on: CL4.

### CL12 [ ] config: audit gb_config

Depends on: CL11.

### CL13 [ ] collide: audit gb_collide

Depends on: CL4.

### CL14 [ ] game: audit gb_game

Depends on: CL4.

### CL15 [ ] image: audit gb_image

Depends on: CL9.

### CL16 [ ] job: audit gb_job

Depends on: CL5, CL7.

### CL17 [ ] message: audit gb_message

Depends on: CL4.

### CL18 [ ] resource: audit gb_resource

Depends on: CL9.

### CL19 [ ] render: audit gb_render

Depends on: CL18.

### CL20 [ ] render: audit gb_render_vulkan and gb_render_vulkan_sdl

Depends on: CL19.

### CL21 [ ] imgui: audit gb_imgui and gb_imgui_sdl

Depends on: CL19.
