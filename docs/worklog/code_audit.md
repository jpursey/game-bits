# Code audit

Every Game Bits library was read through in tier order, looking for bugs,
missing tests, and optimizations that leave the public API unchanged. It landed
as 26 commits named "Code audit: ...", each covering one library or part of
one, and the last a pass over conventions across all of them.

Each bug was fixed after a failing test showed it. Code that can't be unit
tested (the Vulkan backend, windows, and the GUI on screen) was read for bugs
only, and checked by hand in the new `examples/render-check`. Beyond the bugs
fixed, behavior is unchanged except where listed under
[Behavior](#behavior). Work found but not done is in the
[backlog](../backlog.md), each item naming the code audit as its background.

## Behavior

### Changes callers may notice

Most fixes only make code do what it already documented. These change what a
caller can rely on, or break code that depended on the old behavior:
- **gb_base:**
  - `WeakScope` is no longer movable; every move crashed.
  - A `WeakPtr` to a base class that isn't at the start of the scope's object
    (multiple inheritance) is a CHECK failure in `GetWeakPtr()`.
  - A `WeakLock` taken while `InvalidateWeakPtrs()` is waiting is null at once,
    rather than blocking.
  - `ScopedCall` isn't copyable.
  - `ToUtf16()` and `ToUtf8()` return an empty string for invalid input, as
    documented, rather than throwing.
- **gb_container:** `Array` classes have `swap()` and `==` for their own type,
  and can be constant-initialized. Pushing to a full `Queue` that can't grow is
  a CHECK failure. `Queue` no longer overloads `std::swap`; its `swap()` is
  found by argument-dependent lookup.
- **gb_file:**
  - Every public `FileProtocol` function fails for a path that is relative or
    not normalized, so a direct call can't reach outside a protocol's root.
    `FileSystem` already passed normalized absolute paths.
  - `NormalizePath()` removes a `..` at the root of an absolute path.
    `IsRootPath()` is false for an empty path and for a protocol on its own.
  - `LocalFileProtocol` paths are UTF-8, not the Windows code page; a path with
    a host, or that isn't valid UTF-8, is invalid. Copying a file onto itself
    succeeds, as in the other protocols.
  - `FileSystem::GetCurrentFolder()` and `SetCurrentFolder()` don't call a
    protocol without `kCurrentPath`.
- **gb_parse:**
  - `LexerFlag::kNewlineEscape`, `kTabEscape`, and `kHexEscape` are removed;
    they had no effect.
  - Comment starts that are a prefix of another (`#` and `#[`) are rejected,
    and rules with two rules of the same name are invalid.
  - Binary, octal, and hex integers wider than the bit depth fail rather than
    wrap.
- **gb_config:** floats are written as the shortest text that reads back as
  the same value, with ".0" on whole numbers, and UTF-8 is written as is.
  `Config::GetArray()` returns `absl::Span<const Config>`. String to number
  conversions use Abseil's parsing, so they ignore the C locale.
- **gb_collide:** touching shapes always intersect, as libccd's GJK mostly
  treats them, except that a ray or line starting on a triangle misses it.
- **gb_game:** `GameState::ChangeState()` and `ExitState()` from a state that
  isn't active fail with a `kInvalidChangeSibling` trace. A null trace handler
  removes the handlers when set, and is ignored when added.
- **gb_image:** a `Pixel` from a color outside [0, 1] is clamped, and NaN
  becomes 0.
- **gb_job:** destroying a `FiberJobSystem` waits for every job to finish,
  pending ones included. Past its `shutdown_timeout` (10 seconds by default,
  timed by the context's `Clock`), it ends the process with a fatal error naming
  the unfinished jobs. Idle job threads sleep rather than spin.
- **gb_message:** deleting an endpoint no longer waits for the handlers of
  endpoints subscribed to it. An endpoint that subscribes after a message is
  sent gets it only if its dispatcher already had a copy queued.
- **gb_resource:** registering a resource type under a name already in use
  fails. `ResourcePtr`s compare the resources they point to. A failed
  `ResourceFileWriter::Write()` leaves the existing file.
- **gb_render:**
  - Deleting a texture or mesh while an `ImageView` or `MeshView` of it is
    active is a CHECK failure.
  - `Mesh::Set()` fails past `kMaxMeshVertices` (65535, as indices are 16 bits).
  - `kMaxTextureWidth` and `kMaxTextureHeight` are 8192, not 8096, and the
    maximum itself loads.
  - Material types save their depth test and raster mode, and loading takes
    bindings from the scene type and shaders, as creating does. Older files
    still load; the binding fields they saved are deprecated.
  - Scenes and material types start from their scene type's defaults.
- **gb_render_vulkan:** growing a `kStaticWrite` render buffer that was
  recently rendered fails, as does a partial update of a mipmapped
  `kStaticWrite` texture, or setting a texture in a `kStaticWrite` texture
  array that is in use. Each is documented on the function.
- **gb_imgui:** several `ImGuiInstance`s can share a render system.
  `Create()` makes the new instance active. `AddTexture()` returns the same ID
  for the same texture, and null for an invalid one. A GUI over
  `kMaxMeshVertices` skips its frame, logging an error.
- **Includes:** unused includes were removed in every library, so code that
  got a header indirectly through a Game Bits header may need to include it
  itself.

### New APIs and test support

- `ThreadTester::WaitForLoopCount()` waits until a `RunLoop()` has run its
  function a number of times, so stress tests don't count by hand or sleep.
- `Token::HasValue()` compares a token's value in place, without allocating.
- `FiberJobSystem`'s optional `shutdown_timeout` context value
  (`kKeyShutdownTimeout`).
- `kMaxMeshVertices` in `render_types.h`.
- `gb_render`'s test backend records the commands of each `DrawList` it draws
  (`TestRenderBackend::State::draw_commands`), and `TestBindingData` holds
  texture arrays. `gb_imgui`'s tests draw with it.
- `examples/render-check` exercises the Vulkan backend and `gb_imgui` by hand:
  resizing, recreating instances each frame, painting each texture volatility,
  partial texture and texture array updates, and GUIs that grow past the mesh
  limit. It shows memory use and every logged warning and error, including the
  validation layer's.

## Structure

Each library's notable fixes, lower tiers first. The commits have the full
list for each library.

- **gb_test:** `ThreadTester::RunThenSignal()` crashed if its signal was
  already notified.
- **gb_base:**
  - Taking a second `WeakLock` deadlocked with an invalidating thread.
  - `Context` move assignment leaked owned values, and self-move deadlocked.
    Setting a value to the pointer it already held kept the old ownership.
  - `Allocator::New()` and `TsAllocator` copied their arguments, and
    `NewArray()` moved the same arguments into every element.
  - `StdAllocator` didn't compile in a container, and `IsPlatformSupported()`
    had no definition.
- **gb_alloc:** `PoolAllocator` misaligned every other slot for sizes that
  aren't a multiple of their alignment, and `TestAllocator::IsValidMemory()`
  passed almost any pointer.
- **gb_container:** `Array3d` dropped its Z size, and `Queue` copies grew their
  capacity each time, with moves that weren't `noexcept`.
- **gb_thread:**
  - Thread and fiber names read past short names and overflowed on long ones.
  - `SwitchToFiber()` let another thread switch to a fiber still running.
  - Joined or detached threads leaked their Windows handles if already exited.
- **gb_profile:** functions returning references couldn't be hooked, and a very
  long slow frame time reported every frame as slow.
- **gb_file:**
  - `PathMatchesPattern()` failed patterns whose parts repeat in the path.
  - `File::ReadLine()` cut lines at NUL bytes.
  - `ChunkWriter` miscounted padding for unowned data, and `ChunkReader` move
    assignment leaked.
  - Recursive folder creation under a missing host looped forever.
  - Copying between two names for one protocol emptied files or recursed
    forever.
  - `LocalFile` lost a write straight after a read, as an `fstream` needs a seek
    between them.
- **gb_sdl:** no bugs; `SdlToGLog()` gained tests and an include guard.
- **gb_parse:**
  - A `Lexer` kept a view of the caller's filename.
  - Content after the first had wrong rewinds and line lookups.
  - Token indexes from another lexer read past a line's tokens.
  - An unmatched symbol or comment start never advanced.
  - Zero floats were invalid.
  - `ParserProgram::Create()` raced on its shared rules.
  - Matching tokens and looking up rules allocated a string each time.
- **gb_config:** text reading raced on its cached parser programs, floats lost
  precision on a round trip, arrays wrapped in the wrong places, and a negative
  `ResizeArray()` threw.
- **gb_collide:** sphere / sphere, ray / sphere, and sphere / triangle tests
  were wrong in most cases (the library had no tests), and the ray / triangle
  overloads that return a position never returned on a hit.
- **gb_game:** one late frame lost the frame rate cap for good, and a parent
  state updated again when its child changed state.
- **gb_image:** regions of an empty image asserted, a const `Image` had no
  view, and `Pixel::Packed()` broke strict aliasing (it now uses
  `std::bit_cast`).
- **gb_job:** destruction dropped pending jobs, `SetVerboseLogging(false)`
  turned logging on, and `Wait()` on a done counter switched fibers.
- **gb_message:**
  - Dispatching walked subscriber sets unlocked, which raced with removal.
  - Messages went to the wrong dispatcher, and circular subscriptions looped
    forever.
  - Sending now finds the receivers under the lock and queues one copy per
    dispatcher.
  - A queued message with no handler stopped an endpoint's queue.
  - `MessageStackEndpoint::Remove()` left handlers unable to be pushed again.
- **gb_resource:**
  - `Load()` deadlocked on a named resource never referenced.
  - Short names dangled as more were added.
  - Concurrent releases could leave a resource loaded.
  - Duplicate name reservations released each other's names.
  - Reading a file twice crashed, and chunks were read past their end.
- **gb_render:**
  - Copies of `LocalBindingData` shared their source's buffer, so every
    material changed its type's defaults; values were also misaligned.
  - `MeshView` returned index counts as triangle counts, and shifted data with
    overlapping `memcpy`.
  - Large texture regions overflowed size checks.
  - Material types lost a shared binding's stages, and loading accepted what
    creating rejects.
  - A chunk reader that failed after creating its resource left it behind for
    the next read.
- **gb_render_vulkan:**
  - A failed present redrew the previous frame's draws.
  - Garbage collectors rotated on failed frames, freeing resources still in
    use.
  - Descriptor sets were never reused, so pools grew without bound.
  - Growing buffers and partially updating textures and texture arrays lost or
    corrupted their contents.
  - Shader code that wasn't 32-bit aligned was passed to Vulkan.
- **gb_render_vulkan_sdl:** `SdlVulkanWindow` destroyed its callback before its
  message endpoint.
- **gb_imgui:**
  - A second instance failed to create, and `LoadFonts()` made a new font
    texture on every call.
  - `AddTexture()` made a material on every call.
  - Drawing stopped for good past half the mesh limit, or after one failed
    edit.
  - `ImDrawCallback_ResetRenderState` left the next draw with no material or
    mesh.
  - `gb_imgui_sdl` only builds ImGui's SDL backend, so it had nothing to audit.

### Conventions across src/gb

The final pass checked these across every library, and they now hold:
- Include guards are `GB_<DIR>_<FILE>_H_`, and every file starts with the
  copyright header.
- Every header in a library with a `*_types.h` includes it. Not every library
  needs one: `gb_game` forward declares `GameStateInfo` and `GameStateMachine`
  in `game_state.h` to break an include cycle, without one.
- No unused includes, checked with clang-tidy's `misc-include-cleaner`, other
  than ones it reports wrongly: umbrella headers (`<windows.h>`,
  `glm/glm.hpp`, `SDL.h`, `flatbuffers/flatbuffers.h`, and similar), `<new>`
  for placement new, headers needed to destroy a `std::unique_ptr`, a test's
  own header, each `*_types.h`, and `vulkan_types.h`'s Vulkan-Hpp setup.
- Libraries list Abseil targets as dependencies (`_DEPS`), not libraries.
- Tests don't wait on real time, except where a race needs it to be likely and
  nothing can arrange it (some job and message tests), and the `Game::Run()`
  overloads that make their own context.

### Deliberate choices

Reviewed and kept as they are:
- `ValidatedContext::Complete()` keeps a shared context alive until the
  `ValidatedContext` is destroyed or reassigned, as `Assign()` depends on it.
- A failed `LocalFile` write leaves the file invalid, as its contents are
  unknown.
- `SdlToGLog()` keeps its name, and critical SDL messages are `DFATAL`.
- Each message dispatcher walks the subscription graph again when it delivers,
  so an endpoint unsubscribed by then doesn't receive the message.
- Shapes' default constructors leave their members uninitialized, as
  `glm::vec3` does, and colors convert to pixels by truncating (every 8-bit
  value round trips).
- `WriteBindingData()` writes nothing for `kStaticWrite` constants, which
  can't be read back, as `SaveMaterial()` documents.
