// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MESH_POOL_H_
#define GB_RENDER_MESH_POOL_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/base/type_info.h"
#include "gb/container/queue.h"
#include "gb/container/range_packer.h"
#include "gb/render/mesh_pool_buffers.h"
#include "gb/render/render_types.h"
#include "gb/resource/resource.h"

namespace gb {

//==============================================================================
// MeshPoolStats
//==============================================================================

// How much of a mesh pool is used, from MeshPool::GetStats().
struct MeshPoolStats {
  // The space for vertices, or for triangles.
  struct Space {
    bool operator==(const Space&) const = default;

    int capacity = 0;

    // Held by meshes, including destroyed meshes whose space is `waiting`.
    int used = 0;

    // Held by destroyed meshes. Their space becomes free in the first
    // MeshPool::CreateMesh() call once no frame in flight may draw them.
    int waiting = 0;

    // The number of free ranges, and the size of the largest, which is the
    // most a mesh can take without the pool growing.
    int free_ranges = 0;
    int largest_free_range = 0;
  };

  int mesh_count = 0;  // Meshes not yet destroyed.
  Space vertices;
  Space triangles;
};

//==============================================================================
// PooledMesh
//==============================================================================

// A mesh in a MeshPool, which holds its place in the pool until it is
// destroyed.
//
// A pooled mesh can't be changed. To change one, create its replacement in the
// pool, and destroy the old one once it is no longer drawn.
//
// This class is thread-safe.
class PooledMesh final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  PooledMesh(const PooledMesh&) = delete;
  PooledMesh& operator=(const PooledMesh&) = delete;

  // Frees the mesh's place in the pool, for reuse once no frame that may draw
  // it is in flight. So a mesh may be destroyed as soon as it has been drawn
  // for the last time, even within the same frame.
  ~PooledMesh();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  MeshPool* GetPool() const { return pool_; }
  int GetVertexCount() const { return vertex_count_; }
  int GetTriangleCount() const { return triangle_count_; }

  // Returns the draw of all of the mesh's triangles, or of `triangle_count`
  // of them from `first_triangle`, with `draw_index` as its first instance
  // (which shaders read as gl_InstanceIndex).
  //
  // The draw holds offsets into the pool, so it draws whatever takes this
  // mesh's place once the mesh is destroyed.
  MeshDraw GetDraw(int draw_index = 0) const {
    return GetDraw(0, triangle_count_, draw_index);
  }
  MeshDraw GetDraw(int first_triangle, int triangle_count,
                   int draw_index = 0) const;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  PooledMesh(RenderInternal, MeshPool* pool, int vertex_offset,
             int vertex_count, int triangle_offset, int triangle_count);

 private:
  friend class MeshPool;

  MeshPool* const pool_;
  const int vertex_offset_;
  const int vertex_count_;
  const int triangle_offset_;
  const int triangle_count_;
};

//==============================================================================
// MeshPool
//==============================================================================

// A vertex buffer and an index buffer, for one vertex type and index type,
// which many meshes are created in.
//
// Each mesh's indices count from its own first vertex, as a Mesh's do. A pool
// grows as its meshes need, and never moves a mesh once it is created.
//
// The pool must outlive its meshes: destroying it first is a CHECK failure.
//
// This class is thread-safe. Creating and destroying meshes never waits for
// the GPU, and takes no lock that rendering needs.
class MeshPool final : public Resource {
 public:
  // The most vertices and triangles a pool holds. Its index count is an int,
  // so it holds at most a third as many triangles.
  static constexpr int kMaxVertices = std::numeric_limits<int>::max();
  static constexpr int kMaxTriangles = std::numeric_limits<int>::max() / 3;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const VertexType* GetVertexType() const { return vertex_type_; }
  IndexType GetIndexType() const { return index_type_; }

  MeshPoolStats GetStats() const;

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Creates a mesh in the pool from its vertices and indices (three for each
  // triangle), copying them to the GPU before the next frame that is ended
  // draws.
  //
  // This returns null if the vertices aren't of the pool's vertex type, the
  // indices aren't of its index type (Triangle is 16 bits), there are no
  // triangles, there are more than kMaxMeshVertices vertices in a 16-bit pool,
  // or the pool couldn't grow to hold the mesh.
  template <typename Vertex>
  std::unique_ptr<PooledMesh> CreateMesh(absl::Span<const Vertex> vertices,
                                         absl::Span<const uint16_t> indices);
  template <typename Vertex>
  std::unique_ptr<PooledMesh> CreateMesh(absl::Span<const Vertex> vertices,
                                         absl::Span<const uint32_t> indices);
  template <typename Vertex>
  std::unique_ptr<PooledMesh> CreateMesh(absl::Span<const Vertex> vertices,
                                         absl::Span<const Triangle> triangles);

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  MeshPool(RenderInternal, ResourceEntry entry, const VertexType* vertex_type,
           IndexType index_type, int vertex_capacity, int triangle_capacity,
           std::unique_ptr<MeshPoolBuffers> buffers);

  MeshPoolBuffers* GetBuffers(RenderInternal) const { return buffers_.get(); }

 private:
  friend class PooledMesh;

  // Space a destroyed mesh held, with the ticket it was freed with.
  struct WaitingMesh {
    uint64_t ticket;
    int vertex_offset;
    int vertex_count;
    int triangle_offset;
    int triangle_count;
  };

  struct MeshOffsets {
    int vertex_offset;
    int triangle_offset;
  };

  ~MeshPool() override;

  std::unique_ptr<PooledMesh> DoCreateMesh(
      TypeKey* vertex_type, const void* vertices, int vertex_count,
      IndexType index_type, const void* indices, int index_count);

  // Places a mesh's vertices and triangles, growing the pool if they don't
  // fit, and counts the mesh. Returns where it was placed, or nullopt if the
  // pool couldn't grow.
  std::optional<MeshOffsets> PlaceMesh(int vertex_count, int triangle_count)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Grows the pool so the counts fit, returning false if it couldn't.
  bool Grow(int vertex_count, int triangle_count)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Frees the space of destroyed meshes that the buffers can reuse.
  void FreeWaitingMeshes() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Called by ~PooledMesh().
  void ReleaseMesh(const PooledMesh& mesh) ABSL_LOCKS_EXCLUDED(mutex_);

  const VertexType* const vertex_type_;
  const IndexType index_type_;
  const std::unique_ptr<MeshPoolBuffers> buffers_;

  mutable absl::Mutex mutex_;
  RangePacker vertices_ ABSL_GUARDED_BY(mutex_);
  RangePacker triangles_ ABSL_GUARDED_BY(mutex_);

  // In the order they were destroyed, so also in ticket order, with the total
  // space they hold.
  Queue<WaitingMesh> waiting_meshes_ ABSL_GUARDED_BY(mutex_);
  int waiting_vertices_ ABSL_GUARDED_BY(mutex_) = 0;
  int waiting_triangles_ ABSL_GUARDED_BY(mutex_) = 0;
  int mesh_count_ ABSL_GUARDED_BY(mutex_) = 0;
};

template <typename Vertex>
std::unique_ptr<PooledMesh> MeshPool::CreateMesh(
    absl::Span<const Vertex> vertices, absl::Span<const uint16_t> indices) {
  return DoCreateMesh(TypeKey::Get<Vertex>(), vertices.data(),
                      static_cast<int>(vertices.size()), IndexType::kUint16,
                      indices.data(), static_cast<int>(indices.size()));
}

template <typename Vertex>
std::unique_ptr<PooledMesh> MeshPool::CreateMesh(
    absl::Span<const Vertex> vertices, absl::Span<const uint32_t> indices) {
  return DoCreateMesh(TypeKey::Get<Vertex>(), vertices.data(),
                      static_cast<int>(vertices.size()), IndexType::kUint32,
                      indices.data(), static_cast<int>(indices.size()));
}

template <typename Vertex>
std::unique_ptr<PooledMesh> MeshPool::CreateMesh(
    absl::Span<const Vertex> vertices, absl::Span<const Triangle> triangles) {
  return DoCreateMesh(TypeKey::Get<Vertex>(), vertices.data(),
                      static_cast<int>(vertices.size()), IndexType::kUint16,
                      triangles.data(), static_cast<int>(triangles.size()) * 3);
}

}  // namespace gb

#endif  // GB_RENDER_MESH_POOL_H_
