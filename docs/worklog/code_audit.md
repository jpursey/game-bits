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

### CL16 [x] collide: audit gb_collide

Depends on: CL4.

- `gb_collide` had no tests; it now has them for every shape and intersection.
- `SphereHitsSphere()` compared the squared distance to the product of the
  radii rather than the square of their sum, so most overlapping spheres
  missed (two unit spheres 1.5 apart).
- The `RayHitsTriangleAt()` and `RayHitsNormalTriangleAt()` overloads that
  return a position never returned on a hit (undefined behavior), so the
  `Intersects()` overloads of a ray and triangle with a position did too.
  Nothing called them, so the compiler never reported it.
- Ray / sphere intersection found the closest point on the ray as if its
  direction were normalized. `RayHitsSphere()` was wrong for any other
  direction (its docs said it needn't be normalized), and `LineHitsSphere()`,
  whose direction is the segment, was wrong for any segment not of length 1.
  `SphereHitsTriangle()` uses it for the triangle's edges, so it missed spheres
  that only touch an edge. It now compares the distance from the line with a
  cross product, which needs no division.
- `IsPointInTriangle()` was true for any point on the line through the second
  and third vertices, past either end, so `SphereHitsTriangle()` hit spheres
  beside a corner whose center was on that line.
- Touching shapes intersected for some pairs and not others (a point on a
  sphere's surface was outside it, touching spheres intersected, touching
  boxes didn't, and a sphere touching a triangle's plane missed). Touching
  shapes now always intersect, as libccd's GJK mostly treats them (within its
  tolerance of about 0.0001), and as a ray through the edge shared by two
  triangles must, so meshes have no cracks. A ray or line starting on a
  triangle still misses it, so a ray cast from a surface doesn't hit it.
- `SphereHitsTriangle()` only rejected spheres in front of the triangle's
  plane, so a sphere far behind a triangle hit it. It also DCHECK failed in
  debug builds for most normals from `GetTriangleNormal()`, as its check that
  the normal is normalized allowed less error than a float has.
- `Intersects()` of a point with a triangle or sphere took the shape by
  non-const reference, and `CCapsule` took a non-const array, so neither
  worked with const shapes.
- Docs: `IsPointInTriangle()` tests a point off the plane by its projection,
  rays never need a normalized direction, `GetTriangleNormal()` needs a
  non-degenerate triangle, and a point cloud needs a point (now a DCHECK, as
  an empty one collided as a point at the origin). Removed docs of
  parameters that don't exist, and fixed typos. Smaller cleanups: include
  guards named as in the rest of Game Bits, `absl::check` and `absl::log` as
  dependencies rather than libraries, missing includes, an unused
  `<algorithm>` include in `shapes.h`, and one function to set up libccd
  rather than four copies.
- Left as is: shapes' default constructors leave their members uninitialized,
  as `glm::vec3` does; ray / triangle intersection treats a ray as parallel
  using an absolute epsilon, so a very short direction can miss; and `CLine`
  and `CTriangle` convert implicitly from a pointer to an array, which can't
  be made explicit without breaking callers.

**Verify**
- Standard checks.
- Unit tests: triangle normals, bounds, and boxes around shapes; each
  intersection hitting and missing, including from behind a triangle, rays
  and segments with unnormalized directions, spheres touching only a
  triangle's edge or corner, points past the ends of each triangle edge,
  points off a triangle's plane, normals computed for skewed triangles, and
  the penetration of a sphere into a box; and every exactly touching case of
  points, rays, lines, spheres, triangles, and boxes, and rays and lines
  starting on a triangle.

### CL17 [x] game: audit gb_game

Depends on: CL4.

- After a frame more than a millisecond late, the game loop set the next
  frame's deadline to the time it was already past, so every later frame that
  took over a millisecond was late too and the frame rate cap was lost for
  good (after loading a level, say). The next frame now gets a full frame from
  when the slow one ended.
- `Game::Run(argc, argv)` built its arguments from `argv + 1` to `argv + argc`,
  a reversed range when `argc` is zero (undefined behavior, and an assert in
  debug builds).
- `GameStateMachine::Update()` meant to update each state once, restarting
  from the top state after a state change, but never recorded which states it
  had updated, so a parent was updated again when its child changed state
  during its update. Its counter was also a function static shared by every
  state machine, which raced when two were updated on different threads; it
  is now a member.
- `GameState::ChangeState()` and `ExitState()` from a state that isn't active
  (including from its own `OnExit()`, as a state is inactive by then) changed
  the top state instead, since an inactive state has no parent: `ExitState()`
  exited every state. They now fail with a `kInvalidChangeSibling` trace, and
  read the state's parent under the same lock as checking it is active.
- `SetTraceHandler(nullptr)` and `AddTraceHandler(nullptr)` crashed on the next
  trace. A null handler now removes all handlers, or is ignored when added.
- `CMakeLists.txt` set `gb_base_LIBS` rather than `gb_game_LIBS`, so the
  `absl::check` and `absl::log` it named only came in indirectly. They are now
  dependencies, with `absl::synchronization` and `absl::algorithm_container`.
- Tests no longer wait on real time where they can be given a clock: the game
  tests that take a context run on a fake clock, and the state machine's
  thread test waits for its loops to run rather than sleeping for a second.
- Docs: a state's parent and child are cleared before `OnExit()` (the docs
  said they were set during it); `ChangeState()`, `ExitState()`, and
  `ChangeChildState()` need an active state; a trace's state is
  `kNoGameStateId` for a request to exit; and `kInvalidChangeSibling` traces
  have a parent. Fixed typos. Smaller cleanups: missing includes, an unused
  include, `absl::StrAppend` to build trace strings, `absl::c_linear_search`,
  the trace message moved rather than copied, and an unneeded lock in the
  state machine's constructor.
- Left as is: the tests of the `Run()` overloads that make their own context
  still run a frame on the real clock, as nothing can give them a fake one;
  the `kInvalidChangeSibling` trace for an inactive state names that state only
  in its message, as the trace has no field for the requesting state; and
  trace messages are still built when tracing is enabled but the handler is
  null (the trace level turns tracing off).

**Verify**
- Standard checks.
- Unit tests: the frame rate holding after a slow frame, `Run()` with an empty
  `argv`, a parent updated once when its child changes state during its
  update, `ChangeState()` and `ExitState()` failing from an inactive state, and
  null trace handlers.

### CL18 [x] image: audit gb_image

Depends on: CL12.

- Only `ImageView` had tests; `Image`, `Pixel`, and `LoadImage()` now have them
  too, loading a TGA file built in the test from a memory file system.
- `Pixel` from a `glm::vec3` or `glm::vec4` color outside [0, 1] was undefined
  behavior, and in practice wrapped (2.0 became 254, and -1.0 became 1). Each
  channel is now clamped, as `ModAlpha()` and `operator*` already did. NaN was
  undefined behavior for all of them, as clamping it leaves NaN; it now becomes
  0. They all convert through one function.
- A region's pixels were found by indexing the view, which asserts in debug
  builds when the region starts past the last pixel: any region of an empty
  image, or an empty region on the bottom edge. `GetAll()` and `Clear()` also
  copied from or wrote to null pointers when there was nothing to do, as
  `SetAll()` doesn't, and clearing an empty region no longer marks the view
  modified, as setting one doesn't.
- `Image::View()` wasn't const, so a const image had no view.
- `Pixel::Packed()` read a `Pixel` as a `uint32_t` through a cast pointer,
  which breaks strict aliasing. It and `Pixel(uint32_t)` now use
  `std::bit_cast`, and so are also `constexpr`.
- `CMakeLists.txt` listed `absl::check` and `absl::log` as libraries rather
  than dependencies. `absl::log` is now a dependency, and `absl::check`, which
  nothing used, is gone.
- Docs: who frees the pixels an `Image` is given, that `LoadImage()` reads from
  the start of the file, what a short buffer does to `GetAll()` and `SetAll()`,
  the byte order of a packed pixel, and the modifications an `ImageView`'s
  callback is told about (the docs named functions that don't exist). Smaller
  cleanups: missing includes, `std::fill_n` to clear a new image, the stb
  callbacks as a constant, and a redundant flag set in `ImageView::Modify()`.
- Removed the unused `<algorithm>` include from `pixel.h`, so code that got it
  from there may need its own include. `image_file.h` now includes
  `image_types.h` directly, like the library's other headers.
- Left as is: colors convert to pixels by truncating rather than rounding
  (every 8-bit value still round trips), and image and region sizes aren't
  checked.

**Verify**
- Standard checks.
- Unit tests: `Pixel` from in-range, out-of-range, and NaN colors, packed values,
  alpha and color scaling; `Image` construction, ownership of given pixels,
  editing, and the view of a const image; empty regions on each edge and of an
  empty image; the modified flag passed to an `ImageView`'s callback; and
  `LoadImage()` from a file system and a file (positioned anywhere), and of
  missing, empty, and invalid files, with the errors they log.

### CL19 [x] job: audit gb_job

Depends on: CL5, CL7.

- Destroying a `FiberJobSystem` dropped its pending jobs, so a job waiting on
  one never resumed, leaking its fiber and state (and failing a debug check).
  A job that waited during shutdown could also take a fiber that was exiting,
  and switch to it (a debug check, and a crash in release builds). Destroying
  the job system now waits for every job to finish, including pending jobs
  and any they run. Past a timeout (the new optional `shutdown_timeout`, 10
  seconds by default, timed by the context's `Clock`), the process ends with
  a fatal error that names the unfinished jobs (when job names are kept),
  rather than hanging. Jobs are counted with an atomic, and only tracked by
  name (under a mutex) when names are kept, which they already cost a string
  for.
- Idle job threads spun on `std::this_thread::yield()`, so an idle job system
  kept every job thread's core busy: 31.8 seconds of CPU per second with 32
  threads. They now wait on a `moodycamel::LightweightSemaphore` (from the
  already vendored concurrentqueue), which spins briefly and then sleeps,
  signaled for each queued job or resumed fiber: 0 ms of CPU per second.
- `SetVerboseLogging(false)` turned verbose logging on, as it ignored its
  argument. The flag is now atomic, as every job thread reads it.
- `GetData()` DCHECK failed for the last of the `kMaxJobDataHandles` handles.
- A thread count of zero or less wasn't clamped to `kMaxThreadCount`, as
  documented, on machines with more hardware threads than that. On Windows the
  hardware threads come from one 64 bit affinity mask, so this only affects
  other platforms, and isn't tested.
- `Wait()` on a counter that was already done switched to another fiber and
  back, creating a new fiber if none was unused. It now returns at once.
- `CMakeLists.txt` listed `absl::check` and `absl::log` as libraries rather
  than dependencies, named `absl::str_format`, which nothing used, and not
  `absl::flat_hash_set`, `absl::inlined_vector`, `absl::memory`, or
  `absl::time`. Removed the unused `<iostream>` include (the disabled log
  macro now uses `LOG_IF(INFO, false)` rather than `std::cout`), and the
  unused `flat_hash_map` include.
- Docs: a data handle's create callback must return an object from `new`, and
  may be called from several jobs at once; `SetVerboseLogging()` and what
  destruction does. Smaller cleanups: unused `FiberState` members, missing
  includes, `s_` for a function static, the job loop split into
  `WaitForWork()` and `RunJob()`, and in the test, `GTEST_SKIP()` rather than
  a check in every test, and `AbslStringify` rather than an iostream `PrintTo`.
- Left as is: the tests sleep for a few milliseconds in some jobs so that
  waits are likely to block, which nothing else can arrange; `JobDataHandle`
  is a `size_t`, which is public API; destruction polls every millisecond
  while jobs are unfinished (which can be about 15 ms on Windows), as waiting
  on a notification can't use the context's `Clock`; and
  `JobCounter::IsDone()` takes a reader lock, which is only on the `Wait()`
  path.

**Verify**
- Standard checks.
- Unit tests: destroying the job system with pending jobs and waiting jobs
  (both ran, and the waiting ones resumed), the shutdown timeout (a death
  test on a fake clock), verbose logging on and off, all
  `kMaxJobDataHandles` handles, and `Wait()` on a done counter creating no
  fiber. The job tests passed 25 runs each in Debug and Release.
- Idle CPU, measured by hand with a temporary test: process CPU time over
  one second with an idle 32 thread job system, before and after.

### CL20 [x] message: audit gb_message

Depends on: CL4.

- `MessageSystem` walked subscriber sets with its mutex released, deferring
  subscription changes and removals until a dispatch ended. That caused several
  bugs:
  - Removing an endpoint from one thread while another dispatched to it erased
    a stale iterator after waiting, which broke if the map had grown in the
    meantime.
  - A finished dispatch erased removed subscribers while another thread was
    still iterating the same set. That is a data race, and a crash if it was
    the element being visited.
  - An endpoint without its own dispatcher, reached through one that had one,
    ran its handlers on that other dispatcher instead of the system dispatcher.
  - A circular subscription between endpoints with different dispatchers
    re-queued the message forever.
  - A removed endpoint's ID stayed in the subscriber sets of channels that were
    never sent to.

  Sending now finds the receivers (the endpoint and everything subscribed to
  it, once each) under the lock, and queues one copy of the message on each
  dispatcher they use. Each dispatcher, or the send itself for receivers
  without one, finds the receivers again and delivers to its own, one at a
  time. Subscriptions are kept consistent in both directions, so removal cleans
  them up.

  This removes the deferred add and remove sets, `erase_after_dispatch`, the
  set of dispatching threads, and the unused dispatcher reference counts. A
  message that reaches no endpoints is no longer copied. Two side effects:
  - Deleting an endpoint no longer waits for the handlers of the endpoints
    subscribed to it.
  - An endpoint that subscribes after a message is sent gets it only if its
    dispatcher already had a copy queued.
- Endpoints are tracked by a `WeakScope`, which is locked while a message is
  delivered. Destroying an endpoint waits for deliveries on other threads, even
  if its system is being destroyed, where the old code could deliver to an
  endpoint that had been freed.
- `MessageEndpoint`:
  - A queued message with no handler stopped the queue. The messages after it
    were never delivered, they were delivered twice by a later receive, and
    they leaked.
  - A handler that set a new handler for its own type was put back over the
    new one.
  - A null callback crashed when called; it now clears the handler.

  Handlers are now shared, so one that is running isn't moved out of the map
  and back, and one that replaced itself is destroyed outside the lock.
  `MessageStackHandlers` had the same two handler bugs, fixed the same way.
- `MessageStackEndpoint`:
  - `Remove()` didn't clear the handlers' stack, so they could never be pushed
    again (terradiem's and block-world's states remove theirs on exit).
  - Pushing the same handlers onto two stacks at once could fail a CHECK.
    `Push()` and `Remove()` now change the handlers' stack with a single
    compare and set.
  - Destroying the stack cleared its message types while a handler could
    still be running on another thread, failing a DCHECK (or crashing in
    release builds). It also destroyed its mutex and stack before its
    endpoint, which a running handler still used. It now takes the handlers
    off the stack, then destroys the endpoint (waiting for running handlers).
- `CMakeLists.txt` set `gb_base_LIBS` instead of `gb_message`'s, so
  `absl::check` and `absl::log` were only linked through `gb_base`. They and
  the other Abseil libraries used are now dependencies. Include guards in
  `message_system.h` and `message_dispatcher.h` were named for `gb_base`.
  Missing includes were added, and each header includes `message_types.h`.
- Two thread dispatcher tests slept to let pending messages arrive; they now
  `Cancel()` the dispatcher, which joins its thread.
- Left as is:
  - `RemoveEndpointFromOtherThread` and the two thread dispatcher destruction
    tests still sleep, to make the race they exercise likely. They pass
    whatever the timing, and there is no hook to wait on instead.
  - The concurrency fixes (the stale iterator, the concurrent erase, destroying
    an endpoint or stack while delivering on another thread) have no
    deterministic tests.
  - Each dispatcher walks the subscription graph again when it delivers, so
    that an endpoint unsubscribed by then doesn't receive the message.

**Verify**
- Standard checks.
- Unit tests:
  - An endpoint without its own dispatcher, subscribed to one with a
    dispatcher, receives through the system dispatcher.
  - A circular subscription across dispatchers delivers once to each endpoint.
  - There is one copy per dispatcher, and none when there are no receivers.
  - A handler can set its own handler from inside itself; a null handler
    clears it; an unhandled queued message neither stops the queue nor leaks.
  - Deleting an endpoint doesn't wait for its subscribers' handlers.
  - Stack handlers can be pushed again after `Remove()`, can't be pushed onto
    two stacks, and leave a destroyed stack. A stack handler can set itself,
    and a null stack handler passes the message down the stack.

### CL21 [x] resource: audit gb_resource

Depends on: CL12.

- `ResourceSystem::Load()` deadlocked on a named resource that had never been
  referenced, such as one just saved by `ResourceFileWriter`: it took the
  returned reference with the system locked, and a first reference makes the
  resource visible, which locked the system again to set its copy of the
  flag. Visibility is now kept only on the resource (an atomic set under the
  resource's own lock), so adding a reference never locks the system.
- `Resource::GetResourceName()` returned a view into a `flat_hash_map`, which
  moves its strings as it grows, so the view of a short name dangled once
  other resources of the type were named. Names are now kept in an
  `absl::node_hash_map`.
- Two threads removing references at once could both decrement the count
  without either seeing the last one, so an auto-release resource was never
  released (it stayed loaded until its manager was destroyed). `RemoveRef()`
  now decrements with a compare and swap, and releases if it holds the last
  reference.
- Move assigning a valid `ResourceEntry` or `ResourceNameReservation` leaked
  the entry or name it replaced, which stayed in use for the life of the
  system.
- Reserving a name a resource already had reserved succeeded, and either
  reservation released the name. So a failed `Write()` dropped the name its
  caller had reserved, and a stale duplicate reservation could release
  another resource's name. Only the reservation that added a name now
  releases it, and releasing or applying checks the name is still the
  resource's.
- A loader that returned an existing resource with a different name left the
  old name mapped, so once the resource was deleted, loading the old name
  always failed.
- `GetResourceType()` kept returning the types of removed managers, and
  registering a type whose name another type had silently replaced it. A
  duplicate name now fails registration.
- Null dependencies, which `ResourceSet::Add()` already skipped, crashed
  `ResourceSet::Remove()` and `ResourceFileWriter::Write()`. They are now
  ignored everywhere, as the `GetResourceDependencies()` comment now says.
- `ResourceFileWriter::Write()` opened (and so emptied) the file before
  checking dependencies and running the writer, so a failed write destroyed
  the existing file. It now opens the file last, so the registered writer runs
  even when the file then can't be opened. A null resource crashed.
- `ResourceFileReader::Read()`:
  - Reading a file whose resource already existed but wasn't visible (such as
    reading the same file twice) passed the reader an invalid entry, which
    crashed when the resource was constructed. `ResourceSystem::Find()` now
    finds resources that aren't visible, as the reader's comments assume, and
    an ID in use without a resource fails the read.
  - A missing dependency without a name crashed logging its null name.
  - Chunks smaller than their struct (or the minimum flat buffer size) were
    read past their end; the size constants were never checked.
- `CMakeLists.txt` was missing several Abseil dependencies, the
  `resource_system.h` include guard was misspelled, and `ResourcePtr` used
  `nullptr_t` without `std::`. Missing includes were added; removing
  `absl/log/log.h` from `resource.h` meant adding it to six `gb_render` files.
  Loops in `~ResourceSystem()` and `RemoveManager()` copied every type's name
  maps.
- `ResourcePtr`s compared by converting both to `bool`, so any two non-null
  pointers were equal. They now compare the resources they point to. The
  implicit `operator bool` (also on `ResourceEntry` and
  `ResourceNameReservation`) still lets other operators compile this way;
  making it explicit may break callers, so it is the backlog item *Explicit
  resource bool conversions*.
- Chunk structs and flat buffers are still trusted past their size: offsets
  inside them aren't checked. That belongs in `gb_file`, as the backlog item
  *Validate chunk contents*.
- `ThreadAbuse` slept in its loops and waited a second of real time; it now
  waits for loop counts. Four `ResourceSet` tests were in the `ResourceTest`
  suite.
- Left as is:
  - A reference added and removed while a release handler runs, after the
    handler's delete failed because of it, leaves the resource unreferenced
    and not released.
  - Resources embedded in a file that is read without a `ResourceSet` are
    never referenced, so they aren't released until their manager is
    destroyed.
  - Loading a name that another thread is loading fails, as its `TODO` says.
  - A resource is in the system from its `Resource` base constructor on, so
    two threads reading files that embed the same resource ID can find each
    other's resource before it is fully constructed, or just before a failed
    read deletes it. Before, the second reader crashed instead.
  - The concurrency fixes other than the release race have no deterministic
    tests.

**Verify**
- Standard checks.
- Unit tests:
  - `Load()` of a named resource that was never referenced returns it and
    makes it visible.
  - Short names stay valid as many more resources are named.
  - Many threads releasing the last references at once delete the resource
    (failed on the second of 200 iterations before the fix).
  - Move assigning an entry or a reservation frees the old one; a stale
    duplicate reservation keeps another resource's name; a failed write keeps
    its caller's reservation.
  - Loading a resource under a second name, then deleting it, loads both
    names again.
  - Type names are removed with their manager, and a duplicate type name fails
    registration.
  - `ResourceSet::Remove()` and `Write()` with null dependencies, and
    `Write()` of a null resource.
  - A failed write leaves the existing file.
  - Reading a file twice, reading with the resource ID in use, a chunk too
    small for its struct, and a missing unnamed dependency.
  - `ResourcePtr` comparison.

### CL22 [x] render: audit the gb_render resource classes

Depends on: CL21.

Everything in `gb_render` except `RenderSystem`: textures, meshes, render
buffers, bindings and binding data, materials, scene types, and the draw list.

- `LocalBindingData` never recorded its buffer size, so a copy kept pointing
  at its source's buffer. Every `Material` copies its material type's default
  instance data this way, so setting one material's defaults changed its
  type's and every other material's, and a copy that outlived its source read
  freed memory. Values were also packed without alignment, so a texture after
  a `Vector3` was read through a misaligned pointer. Each value is now stored
  by its offset into a `std::vector<uint8_t>` rather than a pointer, so a copy
  is a plain copy, and is only accessed with `memcpy`.
- `MeshView`:
  - The triangle `Set`, `Add`, and `Remove` functions returned index counts,
    not the triangle counts their comments promise. They now count triangles
    throughout, so indices that aren't a multiple of three (with render
    asserts off) no longer leave a partial triangle.
  - `RemoveVertices()` and `RemoveTriangles()` shifted the remaining data with
    `memcpy` between overlapping ranges, which is undefined behavior (MSVC's
    `memcpy` happens to handle it). They now use `memmove`.
  - `AddTriangles()` of indices didn't assert a multiple of three, as
    `SetTriangles()` does.
- `Mesh::Set()` didn't apply the 65535 vertex limit that `CreateMesh` does
  (indices are 16 bits). It is now `kMaxMeshVertices`, and `Set()` fails past
  it.
- `Texture`:
  - `SetRegion()` checked the buffer size with `width * height` in `int`,
    which wraps for large regions (65536 x 65536 is 0), so a small buffer
    passed and was read far past its end.
  - Clipping added the region's position and size in `int`, so a region as
    large as an `int` allows (meaning "to the edge") overflowed and cleared or
    set nothing. The source offset for clipped rows is also 64 bit now.
- `TextureArray::Get()` into a vector resized the vector before checking the
  index, leaving it resized on failure.
- Deleting a texture or mesh while an `ImageView` or `MeshView` of it was
  active left the view writing to freed memory when it was destroyed. The
  texture and render buffer destructors now `CHECK` that no view is active.
- `DrawList::SetMaterialData()` and `SetInstanceData()` asserted
  `type != nullptr || type->...`, so with a material set they never checked
  the pipeline, and without one they dereferenced null instead of failing the
  assert. `SetScissor()` and `DrawPartial()` now assert their values fit the
  16 bit fields they're stored in, and `SetScissor()`'s comment no longer
  claims clipping.
- `Binding::IsValid()` rejected `kMaxBindingIndex`, which its comment
  includes.
- `RENDER_ASSERT` used an `if` without braces, and its enabled flag was a plain
  `bool` read from render threads; it is now atomic and named `g_`.
- `render_backend.h`'s include guard was `GB_RENDER_GB_RENDER_BACKEND_H_`.
  Every header now includes `render_types.h`, unused includes were removed
  (`mesh.h` no longer includes `material.h` or `binding_data.h`), and missing
  ones added. `Mesh` declared an undefined `DoEdit()`. `Shader`, a `final`
  class, had `protected` members.
- Comments: the texture classes described "vertices" and widths that "may be
  changed by calling Set", `TextureArray`'s class comment was `Texture`'s, and
  typos throughout.
- Tests: `TestBindingData` couldn't hold texture arrays (it `LOG(FATAL)`ed),
  a texture test passed a pixel count as a byte size, a render buffer test
  used `sizeof(kValueSize)` for `kValueSize`, and texture clipping was only
  tested symmetrically, so swapped x and y would pass.
- Left as is:
  - The `memmove`, misaligned read, and `DrawList` assert fixes have no failing
    tests: MSVC's `memcpy` handles overlap, x64 handles misaligned reads, and
    render asserts are off in tests (they `assert`). Making render asserts
    testable is the backlog item *Testable render asserts*.
  - `Texture` and `TextureArray` take sizes as `size_t`, and
    `TextureArray::Get()` takes its vector by reference; changing either
    changes their public API.
  - `Binding::IsCompatible()` compares constants types by pointer, while
    `operator==` compares their `TypeKey`.

**Verify**
- Standard checks.
- Unit tests:
  - A `LocalBindingData` copy has its own data and outlives its source, and a
    material's default instance data is its own.
  - Triangle `Set`, `Add`, and `Remove` return triangle counts, including when
    clipped to the capacity.
  - `Mesh::Set()` past `kMaxMeshVertices` fails.
  - `SetRegion()` too large for its buffer fails, `ClearRegion()` with the
    largest size clears to the edges, uneven clipping, and a full `Edit()`
    round trip.
  - `TextureArray` invalid indexes, leaving the vector unchanged.
  - Deleting a texture or mesh while it is being edited fails a `CHECK`
    (death tests).
  - `Binding` index range and `Combine()`, and texture arrays in binding data
    and their dependencies.

### CL23 [x] render: audit RenderSystem

Depends on: CL22.

`RenderSystem`'s create, load, and save paths, and one fix in
`ResourceFileReader`.

- Material type bindings:
  - `DoCreateMaterialType()` combined a binding shared by the scene type and
    a shader, but passed the uncombined list to the pipeline and material
    type, losing the shader's stages and volatility. Creating and loading a
    material type now share `GetMaterialTypeBindings()`.
  - Loading a material type or material now takes its bindings from the scene
    type and shaders, or the material type, as creating one does. The copies
    saved in their chunks are no longer written, and the schema fields are
    deprecated, so older files still load.
- `ReadBindingData()` looked up constants by index alone, ignoring the set,
  so a scene texture and material constants at the same index crashed. It
  logged a constants size mismatch and then copied the wrong size, and wrote
  an entry of any type into any slot. Each entry must now match the binding
  at its set and index, in type and size.
- Loading a shader accepted bindings that creating one rejects: an
  unregistered constants type (which left a null type), an index out of
  range, or a conflict. Shaders and scene types now share
  `GetUniqueBindings()`. It also accepted an unknown shader type. A loaded
  shader in edit mode had no code data, so it couldn't be saved again.
- `MaterialTypeChunk` had no depth test or raster mode, so they reloaded as
  the defaults. They are new fields at the end of the table, so older files
  load as before. The schema's `ShaderValue` enum listed 5 of the 41 values;
  `ShaderParam` round-tripped anyway, as it is copied as raw bytes.
- Scene type defaults were never applied: `CreateScene()` didn't copy them,
  though `RenderBackend` says it does, and material types didn't start from
  them. `CreateScene()` also passed a null scene type to the backend.
- `CreateMaterial()` made a material whose material binding data failed to
  be created, which `GetResourceDependencies()` then dereferenced. `Material`
  now takes the binding data from the render system, which fails first.
- A resource chunk reader that created its resource and then failed left it
  in the resource system under the file's ID, so a retry returned the
  half-built resource. `ResourceFileReader` now deletes it: only that read
  can have added a resource with the ID. The render system's chunk loads
  also validate before creating. `LoadTextureFile()` doesn't use the reader,
  so it deletes its own texture.
- Chunk validation: a texture array with a bad count or too many pixels was
  logged and loaded anyway. A mesh accepted a partial vertex, more than
  `kMaxMeshVertices`, a partial triangle, and out of range indices. A texture
  file larger than the maximum loaded. A vertex shader input at a negative
  location read before the vertex type's attributes.
- `kMaxTextureWidth` and `kMaxTextureHeight` were 8096 rather than 8192, and
  the chunk loads rejected the maximum itself. The five size checks are now
  `IsValidTextureSize()` and `IsValidTextureArraySize()`, with pixel totals
  in `int64_t`. `CreateMesh()` now uses `kMaxMeshVertices`.
- Tests: `shader_test.cc` reused moved-from shader code, so its invalid and
  incompatible binding tests passed for the wrong reason, and `FailCreate`
  never created the system. Malformed chunk tests write their files with a
  `ResourceFileWriter` registered with the test's own chunk writer.
- Left as is:
  - The texture array pixel total check has no failing test: a fake array
    over the limit allocates gigabytes.
  - `WriteBindingData()` reads constants with `GetInternal()`, which returns
    nothing for `kStaticWrite` bindings, as `SaveMaterial()` documents.

**Verify**
- Standard checks.
- Unit tests:
  - A material type combines a binding shared by its scene type and shaders,
    and starts from the scene type's defaults, as a scene does.
  - Creating a scene with a null scene type, or a material whose binding data
    fails, returns null.
  - Material types round trip their depth test and raster mode, and constants
    at an index the scene set also uses. Wrong size constants fail to load.
  - Shaders with an unknown type, constants type, or out of range index fail
    to load, and a loaded shader in edit mode can be saved again.
  - A file reads again after its chunk reader, plain or flat buffer, created
    its resource and failed. Textures and texture arrays load after a failed
    load of the same file.
  - Textures and texture arrays round trip at the maximum size, and texture
    files over it fail.
  - Malformed meshes and texture arrays fail to load.
  - A vertex shader input at a negative location fails.

### CL24 [x] render: audit gb_render_vulkan and gb_render_vulkan_sdl

Depends on: CL23.

None of this can be unit tested, so it was read for bugs only.
- Frame loop (`VulkanBackend`):
  - A failed or out of date present skipped the end of `EndFrame()`, after
    the frame was submitted. The next frame drew the previous frame's draws
    again, including resources deleted since. It also reused the in flight
    frame's semaphores and per frame resources. The frame now always ends
    once it is submitted.
  - A suboptimal swap chain image was treated as a failure, leaving its
    semaphore to be signaled. The next acquire waited on that same signaled
    semaphore, which is invalid. The frame now renders, and the swap chain is
    recreated next frame.
  - The garbage collectors rotated on every `BeginFrame()`, even ones that
    failed (such as each frame while minimized). Three failures in a row
    destroyed resources the in flight frame still used. They now rotate only
    when a frame begins.
  - The window's size changed callback set a plain `bool` from the message
    thread, and a resize during swap chain recreation was lost. It is now
    atomic, and cleared before recreating.
  - Image updates transitioned from `eUndefined`, which lets the driver
    discard the image's contents, so a partial update could lose the rest of
    the texture. `VulkanImage` now tracks each layer's layout, which partial
    updates move from; full updates still use `eUndefined`.
  - Sampler anisotropy wasn't clamped to the device limit, and validation
    errors were logged as info. Unused: `scene_types_` (which also dangled),
    `InitResources()`, and `physical_format_properties_`.
- Binding data:
  - `VulkanDescriptorPool` never reused a disposed descriptor set, because
    the frame check was inverted. Every new binding data allocated new sets,
    and the pools grew without bound.
  - `VulkanBindingDataFactory::NewBindingData()` read its binding group after
    releasing the lock, while another thread could add a group and move it.
    It leaked a descriptor set when the second one failed, and creating a
    binding group resized its buffer before checking it was created. A
    constants type larger than the device's uniform buffer limit divided by
    zero; creating the factory now fails.
  - `VulkanBindingData::Validate()` accepted an index one past the end.
    `GetBufferOffsets()` copied a vector on every draw call.
- Buffers:
  - Growing a `kStaticReadWrite` render buffer while its host buffer was in
    use lost the existing values, and growing a `kStaticWrite` one copied
    uninitialized memory over them. A `kStaticWrite` buffer now copies only
    the new values, and fails to grow if it was recently rendered, as
    `kStaticWrite` textures already do for partial updates (now documented on
    `Resize()`). An empty buffer queued a zero size copy, which is invalid.
  - `VulkanBuffer::MapData()` left a dangling pointer when invalidating
    failed, and `UnmapData()` unmapped `VMA_MEMORY_USAGE_CPU_ONLY` buffers,
    which it documents as doing nothing.
- Textures:
  - `CopyRegion()` copied a tightly packed region as one block even when it
    was narrower than the texture, which corrupted the texture.
  - Updating part of a `kStaticReadWrite` texture in use replaced its image,
    but only uploaded the updated region, so the rest was lost. It now
    uploads every pixel to a new image.
  - Adding a `kStaticWrite` dirty region removed existing regions above it
    that it didn't contain, so those pixels were never uploaded.
  - A partial update of a mipmapped `kStaticWrite` texture generated its
    mipmaps from uninitialized memory. It now fails, as a partial update of
    one in use already did (now documented on `ClearRegion()` and
    `SetRegion()`).
- Texture arrays:
  - Updating a texture in an array in use replaced the image, but only
    uploaded the textures modified since, so the rest were lost. A
    `kStaticReadWrite` array now uploads every texture, regenerating
    mipmaps only for those modified. A `kStaticWrite` array can't, as its
    other textures can't be read back, so it fails (now documented on
    `Clear()` and `Set()`).
  - `kStaticReadWrite` arrays wrote to a host buffer that an in flight frame
    could still be copying from. They now copy it first, as textures do.
  - `DoGet()` copied each texture's mipmaps too, overrunning the caller's
    buffer.
- Shader code that wasn't whole 32-bit words was passed to Vulkan, which
  requires them. Code that isn't 32-bit aligned (such as `gb_imgui`'s embedded
  `uint8_t` arrays) is copied to aligned memory first.
- `SdlVulkanWindow` destroyed its callback and mutex before its message
  endpoint, while a message could still be handled on another thread.
- Conventions: the garbage collector's include guard, `vulkan_allocator.cc`'s
  copyright and include path, and unused includes (including `glm`, no longer
  a dependency).
- Left as is:
  - Drawing with an unset texture binding crashes in
    `VulkanBindingData::OnRender()`. Skipping it would leave the descriptor
    unwritten or pointing at a deleted image, which is worse; a default
    texture would fix it.
  - A `kPerFrame` texture's host buffers start uninitialized, so mipmaps
    generated by a first partial update include garbage.
  - If the command buffer fails to begin after an image is acquired, its
    semaphore is left signaled.
  - While `BeginFrame()` keeps failing (such as while minimized), nothing is
    garbage collected, so resources deleted meanwhile are freed only when
    rendering resumes.
  - Growing a `kStaticWrite` buffer that was recently rendered could keep
    working with a device to device copy from the old buffer, but nothing
    grows one, so it fails instead. Partial updates of a mipmapped
    `kStaticWrite` texture need mipmaps generated on the GPU.
  - `NewBindingData()` builds the binding data while holding the factory's
    lock. Copying the data out first would shorten it, at the cost of three
    differently shaped return paths.

A new example, `examples/render-check`, exercises these by hand. Its window
events reach the backend on a `ThreadMessageDispatcher` thread, and it shows
memory use and every logged warning and error, including the validation
layer's.

**Verify**
- Standard checks.
- By hand, in `examples/render-check` (which always enables validation), with
  no errors logged:
  - Resize, minimize, and restore the window.
  - Recreate every instance each frame for a minute: memory stays flat, and
    the grid stays whole.
  - Paint the `kStaticReadWrite` and `kPerFrame` textures: the checkerboard
    stays, and the squares are clean.
  - The two region `kStaticWrite` texture shows both regions, and updating
    the drawn one (or a mipmapped one) partially fails as expected.
  - Changing a texture array's layer 0 each frame keeps layers 1 to 3, at
    both sizes. Getting layer 1 returns 4096 pixels. Setting a layer of the
    drawn `kStaticWrite` array fails as expected.

### CL25 [x] imgui: audit gb_imgui and gb_imgui_sdl

Depends on: CL23.

`gb_imgui` had no tests. `ImGuiInstanceTest` draws with `gb_render`'s test
backend, which now records the commands of each `DrawList` it draws.
`gb_imgui_sdl` only builds ImGui's SDL backend, so there was nothing of ours to
audit.
- Instances:
  - A second instance on the same render system always failed to create, as
    it registered the render types again. Instances now share them.
  - `ImGui::CreateContext()` only makes the new context current if there is
    none, so a second instance stored itself in the first one's context, and
    the first was no longer active. `Create()` now makes the new instance
    active as documented, and restores the previous one if it fails.
  - `GetActive()` and `IsActive()` asserted when no context was current (such
    as after the active instance was destroyed). `GetActive()` now returns
    null.
  - `Init()` crashed if the material type, default textures, or material
    failed to create.
  - `LoadFonts()` never set its "done" flag, so each call made another font
    texture.
  - Each `AddTexture()` call made a new material, so calling it every frame
    (the natural way with ImGui) grew without bound. The same texture now
    returns the same ID. A null texture, a texture array index out of range,
    or a texture from another resource system (which the instance couldn't
    keep loaded) returns null.
- Drawing:
  - The mesh grew to twice the vertices needed, so any GUI over half of
    `kMaxMeshVertices` failed to make its mesh, and nothing was drawn again.
    Growth is now capped at the limit, and grows the mesh in place with
    `Mesh::Set()`, which keeps its buffers if larger ones can't be made. A GUI
    over the limit skips the frame, logging an error at most once a second.
  - A failed mesh `Edit()` also stopped all drawing; it now skips the frame.
  - `ImDrawCallback_ResetRenderState` reset the draw list without setting the
    material and mesh again, so the next draw had neither.
  - Commands with an empty clip rectangle were drawn with an empty scissor.
    They are now skipped, and a GUI with no display size isn't drawn.
- ImGui file functions: `ImFileGetSize()` returned 0 on error rather than -1,
  so a failed size read as an empty file. `ImFileRead()` and `ImFileWrite()`
  returned bytes rather than items, as `fread()` does (ImGui only passes items
  of one byte).
- Conventions: `gb_imconfig.h`'s include guard, a missing `explicit`, and
  missing includes.
- Left as is:
  - A GUI over `kMaxMeshVertices` could be drawn by splitting it across
    meshes. Indices and `DrawList::DrawPartial()`'s vertex offset are 16 bits,
    so one mesh can't hold more.
  - The test compiles `gb_render`'s test backend from its sources, as nothing
    outside `gb_render` used it before. A shared test library target would
    avoid listing the files twice.
  - `RenderSystem`'s `Register*` functions fail on a name already registered,
    so `Init()` gets each type before registering it. Returning the existing
    type when the definition matches would let callers just register, and
    would catch a different type registered under the same name, which `Init()`
    doesn't check.
  - `GetActive()` assumes any current ImGui context belongs to an
    `ImGuiInstance`, so a context made directly with `ImGui::CreateContext()`
    must not be current while ImGui uses its file functions.

`examples/render-check` has a new GUI section: extra rectangles to grow the
GUI's mesh, a button that goes over the vertex limit for a second, and a
render state reset in the middle of the panel.

**Verify**
- Standard checks.
- Unit tests (`ImGuiInstanceTest`): two instances on one render system, the
  active instance after creating, destroying, and failing to create, creation
  failing for each render resource, fonts loaded once, `AddTexture()` IDs, mesh
  growth past half the limit, frames over the limit and failed edits skipped,
  render state resets, clipping, and saving and loading settings through the
  file system.
- By hand, in `examples/render-check`, with no errors logged other than those
  expected:
  - Drag "Extra rectangles" to the maximum and back: the block below the grid
    grows and shrinks, and the panel keeps drawing.
  - "Go over the vertex limit for a second": the GUI disappears for about a
    second with a "GUI has ... vertices" error, then comes back.
  - "Reset render state in this panel": the line after the reset, and the
    sections below it, still draw.

### CL26 [x] all: final pass

Depends on: CL1 to CL25.

A pass over every library for conventions that hold across all of them, which
the earlier CLs didn't check consistently. Unused includes were found with
clang-tidy's `misc-include-cleaner` (Visual Studio's clang-tidy 19, run over
every file in `src/gb` with the Debug build's compile commands), and each one
was checked by hand.
- Types headers: every header in a library with a `*_types.h` now includes it,
  so a forward declaration that doesn't match its definition is caught right
  away. It was missing from `chunk_types.h`, `common_protocol_test.h`,
  `local_file_protocol.h`, `memory_file_protocol.h`, `path.h`, `raw_file.h`,
  and `test_protocol.h` in `gb_file`, from `lexer_config.h`,
  `lexer_program.h`, and `symbol.h` in `gb_parse`, from `vulkan_scene_type.h`,
  and from `test_resources.h`. `gb_imconfig.h` is left out, as it is ImGui's
  own configuration, included by ImGui itself.
- Unused includes: removed in every library. Some were replaced with the
  header actually used: `fiber_job_system.h` only needs `thread_types.h` (its
  `.cc` and test now include `fiber.h` and `thread.h`), `vulkan_shader_code.h`
  needs `shader_code.h` rather than `shader.h`, `render_system_test.cc` needs
  the generated flatbuffers header rather than `render_resource_chunks.h`, and
  `parser_rules.cc` and `render_test.cc` use `absl/log/check.h`, not `log.h`.
  `chunk_reader.h` had relied on `chunk_types.h`'s unused `absl/hash/hash.h`
  for `<optional>` and `<vector>`, and now includes them.
- Missing includes, found along the way rather than by a sweep (the include
  cleaner ran with missing includes off): `parse_types.h` used `uint32_t` and
  `std::string`, `raw_file.h` used `int64_t`, `chunk_types.h` used `std::move`
  and `std::strlen`, and `lexer.h` used `std::shared_ptr` without including
  them, so each only compiled after something else.
- Include guards: six didn't follow `GB_<DIR>_<FILE>_H_`: `file.h`,
  `file_types.h`, and `raw_file.h` (still `GB_BASE_FILE_...`), `weak_ptr.h` (no
  trailing `_`), `sdl_vulkan_window.h`, and `thread_types.h`.
- Kept, although the include cleaner reports them:
  - Umbrella headers, whose symbols are declared in headers they include:
    `<windows.h>`, `<malloc.h>`, `<share.h>`, `<intrin.h>`, `glm/glm.hpp`,
    `utf8.h`, `SDL.h`, and `flatbuffers/flatbuffers.h`.
  - `<new>` for placement new.
  - Headers needed for a complete type to destroy a `std::unique_ptr`:
    `raw_file.h` in three `gb_file` tests, and `file.h` in the resource file
    reader and writer.
  - A test's own header, and each `*_types.h`.
  - `vulkan_types.h`'s `<vulkan/vulkan.hpp>` and `render_assert.h`, which set
    up Vulkan-Hpp's asserts for every Vulkan header.
- Libraries without a `*_types.h`: `gb_alloc`, `gb_base`, `gb_collide`,
  `gb_config`, `gb_container`, `gb_game`, `gb_profile`, `gb_sdl`, `gb_test`,
  `gb_imgui_sdl`, and `gb_render_vulkan_sdl`. Not every library needs one; the
  convention only applies to libraries that have one. Only `gb_game` forward
  declares a type defined in another header (`game_state.h` declares
  `GameStateInfo` and `GameStateMachine`, breaking the include cycle with
  `game_state_machine.h`), and it is left without one for now.
- Left as is:
  - clang-cl. Game Bits is meant to build with clang-cl as well, but clang (as
    run by clang-tidy) rejects some of it: `TokenIndex`'s defaulted
    `operator<=>` on bit fields in `parse_types.h`, `using` declarations of
    dependent types without `typename` and a protected `data` access in
    `array.h`, and an unused lambda capture in `fiber_job_system_test.cc` (an
    error with warnings as errors). A clang-cl build couldn't be checked here,
    as Visual Studio's clang component isn't installed.
  - The include check isn't repeatable: the clang-tidy run is not in the tree,
    and the includes kept on purpose aren't marked `// IWYU pragma: keep`, so
    each run reports them again. Whether to add a `.clang-tidy` and a script or
    CMake option, and the pragmas, is a backlog item to decide on.

**Verify**
- Builds cleanly in Debug and Release, `ctest` passes in Debug, and touched
  files pass the clang-format check.
- The include cleaner over `src/gb` reports only the includes kept above.
- `examples/render-check`, `examples/block-world`, `templates/cli`, and
  `templates/sdl` build.
