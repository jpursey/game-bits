# Shared mesh buffers and indirect draws

Every `Mesh` owns a vertex buffer and an index buffer (two GPU allocations),
its indices are 16 bits (`kMaxMeshVertices` is 65,535), and every draw of one
is a command of its own, with instance binding data of its own. A voxel
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

- `DrawCommand` gains `kMeshPool` (sets both buffers and the index type, from
  the pool's `MeshPoolBuffers`, as `kVertices` holds a mesh's `RenderBuffer`),
  and `kIndirect` (a storage buffer, first, and count). Its `Draw` gains
  `first_instance`, and its `vertex_offset` becomes 32 bits, as a pool's are
  large. This fills what was padding, so `DrawCommand` stays 24 bytes.
  `DrawPartial()`'s `first_vertex` is then bounded by the mesh's vertex
  capacity, rather than by 16 bits.
- `DrawList::SetMeshPool()` has the same overloads as `SetMesh()` (a pool, and
  optionally a material and instance data). It checks the material's vertex
  type against the pool's, as `SetMesh()` does for a mesh, and replaces any
  current mesh, as `SetMesh()` replaces any current pool. `Draw()` and
  `DrawPartial()` need a mesh, and the new `Draw(const MeshDraw&)` and
  `DrawIndirect()` need a pool. `Draw(const MeshDraw&)` asserts the draw has
  one instance, as `DrawCommand::Draw` has no instance count yet.
- `DrawIndirect()` asserts that the storage buffer holds `MeshDraw` values, and
  that `first + count` is within it. What is in the values is not checked: the
  GPU reads them.
- The test backend records the commands, as it does today, so tests check what
  was drawn.

### Vulkan

- **Uploads.** `VulkanMeshPoolBuffers` keeps device-local vertex and index
  buffers, and host-visible staging blocks (4 MiB, or bigger for a bigger
  write). A write takes space in the current block (a short lock, which only
  writers take), copies the data in, and pushes the copy onto a lock-free
  queue (`moodycamel::ConcurrentQueue`, already vendored). The backend keeps a
  set of every pool's buffers, and at the start of `EndFrame()` each one
  drains its queue, whether or not the frame draws it, so a write is never
  more than a frame late. The copies join the backend's other buffer updates,
  which are now recorded with one `copyBuffer()` for each run of copies
  between the same two buffers, and one memory barrier before the render
  pass. Each copy holds its block, so the last copy recorded from a full block
  passes it to the garbage collector.
- **Growth.** `Grow()` creates the bigger buffers at once (so it can fail),
  and the next `EndFrame()` copies the old contents into them on the GPU,
  between barriers, before that frame's copies, and passes the old buffers to
  the garbage collector. The queue is drained before the grown buffers are
  taken, so any growth a drained write needs is applied with it, as it
  happened before the write was queued. A second `Grow()` before the frame
  ends replaces the first's unused buffers.
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
  enabled where the device has it (`VulkanBackend::IsMultiDrawSupported()`),
  and `SetMultiDrawEnabled()` turns it off and on from the next frame, from
  any thread, so the fallback can be checked on a GPU that has it. The device's
  name, `multiDrawIndirect`, and `maxDrawIndirectCount` are logged at startup.

### Left out

- **`Mesh` on pools.** `Mesh` keeps its own buffers. Moving it onto pools would
  change its editing (`MeshView`) and volatility, for no gain to its callers.
- **Instance counts above one** come with the backlog's *Instanced draws*.
- **Compaction** and **draw counts from the GPU** are in the backlog: *Mesh pool
  compaction* and *Indirect draw counts from the GPU*.

## Structure

- `gb/container/range_packer.h` / `.cc`: `RangePacker`.
- `gb/render/render_types.h`: `IndexType` and `MeshDraw`.
- `gb/render/mesh_pool.h` / `.cc`: `MeshPool`, `PooledMesh`, and
  `MeshPoolStats`, created by `RenderSystem::CreateMeshPool()`.
- `gb/render/mesh_pool_buffers.h`: `MeshPoolBuffers`, the backend's half, from
  `RenderBackend::CreateMeshPoolBuffers()`.
- `gb/render/draw_list.h` / `.cc`: `SetMeshPool()`, `Draw(const MeshDraw&)`, and
  `DrawIndirect()`, recorded as `DrawCommand`'s `kMeshPool` and `kIndirect`.
- `gb/render/test_mesh_pool_buffers.h` / `.cc`: the test backend's buffers,
  whose reuse the tests control by frame.
- `gb/render/vulkan/vulkan_mesh_pool_buffers.h` / `.cc`: uploads, growth, and
  reuse in Vulkan; `vulkan_backend.cc` draws pools and indirect draws, and
  `vulkan_render_buffer.cc` gives storage buffers `eIndirectBuffer` usage.

Reusable pieces: `RangePacker` for any space handed out in ranges (offsets
only, never memory), mesh pools for any program drawing many meshes of one
vertex type, and a per-frame storage buffer of `MeshDraw` values with
per-draw data by `first_instance`, as block-world uses, for drawing them all at
once.

## Checks

- `gb_container_test`: `RangePacker`'s best fit (the smallest gap that fits,
  and the lowest offset of equal gaps), merging a removed range with the gaps
  on either side, filling the capacity then failing, growth, the counts and
  largest gap after each change, and a randomized run against a flag per
  offset.
- `gb_render_test`:
  - Mesh pools: data reaching the test buffers when the frame ends, `GetDraw()`
    offsets for whole meshes and ranges, the wrong vertex or index type,
    `kMaxMeshVertices` in 16-bit and 32-bit pools, reuse only once the buffers
    accept a ticket, growth keeping every mesh's offsets, stats, destroying
    the pool first (a death test), and meshes created and destroyed on several
    threads while frames end on another.
  - Draw lists: the commands recorded for pools, ranges and indirect draws;
    switching between a mesh and a pool; and the asserts for a material of
    another vertex type, a mesh draw with a pool and a range with a mesh, an
    indirect draw without a pool or instance data, a storage buffer of
    another type, and a range past its end.
- `examples/render-check`, `scripts/mesh_pools.txt`: a row of shapes from one
  32-bit pool, one drawn as two ranges in two colors by `first_instance`, and
  below it the same shapes in one indirect draw, while a shape is replaced
  every frame (so the pool's reuse and growth run), with multi-draw on and off.
  The rows match, and the validation layer logs no errors.
- `examples/block-world`, `scripts/chunk_pool.txt`: every chunk in view drawn
  from one 32-bit pool in one indirect draw, with each chunk's position in a
  per-frame storage buffer by draw index, and drawn each on its own the old way
  (I switches between them). Both draw the world pixel for pixel the same, at
  the default and the farthest view distance, and the validation layer logs no
  errors, including when the draw buffers grow.

## Results

The device used (an NVIDIA GeForce RTX 4070 Ti SUPER) has
`drawIndirectFirstInstance` and `multiDrawIndirect`, with a
`maxDrawIndirectCount` of 2^32 - 1. `gl_InstanceIndex` includes
`first_instance` in indirect draws, with multi-draw and without.

In block-world (Release), the real time between frames is the display's 10 ms
(100 Hz, FIFO present) either way, so the CPU time to draw a frame (from after
`BeginFrame()` to after `EndFrame()`, shown in its "Draw Time" window) is what
differs. Each is the median of 60 frames, over three runs:

| View distance | Chunks | Draw calls | From the pool | Each on its own |
|---|---|---|---|---|
| 640 (default) | 1,272 | 1 / 1,272 | 0.29-0.35 ms | 1.08-1.09 ms |
| 1200 (farthest) | 4,315 | 1 / 4,315 | 0.72-0.81 ms | 3.67-4.21 ms |

Drawing from the pool takes under a third of the CPU time at the default view
distance, and about a fifth at the farthest. Drawing each chunk on its own
keeps block-world's old meshes (a vertex and index buffer, and instance data,
for each chunk), so the comparison is with what the pool replaces.
