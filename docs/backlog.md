# Backlog

Work that isn't being done yet, for Game Bits itself or asked for by the
projects built on it. The worklogs in `docs/worklog/` describe what was built;
this is what has not been. See [workflow.md](workflow.md) for how items are
written, picked up, and requested by other projects.

The list is a rough stack rank: the order the items look worth doing, as a best
guess rather than a commitment. Re-order it freely.

Each item carries:

- **Layers**: the Game Bits libraries it touches, lower tiers first. More than
  one usually means more than one CL.
- **Size**: a guess. *Small* is a single CL. *Medium* is a few. *Large* is
  many, usually after a design.
- **Feature workflow**: whether the item follows the feature workflow, with a
  design and a `docs/worklog/` plan of CLs. Anything that comes down to one or
  two simple CLs doesn't, and is done as an ordinary change.
- **Depends on**: other items that should come first, or "nothing".
- **Requested by**: the project and item that need it, for items another
  project asked for. For ranking only.
- **Background**: where the context is, if anywhere.

## Fiber-safe thread locals

- **Layers:** thread, job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Profiler module](worklog/profiler_module.md) (To confirm)

A fiber sees the `thread_local` variables of the thread it runs on, and
without MSVC's `/GT` option the compiler can cache a thread local's address
across a fiber switch, so a fiber that moved threads reads the old thread's
storage (freed, if that thread exited). `FiberJobSystem` moves fibers between
threads, and `job_system.cc` keeps the current `JobSystem` in a thread local,
as `gb/thread` keeps the current thread. Check whether a job that waits and
resumes on another thread can read the wrong value, with a test that does
exactly that, and fix it if so: `/GT` on the affected code, or state that
moves with the fiber.

## Report buffered write errors

- **Layers:** file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md) CL12

Most write errors are never reported. `LocalFileProtocol` writes through a
buffered `std::fstream`, so `File::Write()` succeeds once the data is buffered,
and the disk write happens later: an error from a full buffer comes back as a
short count from a later `Write()` call, and an error when the file closes is
lost, as the destructor can't report it. So `FileSystem::WriteFile()` of a
small file on a full disk returns true and leaves a truncated file. When an
error is reported, the count is 0 even if some bytes were written, as the
failed stream reports no position.

Add a `RawFile::Flush()` that every protocol implements, `File::Flush()` and a
`File::Close()` that return whether everything written reached the file, and
have `FileSystem::WriteFile()` check it. Fakes cover `File`
(`TestProtocol::FileState::fail_write_after`); a real write failure can't be
made in a unit test, so `LocalFile` is checked by hand.

## Local file protocol rooted at a path

- **Layers:** file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** JPRSurf *Write files through gb::FileSystem*
- **Background:** `LocalFileProtocol::Create()` and `CreateTemp()` in
  `local_file_protocol.h`

Code that already has a folder as a `std::filesystem::path` roots a
`LocalFileProtocol` there through a `Context`, converting the path to UTF-8
itself:

    LocalFileProtocol::Create(
        ContextBuilder()
            .SetValue<std::string>(LocalFileProtocol::kKeyRoot,
                                   ToUtf8(folder.generic_u16string()))
            .Build());

Every caller repeats this, and it is easy to get subtly wrong: `ToUtf8()`
returns an empty string for a path that isn't valid UTF-16, and an empty root
is the current directory, so the files go there instead of the protocol failing
to be created. Add a convenience `Create()` that takes the root as a
`std::filesystem::path`, as `CreateTemp()` is a convenience for a temp folder.
It converts the path the way the protocol already converts the paths it lists
(the file-local `ToString()` in `local_file_protocol.cc`), and returns null for
a path that doesn't convert, with a test of that case.

## Re-read user tokens keep their capture

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** OZ-3 *Instruction forms*
- **Background:** `Lexer::ParseToken` and `Lexer::ParseNextToken` in
  `lexer.cc`

A user token's value depends on how it was read. The first time the lexer
reads it (`ParseNextToken`), its value is its regex's capture group: `$v` for a
token whose regex is `\$([a-z]+)` is `v`. Read again (`ParseToken`), after
`RewindContent`, `RewindToken`, or a parser backtracking over it, its value is
the whole token text, `$v`, unless it is the one token the lexer still caches.
Integers already re-run their regex when read again, so they don't have this
problem.

So the same token can have two values, and which one a parser sees depends on
how the parse went. A rule that tries one alternative and backtracks to a user
token gets the wrong value only some of the time.

Re-run the token regex for user tokens when reading one again, as integers do,
or keep the capture's offset and size in the token's info. Test it with a lexer
test that rewinds over a user token whose capture is shorter than its text, and
a parser test that backtracks over one.

## Number suffixes in either case, and hex numbers that start with a digit

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Requested by:** OZ-3 *Program assembler*
- **Background:** `CreateTokenPattern` in `lexer_program.cc`, and
  `Lexer::ParseInt` in `lexer.cc`

Assemblers of the Z80 and 8086 era write numbers with suffixes (`0FFh`,
`1011b`), in either case (`0FFH`), and a hex number starts with a digit so it
can't be read as a name (`FFh` is an identifier). The lexer can't do either:
- A number's prefix and suffix (`LexerConfig::hex_suffix` and the others) are
  matched exactly, as `RE2::QuoteMeta` text, so a config with `hex_suffix = "h"`
  doesn't read `0FFH`.
- A hex number is any run of hex digits with its prefix and suffix, so with a
  suffix and no prefix, `FFh` is a hex number. It also wins a tie with the
  identifier `FFh`, as integers come before identifiers in the lexer's tie
  order, so a name like `BEh` can't be an identifier either.

Add two `LexerFlag`s: one that matches number prefixes and suffixes ignoring
case (for every integer format, and floats if they have them), and one that
requires a hex number to start with a decimal digit (`[0-9][0-9A-Fa-f]*`,
with the allowed letters still set by `kHexUpperIntegers` and
`kHexLowerIntegers`). `ParseInt` already strips the prefix and suffix by
length, so only the token pattern changes. Test both with suffixes and
prefixes, including that `FFh` and `BEh` are identifiers, `0FFh` and `0FFH`
are numbers, and `1011b` and `1011B` are binary rather than decimal followed by
an identifier.

## Multithreaded profiles

- **Layers:** profile
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Profiler module*
- **Background:** [Profiler module](worklog/profiler_module.md)

Combine the profiles of several threads, each with its own `Profiler`, into
one view: CPU time and calls per point across all threads, per frame of the
main loop, and how busy each thread is per frame. Point indices are global, so
every Profiler's slots line up and combining them is a sum. Each thread adds a
snapshot into a shared total at a safe point of its own (the end of its frame,
or between jobs), or the slots become atomics written only by their own thread
(plain moves on x64) and are read live. Overlap and critical paths need a
timeline of events instead, which this is not. `Profiler::GetReport()` reads
its own slots, so the combined view needs the report to format a snapshot of
slots instead, which a combined total can also provide.

The kind of core a thread runs on matters as much as what it runs. On an
i9-14900KF (8 performance and 16 efficiency cores), frames on efficiency cores
took about 25-30% longer than the same frames on performance cores, and
Windows moved a program's main thread onto them when its window was in the
background (2026-10-05). The tick rate was not at fault: it measured the same
in every session, matching what CPUID reports. The single-thread report
already splits frames by core class and warns when they mix; a combined view
needs the same per thread, since job threads spread across both kinds of core.
Pinning threads to one kind would make profiles compare, but `gb/thread`'s
affinities don't know which hardware threads are which kind. Only
`gb/profile`'s `win_cpu_info.cc` builds that table, which would move to
`gb/thread` for it.

## Fiber-aware profiling

- **Layers:** thread, profile
- **Size:** medium
- **Feature workflow:** yes
- **Depends on:** *Profiler module*, *Fiber-safe thread locals*
- **Background:** [Profiler module](worklog/profiler_module.md)

Let a timed point span a fiber switch, which is a CHECK failure today. When a
fiber switches out, its open timers pause and leave the thread's timer stack,
and when it switches back in, on any thread, they resume there, so a job that
waits is charged only for the time it runs. Needs a hook in `gb/thread`'s fiber
switch that the profiler can use without `gb/thread` depending on it. Most
useful alongside *Multithreaded profiles*, since fiber jobs are the main
multithreaded case.

## Allocator overrides

- **Layers:** base, alloc
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md) (CL4)

Every `Allocator` subclass that overrides `Alloc(size, align)` hides the
one-argument `Alloc(size)`, so it can't be called on the concrete type
(`pool_allocator.Alloc(64)` doesn't compile). `TsAllocator` brings it back with
a `using`, but each other subclass would have to remember to. Make `Alloc()`
and `Free()` public non-virtual functions that call protected virtual
`DoAlloc()` and `DoFree()`, so overriding can't hide anything. This breaks
every subclass outside Game Bits, which must rename its overrides.

## Replace ScopedCall with absl::Cleanup

- **Layers:** base, file, resource
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** none

`gb::ScopedCall` does what `absl::Cleanup` does (from `absl/cleanup/cleanup.h`),
which also has `Cancel()` and `Invoke()`. Switch Game Bits' own uses to
`absl::Cleanup` (`local_file_protocol_test.cc`, `resource_file_reader.cc`, and
`resource_file_writer.cc`), and mark `ScopedCall` deprecated in favor of it.
Removing `ScopedCall` breaks every project that uses it, so each one switches
first, and it is removed once they have.

## Parser rules shared between threads

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md) CL14

`ParserRules::Validate()` is const, but sets the value of each token item from
the lexer it validates against, so two `Parser::Create()` calls with the same
new `ParserRules` on different threads race, and rules validated against one
lexer keep values from it when used with another (a lexer that forces
identifiers to lower case parses a literal differently). Resolve token values
per parser instead, such as in a table the `Parser` builds from its rules and
lexer, so shared rules are never written. `ParserProgram` validates its rules
once and is unaffected.

## Validate chunk contents

- **Layers:** file, resource
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md) CL7 and CL21

A corrupt chunk file can make readers read out of bounds. `ResourceFileReader`
now checks that a chunk is large enough for its count of structs, but
`ChunkReader::ConvertToPtr()` trusts the offset in a `ChunkPtr` (and the string
it points to is not checked for a terminator), and flat buffer chunks are
passed to `flatbuffers::GetRoot()` without verifying them. Check `ChunkPtr`
offsets and strings against the chunk size in `ChunkReader`, and run a
`flatbuffers::Verifier` bounded by the chunk size before a flat buffer chunk
reaches its reader, failing the read if either is invalid.

## Testable render asserts

- **Layers:** render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md) CL22

`RENDER_ASSERT` calls `assert`, and is off unless `SetRenderAssertEnabled()`
turns it on, so tests can't check that misuse is caught: the `DrawList`
argument checks, for instance, were inverted without any test noticing. Let a
test install a handler that records failed render asserts instead of aborting
(as the test backend counts invalid calls), and test the asserts that guard
`DrawList`, `MeshView`, `RenderBufferView`, and `BindingData`.

## Explicit resource bool conversions

- **Layers:** resource
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md) CL21

`ResourcePtr`, `ResourceEntry`, and `ResourceNameReservation` have an implicit
`operator bool`, so comparisons and arithmetic that have no meaning compile
by converting to `bool` (`ResourcePtr` equality used to). Make the conversions
`explicit`. This breaks callers that convert implicitly, such as returning one
from a function that returns `bool`, which each project fixes as it updates.
