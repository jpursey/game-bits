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

gb_file was audited as four CLs, in dependency order: paths, then `File` and
chunk files, then `FileSystem` and the generic protocols, then
`LocalFileProtocol`.

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

### CL11 [x] file: audit FileSystem and the generic protocols

Depends on: CL10.

- Nothing enforced that paths passed to a `FileProtocol` are absolute, as its
  header requires, and `IsRootPath()` being true for an empty path (see CL9)
  was all that kept a direct `DeleteFolder("")` from deleting a protocol's
  root. Every public `FileProtocol` function now fails for a relative path,
  and `IsRootPath()` is false for an empty path and a protocol on its own.
- A recursive `CreateFolder()` under a host that doesn't exist
  (`mem://host/a`) looped forever, as removing the last segment of `//host`
  leaves it unchanged.
- Copying a folder between protocols created the destination before checking
  the source, so a missing source, or a file, "copied" as an empty folder.
- One protocol registered under two names was copied between those names as if
  they were different protocols, with no checks for overlap:
  `CopyFile("a:/x", "b:/x")` emptied the file, and copying a folder into its
  own subfolder recursed until the stack overflowed. Two names for one
  protocol now copy as one protocol.
- Opening a `MemoryFileProtocol` file with `kReset` emptied it, but its size
  was still reported as before.
- `FileSystem::Register()` read the protocol's default names in the same call
  that moved the protocol into an argument, which relies on the unspecified
  order arguments are evaluated in. The `Register()` comments also said a
  name already registered is replaced, when registering fails instead.
- Deleting or replacing a `TestProtocol` file's path while the file is open
  left the file writing to freed state when closed. No test does it, so it is
  now a CHECK failure, rather than freed memory written silently.
- `FileSystem::GetCurrentFolder()` and `SetCurrentFolder()` called the
  protocol whether or not it supports `kCurrentPath`, unlike every other
  `FileSystem` function, so a protocol without it logged an error from the
  default implementation, and one that implemented current paths anyway was
  used. They now return an empty folder, or false, without calling it.
- `file_system_test.cc` included `gb/test/test_util.cc` rather than its header,
  compiling a second copy of `GenerateTestString()` into the test.

**Verify**
- Standard checks.
- Unit tests: relative paths passed to a protocol, `IsRootPath()` of an empty
  path and a protocol, a recursive create under a missing host, the size of a
  reset file, copying a missing folder or a file as a folder between
  protocols, copying under two names for one protocol, and current folders
  with and without `kCurrentPath`.

### CL12 [x] file: audit LocalFileProtocol

Depends on: CL11.

- A path passed straight to a protocol could leave its root:
  `DeleteFile("file", "/../outside")` deleted a file beside a
  `LocalFileProtocol`'s root. `FileSystem` normalizes paths, so only direct
  calls could, but nothing required them to be normalized. Every public
  `FileProtocol` function now fails for a path that isn't normalized, as it
  does for a relative one (see CL11).
- `LocalFileProtocol` joined a path with a host (`file://host/a`) to its root
  as a network path. A path with a host is now invalid.
- Paths went through the Windows code page, not UTF-8: a non-ASCII name was
  created mangled on disk, and listing a name outside the code page threw,
  which ends the process outside a test. Paths now convert through
  `gb/base/unicode.h`, and a path that isn't valid UTF-8 is invalid.
- A root that wasn't normalized was kept as given, so listing skipped every
  entry (`c:/a/./b`) and current folders were wrong. The root is normalized.
- A recursive `DeleteFolder()` that failed (a file open elsewhere) returned
  true, as `remove_all()`'s error result of -1 converts to true. A recursive
  `CreateFolder()` of an existing folder returned false.
- `GetCurrentFolder()` returned "" at the root rather than "/", and a folder
  beside the root whose name starts with the root's name counted as under it.
- Copying a file onto itself failed, where the other protocols succeed. It now
  succeeds for any path to the same file (Windows paths ignore case).
- A write straight after a read on a `LocalFile` was lost (and a read after a
  write is undefined), as an `fstream` needs a seek between them, like C's
  `FILE`. `LocalFile` now seeks when switching.
- The header said links are skipped or invalid, when they are followed. It now
  describes what happens (checked by hand with a junction, as creating a
  symbolic link needs a privilege).
- Smaller fixes: an unreachable error check when creating the root (now checked
  first), the destructor logging a path through the code page, which could
  throw, and an unused parameter of `ListFolder()`. Listing uses each entry's
  cached status, rather than querying it again.
- Most write errors go unreported, as `fstream` buffers writes and an error
  when a file closes is lost; this needs new `File` API, so it is the backlog
  item *Report buffered write errors*.
- Left as is: a failed write leaves the file invalid from then on, as its
  contents are unknown; listing stops at an entry whose status can't be read
  (such as a dangling link), returning what it has so far; and checking that a
  protocol path is normalized allocates a normalized copy to compare, where a
  check that doesn't allocate would repeat the normalization rules.

**Verify**
- Standard checks.
- Unit tests: paths that aren't normalized, for every protocol; recursive
  creates of existing folders, copying a file onto itself, and reads and
  writes without a seek between them, for every protocol; and for
  `LocalFileProtocol`, paths that leave the root or have a host, a recursive
  delete that fails, current folders at the root and beside it, non-ASCII and
  invalid UTF-8 names, roots that need normalizing (including "."), and
  copying a file onto itself under another case.

### CL13 [x] sdl: audit gb_sdl

Depends on: CL4.

- No bugs found. `SdlToGLog()` had no tests; it now does, for every priority
  and category, including VLOG(1) for verbose and DFATAL for critical.
- `sdl_glog.h` had no include guard, and said it sends logs to glog, which Game
  Bits replaced with Abseil logging. It now says how to install it and how SDL
  priorities map to log severities.
- Left as is: the `GLog` in the name, as renaming it breaks every caller for
  little gain, and critical messages ending a debug build, which is the
  intended mapping.

**Verify**
- Standard checks.
- Unit tests: each SDL priority's log severity, verbose logging only at
  VLOG(1), critical as DFATAL, each category's prefix, and unknown priorities
  ignored.

### CL14 [x] parse: audit gb_parse

Depends on: CL4.

- `ParsedItem::GetItems()` of a scoped name ("a.b") whose first part didn't
  match dereferenced null, as its `return` was missing.
- `AbslStringify()` of `ParserRules` named a member without its object, so it
  failed to compile wherever it was used.
- A `Lexer` kept the caller's view of a filename, not its own copy, so
  `GetFileContentId()` failed (or read freed memory) once the caller's string
  changed. Content that failed to add (a line too long) stayed registered under
  its filename, with an id that parsed garbage token indexes.
- Content after the first had several bugs from mixing a line within the
  content with a line within the lexer: `RewindLine()` failed at any line,
  `RewindToken()` at the start of the content moved into the content before it,
  and `GetLineText()` of a line out of range returned another content's line.
- `ParseToken()` of an index from another lexer (one past the tokens parsed on
  a line, or on the empty line between contents) read past the line's tokens,
  and `GetTokenLocation()` and `SetNextToken()` of an end index between contents
  dereferenced null. Token indexes are now checked in one place.
- An invalid token that started with a character that starts a symbol or
  comment, but didn't match one (`<x` with only the symbol `<<`), was an empty
  error token that didn't advance, so the lexer never reached the end.
- Floats of zero ("0.0") were invalid, as zero isn't a normal float.
- Binary, octal, and hex integers wider than the bit depth wrapped instead of
  failing: `0x1FF` at 8 bits was -1.
- Decoding escapes used the escape character even without `kEscapeCharacter`,
  so `"a\"` read past the string. An incomplete hex escape (`"\x4"`, `'\x'`),
  which the lexer accepts as escaping `x`, read past the token when decoded.
- Comment starts that were a prefix of another (`#` and `#[`, `/*` and `/**`)
  were accepted, though one hides the other, and the error for them says they
  are rejected. They now are.
- `Token::IsUint(value)` was true for a negative integer equal to `value` as
  unsigned (-1 for `UINT64_MAX`), unlike `IsUint()`.
- Rules with two rules of the same name (from a parser program, or
  `ParserRules::AddRule()`) kept the first and ignored the second. They are now
  invalid.
- Every `ParserProgram::Create()` validated the shared rules for the program
  syntax, and the first validation sets their token values, so two first calls
  on different threads raced. They are validated once when created, with a
  lexer program that is also created once, rather than on every call.
- Optimizations: matching a token compared it against a copy of its value
  (allocating for every string), and `ParserRules::GetRule()` and
  `ParsedItem::GetItems()` allocated a string to look up each name. None
  allocate now; the new `Token::HasValue()` compares a value in place. Hex
  integers parse with `absl::SimpleHexAtoi()`.
- `LexerFlag::kNewlineEscape`, `kTabEscape`, and `kHexEscape` had no effect,
  as the `LexerConfig` characters enable those escapes, so they are removed
  (breaking any config that names them; no project does).
- Doc fixes: `SetNextToken()` accepts end tokens; `Token::ToString()` returns
  an error's message; `ParseTokenText()` isn't safe to call at the same time as
  other const functions; and `Symbol` characters go up to 126. Smaller
  cleanups: unused locals, a duplicated error check in `Parser::MatchGroup()`,
  and missing includes in `symbol.h` and `parse_result.h`.
- Backlog item: *Parser rules shared between threads* (validating rules writes
  to them).
- Left as is: decoded strings, escaped characters, and forced-case identifiers
  are stored again each time a token is parsed again after the parser backs
  up, so memory grows with parsing work (bounded per token only by caching
  per token, which a block comment re-tokenizing a line complicates); and a
  symbol longer than 7 characters is silently truncated by `Symbol`.

**Verify**
- Standard checks.
- Unit tests: scoped item names that don't match, stringifying rules, a copied
  filename, a failed add, lines and rewinds in a later content, token indexes
  from another lexer, an invalid token at an unmatched symbol or comment start,
  zero floats, non-decimal integers beyond the bit depth, `IsUint()` of a
  negative integer, `Token::HasValue()`, decoding without an escape character
  and with incomplete hex escapes, comment starts that prefix another, and
  duplicate rule names, in a program and in `ParserRules`.

### CL15 [x] config: audit gb_config

Depends on: CL14.

- `ReadConfigFromText()` cached its parser programs in a static map with no
  lock, so reads on different threads raced when either created a program.
- Floats were written to text with 6 significant digits, so saving and loading
  a config changed them (1/3 read back as 0.333333), and a whole number was
  written without a decimal point, so it read back as an int. They are now
  written as the shortest text that reads back as the same value, with ".0"
  added to a whole number. `Config::AsString()` of a float had the same loss
  of precision, and now uses the same shortest text.
- UTF-8 characters in strings were written as hex escapes (`char` is signed),
  though the format says they are written as is, and hex escapes aren't JSON.
- Non-compact arrays wrapped lines in the wrong places: after a value was moved
  to a new line, the line length was measured from near its end, so the next
  long value didn't wrap; and the length of a value that spans several lines
  (a map) was measured to its end rather than its first line.
- `Config::ResizeArray()` with a negative size threw `std::length_error`,
  which Game Bits never catches; it is now treated as zero.
- `Config::AsInt()` of a string out of range returned the largest or smallest
  int rather than 0, and `AsInt()` and `AsFloat()` of a string stopped at an
  embedded NUL. `AsFloat()` of a string depended on the C locale, so "1.5" was
  0 after `setlocale()` to a locale with a decimal comma. Both now use
  Abseil's parsing, which also ignores trailing whitespace as well as leading.
- `Config::GetArray()` is const, but returned a mutable span by casting away
  const, so a const Config could be changed through it. It now returns
  `absl::Span<const Config>` (breaking any caller that changes the array
  through it; no project does).
- Optimizations: writing a map copied it (and all of its values, at every
  level) into a sorted map; it now sorts pointers to the entries, as does
  `Config::AsArray()` of a map, which also looked each key up again.
  `AsBool()` of a string no longer lowercases a copy.
- Doc fixes: the text format isn't rootless by default, and several typos.
  Smaller cleanups: missing includes, a missing trailing newline, garbled
  characters in the test section headings, and a `size_t` loop.
- Left as is: infinities, NaNs, and subnormal floats are written as text that
  can't be read back (as in JSON, which has no text for them); a non-compact
  empty map is written as `{` and `}` on separate lines with a blank line
  between; JSON output escapes control characters as `\xNN`, which JSON
  doesn't support (noted in the format's docs); and deeply nested text
  recurses once per level when read, so very deep nesting can overflow the
  stack.

**Verify**
- Standard checks.
- Unit tests: negative array sizes, strings out of range or with whitespace or
  NUL as ints and floats, floats from strings under a decimal comma locale,
  floats as strings and text (exact, shortest, whole numbers, exponents, and
  round trips including -0.0), UTF-8 strings written as is and read back, array
  wrapping of long values and after a multi-line value, keyword map keys, and
  reading with every flag combination on several threads at once.

### CL16 [ ] collide: audit gb_collide

Depends on: CL4.

### CL17 [ ] game: audit gb_game

Depends on: CL4.

### CL18 [ ] image: audit gb_image

Depends on: CL12.

### CL19 [ ] job: audit gb_job

Depends on: CL5, CL7.

### CL20 [ ] message: audit gb_message

Depends on: CL4.

### CL21 [ ] resource: audit gb_resource

Depends on: CL12.

### CL22 [ ] render: audit gb_render

Depends on: CL21.

### CL23 [ ] render: audit gb_render_vulkan and gb_render_vulkan_sdl

Depends on: CL22.

### CL24 [ ] imgui: audit gb_imgui and gb_imgui_sdl

Depends on: CL22.
