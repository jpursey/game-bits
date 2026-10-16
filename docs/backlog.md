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

## Report read errors

- **Layers:** file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** `RawFile::Read()` in `raw_file.h`, and the copy loops in
  `FileSystem::GenericCopyFile()` and `FileProtocol::BasicCopyFile()`

A read that fails looks the same as one that reaches the end of the file:
`RawFile::Read()` returns a short count either way, and the `File` stays valid,
so a caller can't tell a failed read from a short file. The copy loops stop at
the first short read, so a copy whose source fails part way leaves a truncated
file and reports success (copies within `LocalFileProtocol` use
`std::filesystem::copy_file`, and are unaffected). Let a raw file report a
failed read, have `File` become invalid on one as it does on a failed write,
and have the copies check for it. The two copy loops are the same, so share
them while there. `LocalFile::Read()` also tries to work out how much a failed
read read, which a failed stream can't report.

## Default texture for unset bindings

- **Layers:** render, render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Drawing with a texture binding that was never set crashes in
`VulkanBindingData::OnRender()`. Skipping the binding would leave its
descriptor unwritten or pointing at a deleted image, which is worse. Give the
backend a default texture (one opaque pixel, say) that every texture binding
starts with and falls back to, and check it by hand in `examples/render-check`
with a material whose texture is never set.

## clang-cl build

- **Layers:** container, parse, job
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Game Bits is meant to build with clang-cl as well as MSVC (CMakeSettings.json
has `x64-Clang-Debug` and `x64-Clang-Release`), but clang 19 rejects some of
it:
- `TokenIndex`'s defaulted `operator<=>` in `parse_types.h` compares
  `uint32_t` bit fields, which clang won't narrow to the `int` they promote
  to. Every file that includes `parse_types.h` fails.
- `array.h`'s `using` declarations of the base's dependent types need
  `typename`, and one function reads the base's protected `data`.
- `fiber_job_system_test.cc` has an unused lambda capture, an error with
  warnings as errors.

These were found by clang-tidy, which parses with clang; there may be more
once those build. Building needs Visual Studio's "C++ Clang tools for Windows"
component, as the MSVC standard library rejects older clang versions. Fix them,
build and test both clang configurations, and consider building one in CI so it
stays working.

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
- **Background:** [Code audit](worklog/code_audit.md)

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
- **Background:** [Code audit](worklog/code_audit.md)

`ParserRules::Validate()` is const, but sets the value of each token item from
the lexer it validates against, so two `Parser::Create()` calls with the same
new `ParserRules` on different threads race, and rules validated against one
lexer keep values from it when used with another (a lexer that forces
identifiers to lower case parses a literal differently). Resolve token values
per parser instead, such as in a table the `Parser` builds from its rules and
lexer, so shared rules are never written. `ParserProgram` validates its rules
once and is unaffected.

## Parser groups merge their children's items

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** `Parser::MatchGroup` and `Parser::MatchRuleItem` in
  `parser.cc`

`Parser::items_` tells each rule item where its named sub-items go: the
enclosing group's items for an unnamed group or unscoped rule (`<rule>`), or
null for one that keeps its own. `MatchGroup` sets it before each sub-item, and
every match function must restore it before returning (`MatchGroup` does with
an `absl::Cleanup`, `MatchRuleItem` by hand). One that doesn't sends the items
of later sub-items to the wrong map.

Remove the member and let the parent decide instead. Each group returns its own
items, and when an unnamed sub-item matches, `MatchGroup` merges that match's
items into its own if the sub-item inlines them: a group does, a rule name does
unless it is scoped, and a token has none. That removes `items_`, the
`scope_items` argument to `MatchRuleItem`, and the cleanup, and a failed match
leaves nothing behind because it has no items to merge. The existing parser
tests, including those for failed inline groups, cover the behavior.

## Validate chunk contents

- **Layers:** file, resource
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

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
- **Background:** [Code audit](worklog/code_audit.md)

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
- **Background:** [Code audit](worklog/code_audit.md)

`ResourcePtr`, `ResourceEntry`, and `ResourceNameReservation` have an implicit
`operator bool`, so comparisons and arithmetic that have no meaning compile
by converting to `bool` (`ResourcePtr` equality used to). Make the conversions
`explicit`. This breaks callers that convert implicitly, such as returning one
from a function that returns `bool`, which each project fixes as it updates.

## Resource loading races

- **Layers:** resource
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Resources loaded or released from several threads still have gaps:
- Loading a name that another thread is loading fails, as the `TODO` in
  `ResourceSystem` says, rather than waiting for that load.
- A resource is in the system from its `Resource` base constructor on, so two
  threads reading files that embed the same resource ID can find each other's
  resource before it is fully constructed, or just before a failed read
  deletes it.
- A reference added and removed while a release handler runs, after the
  handler's delete failed because of it, leaves the resource unreferenced and
  not released.
- Resources embedded in a file read without a `ResourceSet` are never
  referenced, so they stay loaded until their manager is destroyed.

Each needs a test that makes the interleaving happen deterministically.

## Vulkan frame failure loose ends

- **Layers:** render_vulkan
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Smaller problems in the Vulkan backend, none of which can be unit tested:
- If the command buffer fails to begin after a swap chain image is acquired,
  the image's semaphore is left signaled, so the next acquire waits on a
  signaled semaphore.
- While `BeginFrame()` keeps failing (such as while minimized), nothing is
  garbage collected, so resources deleted meanwhile are only freed once
  rendering resumes.
- A `kPerFrame` texture's host buffers start uninitialized, so mipmaps
  generated by its first partial update include garbage.
- `VulkanBindingDataFactory::NewBindingData()` builds the binding data while
  holding the factory's lock.

Check each by hand in `examples/render-check`.

## GPU-side kStaticWrite updates

- **Layers:** render_vulkan
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`kStaticWrite` resources keep no copy of their data on the host, so updates
that need the existing contents fail:
- A partial update of a mipmapped `kStaticWrite` texture fails, as its mipmaps
  would be generated from host memory that doesn't hold the rest of the
  texture. Generating mipmaps on the GPU (with `vkCmdBlitImage`) would let it
  work, and would be faster for every volatility.
- Growing a `kStaticWrite` render buffer that was recently rendered fails, as
  the old values can't be read back. A device to device copy from the old
  buffer would keep them. Nothing grows one today.

## Large GUIs across meshes

- **Layers:** imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`ImGuiInstance` draws a GUI as one mesh, and its indices and
`DrawList::DrawPartial()`'s vertex offset are 16 bits, so a GUI over
`kMaxMeshVertices` skips its frame, logging an error. Split a large GUI's draw
lists across several meshes instead, checked by hand with
`examples/render-check`'s "Go over the vertex limit" button, which should then
keep drawing.

## Register returns matching render types

- **Layers:** render, imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`RenderSystem`'s `Register*` functions fail for a name already registered, so
code that may run more than once (such as each `ImGuiInstance`'s `Init()`)
has to look each type up first and register it only if missing. That also
never checks that the existing type matches. Have `Register*` return the
existing type when its definition matches, and fail when it differs, then
simplify `ImGuiInstance::Init()` to just register.

## Shared gb_render test support

- **Layers:** render, imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`gb_imgui`'s tests draw with `gb_render`'s test backend, which they compile
from its source files, listed again in `gb_imgui`'s `CMakeLists.txt`. Make
the test backend and `RenderTest` a library target that tests link instead,
so the list is kept in one place.

## Deterministic concurrency tests

- **Layers:** game, job, message, resource
- **Size:** medium
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Several tests still sleep or use the real clock, and several concurrency fixes
have no test that fails without them, as nothing lets a test arrange the
interleaving:
- Some `FiberJobSystem` tests sleep in jobs so that waits are likely to block.
- `MessageSystem`'s `RemoveEndpointFromOtherThread` and two thread dispatcher
  destruction tests sleep to make their race likely. The message system's
  fixes for erasing a stale iterator, a concurrent erase, and destroying an
  endpoint or stack while delivering have no deterministic tests.
- Resource system races other than the release race have none either.
- The `Game::Run()` overloads that make their own context run a frame on the
  real clock, as nothing can give them a fake one.

Add test hooks (a callback at the point of interest, or a clock passed in)
that let each test wait for the state it needs, rather than sleeping.

## Lexer storage on re-parse

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Decoded strings, escaped characters, and forced-case identifiers are stored
again each time a token is parsed again after the parser backs up, so a
lexer's memory grows with the parsing work done rather than the text. Cache
them per token, which needs care where a block comment re-tokenizes a line.

## Symbols longer than 7 characters

- **Layers:** parse
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

A `Symbol` holds up to 7 characters and silently truncates a longer one, so a
lexer config with a long symbol gets a shorter one than it asked for. Reject a
symbol that is too long where it is made, and in `LexerConfig` validation,
with a test.

## Config text edge cases

- **Layers:** config
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Text configs still don't round trip a few values:
- Infinities, NaNs, and subnormal floats are written as text that can't be
  read back, as JSON has no text for them. Pick a text form for them in the
  Game Bits format.
- JSON output escapes control characters as `\xNN`, which JSON doesn't
  support (`\u00NN` is the JSON form).
- A non-compact empty map is written as `{` and `}` on separate lines with a
  blank line between.
- Reading recurses once per nesting level, so very deeply nested text can
  overflow the stack. A nesting limit would fail it instead.

## Local folder listings stop at unreadable entries

- **Layers:** file
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`LocalFileProtocol`'s folder listing stops at an entry whose status can't be
read (such as a dangling link), returning only the entries before it. Skip
such entries, or list them with an invalid type, and check it by hand with a
dangling junction.

## Collision precision and conversions

- **Layers:** collide
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

Ray / triangle intersection treats a ray as parallel to the triangle using an
absolute epsilon, so a ray with a very short direction can miss; scale it by
the direction's length. Separately, `CLine` and `CTriangle` convert implicitly
from a pointer to an array, so any pointer converts; making them `explicit`
breaks callers that rely on it.

## Game state trace details

- **Layers:** game
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

A `kInvalidChangeSibling` trace from a state that isn't active names that
state only in its message, as `GameStateTrace` has no field for the requesting
state. Trace messages are also built when tracing is enabled but the handler
is null. Add the field, and skip building the message without a handler.

## Image size checks

- **Layers:** image
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`Image` and `ImageView` don't check their sizes or regions, so a negative
size, or one whose pixel count doesn't fit in an `int`, goes unnoticed. Check
them where they are made, as `gb_render`'s texture loading now does, with
tests.

## Binding compatibility by type key

- **Layers:** render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`Binding::IsCompatible()` compares constants types by pointer, while
`Binding::operator==` compares their `TypeKey`, so two bindings can be equal
but not compatible. Compare by `TypeKey` in both, with a test.

## size_t in public APIs

- **Layers:** job, render
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

A few public APIs still use `size_t`, against the Game Bits style:
`JobDataHandle`, and the sizes `Texture` and `TextureArray` take.
`TextureArray::Get()` also returns pixels through a vector taken by reference
rather than a pointer. Changing them breaks callers, which each project fixes
as it updates.

## ImGui contexts made outside ImGuiInstance

- **Layers:** imgui
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

`ImGuiInstance::GetActive()` assumes any current ImGui context belongs to an
`ImGuiInstance`, as it reads the instance from the context's
`ImGuiIO::UserData`. So a
context made directly with `ImGui::CreateContext()` must not be current while
ImGui uses its file functions, which find the instance this way. Mark the
contexts an instance makes, so `GetActive()` returns null for any other.

## Repeatable include check

- **Layers:** all
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** [Code audit](worklog/code_audit.md)

The code audit found unused includes with clang-tidy's `misc-include-cleaner`,
but nothing in the tree runs it again, so unused includes, bad include guards,
and headers missing their `*_types.h` creep back. It also reports about 40
includes that are needed (umbrella headers such as `<windows.h>` and
`glm/glm.hpp`, `<new>` for placement new, headers needed to destroy a
`std::unique_ptr`, and each `*_types.h`). Decide whether to add:
- A `.clang-tidy` with the check (and `IgnoreHeaders` for the umbrella
  headers), run by a script or a CMake option that sets
  `CMAKE_CXX_CLANG_TIDY`. Running clang-tidy over MSVC compile commands needs
  `-D_CRT_USE_BUILTIN_OFFSETOF`.
- `// IWYU pragma: keep` on the includes it reports wrongly.
- A script that checks include guards and the `*_types.h` rule.
