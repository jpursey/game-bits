# Shared mesh buffers and indirect draws

Today every `Mesh` owns a vertex buffer and an index buffer (two GPU
allocations), its indices are 16 bits (`kMaxMeshVertices` is 65,535), and every
draw is a command of its own, with instance binding data of its own. A voxel
world draws thousands of small meshes (Terra Diem had 4,212 chunk meshes within
its default view distance) and rebuilds them often, from job threads, so the
cost per mesh and per draw dominates.

This adds mesh pools: large vertex and index buffers for one vertex type, which
many meshes are allocated from, from any thread, with no lock the render loop
needs. A pool's indices are 16 or 32 bits. Any range of a pooled mesh can be
drawn on its own, and many ranges can be drawn in one indirect draw, whose
ranges come from a storage buffer that the program fills. Each draw finds its
own data (such as a chunk's position) in another storage buffer, by an index
the draw carries.

Terra Diem would allocate each chunk's mesh from a pool, with its faces grouped
by direction as ranges, and draw every visible chunk in a few indirect draws,
skipping the directions that face away from the camera.

## Behavior

Nothing existing changes: `Mesh`, `DrawList::SetMesh()`, `Draw()`, and
`DrawPartial()` work as before. The additions:

- **Mesh pools.** `RenderSystem::CreateMeshPool()` creates a `MeshPool` (a
  resource) for one vertex type and index type (16 or 32 bits), with a starting
  vertex and triangle capacity.
  - `MeshPool::CreateMesh()` copies a mesh's vertices and indices into the pool,
    and returns a `PooledMesh`, which holds the mesh's place in the pool until
    it is destroyed. It never fails for lack of room: a full pool grows.
  - Pooled meshes can't be edited. A program changes one by creating its
    replacement and destroying the old one once it no longer draws it.
  - A pooled mesh's indices count from its own first vertex, as a `Mesh`'s do.
    In a 16-bit pool, each mesh has at most `kMaxMeshVertices` vertices, but
    the pool can hold any number.
  - `CreateMesh()` and destroying a `PooledMesh` are thread-safe, and take no
    lock that the render loop needs. Neither waits for the GPU.
  - The data is copied to the GPU before the next frame that is ended (by
    `RenderSystem::EndFrame()`) draws, in one batch with every other pooled
    mesh created since the last frame.
  - A destroyed mesh's place is reused only once no frame that may draw it is
    still in flight. So a mesh can be destroyed as soon as the program has
    drawn it for the last time, even within the same frame.
  - `MeshPool::GetStats()` reports the space used and free, and how fragmented
    the free space is: the number of free ranges, and the largest.
  - The pool must outlive its meshes; destroying it first is a `CHECK` failure.
- **Draws.** A `MeshDraw` is a range of a pool's indices to draw: the index
  count, the first index, the vertex offset added to each index, and an
  instance range. A `PooledMesh` gives the `MeshDraw` for all of its triangles,
  or a range of them. Its layout is that of Vulkan's, Direct3D 12's, and
  OpenGL's indirect draw arguments, so a buffer of them is drawn as is.
- **Drawing ranges.** `DrawList::SetMeshPool()` sets the pool to draw from, as
  `SetMesh()` sets a mesh, and `DrawList::Draw(const MeshDraw&)` draws a range
  from it.
- **Indirect draws.** `DrawList::DrawIndirect(StorageBuffer* draws, int first,
  int count)` draws `count` ranges from the current pool, read from a storage
  buffer of `MeshDraw` values (a type the program registers, as for any
  storage buffer). The GPU reads them when the frame is drawn, so the buffer
  can be filled by the program on the CPU, or later by a compute pass. The
  draws are one multi-draw call where the GPU supports it, and one call each
  where it doesn't.
- **Each draw's data.** Shaders read the draw's `first_instance` as
  `gl_InstanceIndex` (with one instance, as each draw has for now), so a
  program sets it to the index of the draw's data in a storage buffer bound in
  the material or instance data. This works for direct draws too, and on GPUs
  without multi-draw, unlike `gl_DrawID`.

## Design

The pool's bookkeeping (which ranges are used, free, and waiting to be reused)
is in `gb_render`, independent of any graphics API and covered by unit tests
with the test backend. The backend only provides the buffers: writes into them
from any thread, staged and copied on the render thread, growth, and when freed
space may be written again. The frames in flight stay inside the backend, as
`RenderBackend` has no other caller that needs them.

### Names

| Name | Alternatives | Notes |
| --- | --- | --- |
| `RangePacker` | `SpaceMap`, `FreeList` | Not an "allocator": in Game Bits, `Allocator` and `PoolAllocator` hand out memory, and this holds offsets only. `SpaceMap` is a filesystem term, and `FreeList` names a memory allocator's internals. |
| `MeshPool` | `SharedMesh`, `MeshBuffers` | The backlog item's own term. Unrelated to `PoolAllocator`. |
| `PooledMesh` | `PoolMesh`, `MeshSlot`, `MeshRange` | `MeshRange` would collide with the ranges a mesh is drawn in. |
| `MeshDraw` | `DrawRange`, `IndexedDraw` | Distinct from `DrawList` and `DrawCommand`. |
| `IndexType` (`kUint16`, `kUint32`) | `IndexSize` | No existing `IndexType` in Game Bits; `vk::IndexType` is only in Vulkan code. |
| `MeshPoolBuffers` | `RenderMeshPool`, `PoolBuffers` | The backend's half of a pool, as `RenderBuffer` is for a `Mesh`. Internal. |

### RangePacker

```
// Packs ranges of offsets into a capacity, such as places in a GPU buffer,
// keeping track of the gaps between them. It holds offsets only, never memory.
//
// This class is thread-compatible.
class RangePacker final {
 public:
  // `capacity` must be positive.
  explicit RangePacker(int capacity);

  int GetCapacity() const;
  int GetUsed() const;
  int GetGapCount() const;
  int GetLargestGap() const;

  // Places a range of `size` offsets, returning its first offset, or nullopt
  // if no gap is big enough.
  std::optional<int> Place(int size);

  // Removes a range Place() returned, leaving a gap.
  void Remove(int offset, int size);

  // Increases the capacity, adding the new offsets as a gap.
  void Grow(int capacity);
};
```

- In `gb_container`, as it is a generic data structure holding no memory, like
  `OffsetArray` and `Queue`.
- Best fit, with gaps kept by offset (to merge with the gaps on either side of
  a removed range) and by size (to find the best fit), in Abseil btrees.
  Placing and removing are O(log n) in the number of gaps.
- The caller passes the size to `Remove()`, as the pool keeps it anyway, so the
  packer doesn't need to.


### MeshPool and PooledMesh

```
class MeshPool final : public Resource {
 public:
  const VertexType* GetVertexType() const;
  IndexType GetIndexType() const;

  // Thread-safe.
  MeshPoolStats GetStats() const;

  // Copies the vertices and indices into the pool, returning null if they
  // are the wrong types, or the pool couldn't grow to hold them. Thread-safe.
  // One overload for each index type, so a std::vector converts to the span.
  template <typename Vertex>
  std::unique_ptr<PooledMesh> CreateMesh(absl::Span<const Vertex> vertices,
                                         absl::Span<const uint16_t> indices);
  template <typename Vertex>
  std::unique_ptr<PooledMesh> CreateMesh(absl::Span<const Vertex> vertices,
                                         absl::Span<const uint32_t> indices);
  template <typename Vertex>
  std::unique_ptr<PooledMesh> CreateMesh(absl::Span<const Vertex> vertices,
                                         absl::Span<const Triangle> triangles);
};

// The vertex and triangle space each has a capacity, the amount used, the
// amount of that waiting for reuse, the number of free ranges, and the
// largest free range.
struct MeshPoolStats {
  struct Space;
  int mesh_count;
  Space vertices;
  Space triangles;
};

class PooledMesh final {
 public:
  // Frees the mesh's place in the pool, once no frame that may draw it is in
  // flight. Thread-safe.
  ~PooledMesh();

  MeshPool* GetPool() const;
  int GetVertexCount() const;
  int GetTriangleCount() const;

  // Returns the draw of all of the mesh's triangles, or `triangle_count` of
  // them from `first_triangle`, with `first_instance` set to `draw_index`.
  MeshDraw GetDraw(int draw_index = 0) const;
  MeshDraw GetDraw(int first_triangle, int triangle_count,
                   int draw_index = 0) const;
};

struct MeshDraw {
  uint32_t index_count;
  uint32_t instance_count;
  uint32_t first_index;
  int32_t vertex_offset;
  uint32_t first_instance;
};
```

- Capacities and stats are in vertices and triangles, as for `Mesh`
  (`CreateMesh(vertex_type, volatility, max_vertices, max_triangles)`), and
  only the backend and `MeshDraw` count indices.
- A pool has a `RangePacker` each for vertices and triangles, behind a mutex
  that only `CreateMesh()`, `~PooledMesh()`, and `GetStats()` take. The render
  loop never takes it. `CreateMesh()` places the mesh under the lock, and
  writes its data after, as the space is its own by then.
- A destroyed mesh's ranges go on a list with a reuse ticket from the buffers,
  and `CreateMesh()` removes the ones the buffers say can be reused from the
  packers before it places its own. Tickets never decrease, so the list is in
  ticket order and is checked from the front.
- When a range doesn't fit, the pool grows its packer and buffers to at least
  double the capacity, and to at least the old capacity plus the range, so the
  range fits in the new space at the end. The new capacity is used at once
  (offsets never move), and the backend makes the buffers bigger before the
  next frame's copies. Growth happens under the lock, so any write to the new
  space is queued after it.
- A `PooledMesh` holds a pointer to its pool, and the pool counts its meshes, so
  destroying the pool first is a `CHECK` failure, as for a `MeshView`.
- `RenderSystem::CreateMeshPool()` validates the vertex type, and that the
  capacities are positive.
- Pools aren't saved or loaded, as nothing needs it.

**Brittleness:** a `MeshDraw` is a copy of offsets, so one the program keeps
(in a storage buffer it reuses across frames, say) after destroying its mesh
draws whatever later takes that place. The program must refill its draw buffers
as it destroys meshes. A frame-by-frame (`kPerFrame`) draw buffer, refilled
every frame from the live meshes, avoids this.

### MeshPoolBuffers (backend)

```
// The backend's buffers for a MeshPool. Every function is thread-safe.
class MeshPoolBuffers {
 public:
  // Copies a mesh's vertices and indices into the buffers, for the next frame
  // that is ended to draw, or the one after if that frame is already ending.
  // No other write to the same values is in progress, and no frame in flight
  // draws them. One call per mesh, so a backend reserves staging space once.
  virtual bool WriteMesh(int vertex_offset, const void* vertices,
                         int vertex_count, int index_offset,
                         const void* indices, int index_count) = 0;

  // Makes the buffers at least this big, keeping their contents, before the
  // next frame that is ended draws. Capacities only increase. It runs under
  // the pool's lock, so it does as little as it can.
  virtual bool Grow(int vertex_capacity, int index_capacity) = 0;

  // Returns a ticket for space the pool frees now. Tickets never decrease.
  virtual uint64_t GetReuseTicket() const = 0;

  // Returns true if space freed with the ticket can be written again: no frame
  // that may draw it is in flight, and no write to it is still to be copied.
  // Once a ticket can be reused, so can every earlier one.
  virtual bool CanReuse(uint64_t ticket) const = 0;
};

// In RenderBackend.
virtual std::unique_ptr<MeshPoolBuffers> CreateMeshPoolBuffers(
    RenderInternal, int vertex_size, IndexType index_type,
    int vertex_capacity, int index_capacity) = 0;
```

- The test backend's version keeps the values in vectors, and copies writes
  into them when its frame ends, so tests see what a frame would draw. Its
  ticket is the backend's frame, and a configurable number of frames must end
  after it before the space is reused, so tests control reuse.
- Sizes in bytes are `int64_t`, as a pool's buffers may hold more than 2 GiB.

### Drawing pools

- `DrawCommand` gains `kMeshPool` (sets both buffers and the index type), and
  `kIndirect` (a storage buffer, first, and count). Its `Draw` gains
  `first_instance`, and its `vertex_offset` becomes 32 bits, as a pool's are
  large.
- `DrawList::SetMeshPool(MeshPool* pool, Material* material = nullptr,
  BindingData* instance_data = nullptr)` checks the material's vertex type
  against the pool's, as `SetMesh()` does for a mesh, and replaces any current
  mesh. `Draw()` and `DrawPartial()` need a mesh, and the new `Draw(const
  MeshDraw&)` and `DrawIndirect()` need a pool.
- `DrawIndirect()` asserts that the storage buffer holds `MeshDraw` values, and
  that `first + count` is within it. What is in the values is not checked: the
  GPU reads them.
- The test backend records the commands, as it does today, so tests check what
  was drawn.

### Vulkan

- **Uploads.** `VulkanMeshPoolBuffers` keeps device-local vertex and index
  buffers, and host-visible staging blocks. A write takes space in the current
  block (a short lock, which only writers take), copies the data in, and
  pushes the copy onto a lock-free queue (`moodycamel::ConcurrentQueue`,
  already vendored). At the start of `EndFrame()`, the render thread drains
  the queue, records the copies in batches (one `copyBuffer()` per block and
  buffer), and one barrier before the render pass. A block is freed by the
  garbage collector once every copy from it has been drawn by a finished
  frame.
- **Growth** is queued too, so it is applied in order: the render thread makes
  the bigger buffers, copies the old contents (on the GPU), and passes the old
  buffers to the garbage collector, before the copies that follow it.
- **No hazards on reuse.** The reuse ticket is the backend's frame, and
  `CanReuse()` is true once the frame after the ticket's is no longer in use
  (`IsFrameInUse(ticket + 1)`, as Vulkan already has internally). The extra
  frame covers a write queued during the ticket's frame but only copied in the
  next. Writes are only recorded in `EndFrame()`, after `BeginFrame()` has
  waited for the frame two before to finish, so nothing in flight reads a range
  when it is written, and two writes to one range are never in flight at once.
- **Indirect draws.** Storage buffers are also created with
  `eIndirectBuffer` usage, so any can be drawn from. `DrawIndirect()` is one
  `drawIndexedIndirect()` with the count where the device has
  `multiDrawIndirect` (split at `maxDrawIndirectCount`), and one call per draw
  otherwise.
- **Features.** `drawIndirectFirstInstance` is required, as
  `samplerAnisotropy` is: every desktop GPU has it. `multiDrawIndirect` is
  enabled where the device has it, and a backend option turns it off, so the
  fallback can be checked on a GPU that has it.

### Not included

- **Compaction.** Moving meshes within a pool would change the offsets in every
  `MeshDraw` a program keeps, so it needs a way to tell the program, or draws
  built on the GPU. A fragmented pool can instead be rebuilt by recreating its
  meshes in a new one, which `GetStats()` lets a program decide. This goes in
  the backlog when the feature is complete.
- **Draw counts from the GPU** (`drawIndexedIndirectCount()`, Vulkan 1.2), for
  culling in a compute pass. That comes with compute shaders.
- **Instance counts above one**, which *Instanced draws* adds.
- **`Mesh` on pools.** `Mesh` keeps its own buffers. Moving it onto pools would
  change its editing (`MeshView`) and volatility, for no gain to its callers.

### To confirm

- `gl_InstanceIndex` includes `first_instance` in indirect draws, and in the
  fallback without multi-draw (CL6).
- The device in use has `drawIndirectFirstInstance` and `multiDrawIndirect`
  (CL6).
- In block-world, drawing every chunk from one pool with an indirect draw is
  faster than drawing each chunk on its own (CL7).

## CLs

Each `gb_render` CL changes the backend interface, so it also gives the Vulkan
backend just enough to build (failing to create what it can't yet), and the
next CL implements it there and checks it by hand in `examples/render-check`.

### CL1 [x] container: RangePacker

Depends on: nothing.

- `RangePacker`, in `gb_container`.
- Unused, so no visible change.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: best fit picks the smallest gap that fits, and the lowest offset
  of equal gaps; removing a range merges it with the gap on either side, or
  both; filling the capacity, then failing; growth adds a gap, merged with a
  gap at the end; the counts and largest gap follow each change; a randomized
  run against a simple model (a flag per offset) agrees after every step.

### CL2 [x] render: Mesh pools

Depends on: CL1.

- `IndexType`, `MeshDraw`, `MeshPool`, `PooledMesh`, `MeshPoolStats`, and
  `RenderSystem::CreateMeshPool()`.
- `RenderBackend::CreateMeshPoolBuffers()`, with `MeshPoolBuffers` (including
  its reuse tickets).
- Test backend: `TestMeshPoolBuffers`, with a frame counter and a configurable
  number of frames before reuse.
- Vulkan backend: `CreateMeshPoolBuffers()` returning null.

**Verify**
- Standard checks.
- Unit tests: a mesh's vertices and indices reach the test buffers when the
  frame ends; `GetDraw()` offsets for whole meshes and ranges; the wrong
  vertex or index type fails; more than `kMaxMeshVertices` vertices fail in a
  16-bit pool and succeed in a 32-bit one; a destroyed mesh's space isn't
  reused until the buffers accept its ticket, and is after; a full pool grows, keeping every mesh's offsets; stats follow each
  change; destroying the pool before a mesh fails a `CHECK` (a death test);
  meshes created and destroyed from several threads at once, with frames
  ending on another, end with every live mesh's data intact and the stats
  consistent.

### CL3 [ ] render: Drawing pools and ranges

Depends on: CL2.

- `DrawList::SetMeshPool()` and `Draw(const MeshDraw&)`, with `kMeshPool`, and
  `DrawCommand::Draw`'s `first_instance` and 32-bit `vertex_offset`.
- Vulkan backend: the wider `vertex_offset` and `first_instance` in its
  existing draws, and `kMeshPool` ignored (no pool can be created yet).

**Verify**
- Standard checks.
- Unit tests: the commands recorded for a pool and its draws; switching between
  a mesh and a pool; a material of another vertex type asserts; `Draw()` with a
  pool, and `Draw(const MeshDraw&)` with a mesh, assert.

### CL4 [ ] render_vulkan: Mesh pools in Vulkan

Depends on: CL3.

- `VulkanMeshPoolBuffers`: staging blocks, the upload queue, growth, reuse
  tickets, and its part of `EndFrame()`.
- Drawing `kMeshPool` and `MeshDraw` ranges, binding the index type.
- render-check: a row of shapes from one 32-bit pool, one of them drawn as two
  ranges (its halves in different colors, by `first_instance` into a storage
  buffer of colors), and a key that replaces one shape every frame (creating the
  new one, destroying the old one), so the pool's reuse and growth run.

**Verify**
- Standard checks.
- render-check, run with `tools\run-program.ps1`: the shapes draw correctly
  before and after replacing them for a few seconds, with no validation errors
  in the log.

### CL5 [ ] render: Indirect draws

Depends on: CL3.

- `DrawList::DrawIndirect()`, with `kIndirect`.
- Vulkan backend: `kIndirect` ignored.

**Verify**
- Standard checks.
- Unit tests: the commands recorded; a storage buffer of another type, or a
  range past its end, asserts; `DrawIndirect()` without a pool asserts.

### CL6 [ ] render_vulkan: Indirect draws in Vulkan

Depends on: CL4, CL5.

- `eIndirectBuffer` usage on storage buffers, the device features, the backend
  option to turn off multi-draw, and drawing `kIndirect`.
- render-check: the row of CL4's shapes drawn again in one indirect draw, from
  a storage buffer of `MeshDraw` values, each with its own color by
  `first_instance`, and a key that turns multi-draw off and on.

**Verify**
- Standard checks.
- render-check, run with `tools\run-program.ps1`: the indirect row matches the
  direct one, with multi-draw on and off, with no validation errors in the log.
- The device's features are logged at startup, confirming both features.

### CL7 [ ] block-world: Chunks in a pool

Depends on: CL6.

- Chunk meshes allocated from one 32-bit pool (one mesh per chunk, as the
  16-bit split is no longer needed), and every visible chunk drawn in one
  indirect draw, with chunk positions in a storage buffer by draw index.
- A key that switches between that and drawing each chunk on its own.

**Verify**
- Standard checks.
- block-world, run with `tools\run-program.ps1`: the world looks the same both
  ways.
- Performance: frame time (and draw calls) both ways in Release, at the default
  view distance, recorded here.
