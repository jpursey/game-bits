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

### CL5 [x] alloc: audit gb_alloc

Depends on: CL4.

- `TestAllocator::IsValidMemory()` compared the pointer against itself rather
  than each allocation, so it passed almost any pointer, and the pool tests'
  memory checks checked nothing.
- `PoolAllocator` didn't round its slots up to a multiple of their alignment,
  so with a size that isn't a multiple (24 bytes aligned to 16) every other
  slot was misaligned. Slots also weren't aligned for the free list pointer
  they hold when free (12 bytes aligned to 4). Slots are now at least
  pointer-aligned, so a pool accepts any alignment up to that.
- `TestAllocator` had no tests, and `test_allocator.cc` had no copyright
  header.

**Verify**
- Standard checks.
- Unit tests: `TestAllocator` (counts, failing allocations, memory inside,
  outside and misaligned), and pool slot alignment for sizes that aren't a
  multiple of the alignment.

### CL6 [x] container: audit gb_container

Depends on: CL4.

- `Array3d<Type, X, Y, Z>` dropped its Z size, so it was `X` deep instead.
- `Array` classes didn't satisfy Container as documented: `swap()` couldn't be
  called (it was inherited from a private `std::array` base), and there was no
  `==`. Each class now has both, for its own type only. Their `constexpr`
  constructor called `memset`, so no `Array` could be constant-initialized; it
  now value-initializes instead.
- Pushing to a full `Queue` that can't grow (or one constructed with no
  capacity) wrote through a null pointer. It is now a CHECK failure.
- Copying a `Queue` with spare capacity added one to the copy's capacity
  (counting the first bucket's extra slot), so each copy of a copy grew.
- `Queue` moves weren't `noexcept`, so a `std::vector` of queues copied every
  queue and its elements when it grew.
- `Queue` assignment and swap kept the destination's grow capacity, unlike
  construction. They now take it from the queue they copy, and swap exchanges
  it.
- `Queue` overloaded `std::swap`, which programs may not add to `std`. Its
  `swap()` is now found by argument-dependent lookup, and `std::swap()` still
  works through the moves.
- `Array` and `OffsetArray` had no tests. `Queue` is now also checked against
  `std::deque` through long runs of pushes, pops, and copies.
- `BufferView` is unchanged apart from missing includes and comment typos.
  `SetOrigin()` DCHECKs that the origin isn't negative, although the
  constructor allows it and nothing else depends on it.

**Verify**
- Standard checks.
- Unit tests: every `Array` class (initialization, indexing and memory order,
  dimensions, `fill()`, `swap()`, `==`), `OffsetArray` in each dimension, and
  `Queue` capacity after copies, grow capacity after assignment and swap,
  moves in a `std::vector`, pushing to a full queue (CHECK failure), and
  random pushes and pops against `std::deque`.

### CL7 [x] thread: audit gb_thread

Depends on: CL4.

- `SetThreadName()` and `SetFiberName()` used `std::max` where they meant
  `std::min`. They always copied 127 bytes, reading past short names (so a
  `string_view` into a longer string took the rest of it), and a name longer
  than 127 bytes overflowed the buffer. `FiberJobSystem` names fibers after
  their jobs.
- `SwitchToFiber()` marked the fiber it switched from as suspended before the
  thread had left its stack, and an exiting fiber was marked exited before it
  switched away. Another thread could switch to the fiber (or delete it) while
  it was still running, which crashed. A fiber now stays running until the
  next fiber on its thread has started, so `GetFiberState()` only reports a
  fiber suspended or exited once it is safe to switch to or delete.
- On Windows, a thread that had already exited when it was joined or detached
  leaked its handle, as the thread cleared the handle on exit.
- `GetThisFiber()` returned the startup data, rather than null, on the thread's
  own fiber that `CreateFiberThreads()` switches back to when a fiber exits.
- On other platforms, `gen_fiber.cc` defined an undeclared `IsFiberRunning()`
  instead of `GetFiberState()`, so nothing calling it linked, and
  `gen_thread.cc` didn't compile with thread logging disabled (release builds),
  as the disabled logging macro used `std::cout` without including it. Both
  were compiled by hand to check them, as they aren't built on Windows.
- The verbose logging flags were plain `bool`s read by every thread.
- `CreateFiberThreads()` documented a negative thread count as subtracting
  from the maximum concurrency twice.
- The `CreateFiber` tests waited on real time, and one fiber test didn't
  check for fiber support.

**Verify**
- Standard checks.
- Unit tests: long thread and fiber names, names that are part of a longer
  string, joining and detaching threads that have already exited (Windows
  handle counts), and switching fibers between 16 threads (fails every run
  without the fix, in both Debug and Release).

### CL8 [x] profile: audit gb_profile

Depends on: CL4.

- `ProfileCallHook::Call()` returned `auto`, which drops references, so a
  function returning a reference couldn't be hooked (and one returning a const
  reference would have returned one to a temporary). It returns
  `decltype(auto)`, as `FunctionHook`'s example Hook now does too.
- A `slow_frame` time longer than an `int64_t` of ticks holds (but not
  infinite) overflowed converting to ticks, which reported every frame as slow.
- The rest of the library was already well tested, and reads no real time in
  its tests.

**Verify**
- Standard checks.
- Unit tests: hooking a function that returns a reference, and a slow frame
  time too long to reach.

### CL9 [x] file: audit paths

Depends on: CL4.

gb_file was audited as three CLs, in dependency order: paths, then `File` and
chunk files, then `FileSystem` and the protocols.

- `NormalizePath()` kept a `..` above the root of an absolute path (`/..`,
  `mem:/../a`, `//host/..`), so a `FileSystem` path could reach outside a
  protocol's root: `DeleteFolder("file:/..", kRecursive)` would delete the
  parent of `LocalFileProtocol`'s root. A `..` at the root of an absolute path
  is now removed, as the root has no parent. `kRequireRoot` still fails on it,
  as it did, and a relative path keeps its leading `..` segments.
- `PathMatchesPattern()` matched each part between `*`s at its last place in
  the path, so a part that appeared again later failed the rest of the
  pattern: `"*/*/*.png"` didn't match `"a/b/c.png"`. Folder listings filter
  with it.
- `JoinPath()` doubled the separator after the second path's host when the
  first path was absolute or empty (`JoinPath("/x", "//b/y")` was
  `"//b//x/y"`).
- The `RemoveProtocol()` comment said it returns an empty string without
  protocol flags, rather than the path unchanged.
- `IsRootPath()` is true for an empty path and for a protocol on its own
  (`"mem:"`), which aren't absolute. That is fixed in CL11, after protocols
  reject relative paths, as it is all that stops `DeleteFolder("")` from
  deleting a protocol's root today.

**Verify**
- Standard checks.
- Unit tests: `..` at the root of absolute, protocol, and host paths (and
  still failing with `kRequireRoot`), patterns whose parts repeat in the path,
  joining under the second path's host, and `IsRootPath()`, which had no
  tests.

### CL10 [x] file: audit File and chunk files

Depends on: CL9.

- `File::ReadLine()` and `ReadLines()` passed their buffer to `append()` as a
  C string, so a line was cut at its first NUL byte, and the next line's
  offset into that shorter string threw (fatal without exceptions).
- Seeking an invalid `File` made it valid again, though the header says every
  operation fails once it is invalid.
- A `ChunkWriter` given data it doesn't own, whose size isn't a multiple of 8,
  returned extra data offsets that didn't count the padding written after the
  data, so read pointers landed short of their data.
- `ChunkReader` move assignment leaked the data it replaced (as did
  `std::vector<ChunkReader>` operations that assign).
- `std::hash<ChunkType>::operator()` wasn't `const`, so a `ChunkType` couldn't
  be a key in a standard unordered container, as documented.
- A chunk larger than an `int32_t` holds overflowed into a negative size. It
  is now a CHECK failure. An empty owned chunk no longer allocates zero bytes,
  which `Allocator` leaves undefined.
- `ReadChunkFile()` accepted a file header with version 0, which
  `ChunkReader` treats as corrupt in every other chunk.
- `File::ReadRemainingLines()` copied the whole text once more to normalize
  line endings before splitting. It now splits on any line ending directly,
  with an `absl::StrSplit` delimiter that reads `\r\n` as one ending.
- The `ChunkWriter` example didn't compile, and the chunk header comment said a
  single chunk's count is 0 (it is 1).
- Left as is: a zero-byte chunk of unowned data is written with a count of 1,
  which the reader rejects as corrupt; `ConvertToPtr()` trusts the offset
  (documented as a precondition).

**Verify**
- Standard checks.
- Unit tests: lines with NUL bytes, seeking an invalid file, unowned unaligned
  data with extra data, `ChunkReader` move assignment (Debug heap counts, so
  MSVC Debug only), `ChunkType` in a `std::unordered_set`, a chunk too large
  (CHECK failure), a version 0 chunk file, and mixed line endings in one file.

### CL11 [ ] file: audit FileSystem and the protocols

Depends on: CL10.

### CL12 [ ] sdl: audit gb_sdl

Depends on: CL4.

### CL13 [ ] parse: audit gb_parse

Depends on: CL4.

### CL14 [ ] config: audit gb_config

Depends on: CL13.

### CL15 [ ] collide: audit gb_collide

Depends on: CL4.

### CL16 [ ] game: audit gb_game

Depends on: CL4.

### CL17 [ ] image: audit gb_image

Depends on: CL11.

### CL18 [ ] job: audit gb_job

Depends on: CL5, CL7.

### CL19 [ ] message: audit gb_message

Depends on: CL4.

### CL20 [ ] resource: audit gb_resource

Depends on: CL11.

### CL21 [ ] render: audit gb_render

Depends on: CL20.

### CL22 [ ] render: audit gb_render_vulkan and gb_render_vulkan_sdl

Depends on: CL21.

### CL23 [ ] imgui: audit gb_imgui and gb_imgui_sdl

Depends on: CL21.
