// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mesh_pool.h"

#include <algorithm>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "gb/render/render_assert.h"

namespace gb {

namespace {

// Returns the capacity `packer` needs to place `count`: its own if it has room
// already, and otherwise at least double, or zero if that would be more than
// `max_capacity`.
int GetGrownCapacity(const RangePacker& packer, int count, int max_capacity) {
  const int capacity = packer.GetCapacity();
  if (packer.GetLargestGap() >= count) {
    return capacity;
  }
  if (count > max_capacity - capacity) {
    return 0;
  }

  // The new space is one gap of at least `count` at the end.
  const int64_t grown =
      std::max(int64_t{capacity} * 2, int64_t{capacity} + count);
  return static_cast<int>(std::min<int64_t>(grown, max_capacity));
}

MeshPoolStats::Space GetSpace(const RangePacker& packer) {
  MeshPoolStats::Space space;
  space.capacity = packer.GetCapacity();
  space.used = packer.GetUsed();
  space.free_ranges = packer.GetGapCount();
  space.largest_free_range = packer.GetLargestGap();
  return space;
}

}  // namespace

//==============================================================================
// PooledMesh
//==============================================================================

PooledMesh::PooledMesh(RenderInternal, MeshPool* pool, int vertex_offset,
                       int vertex_count, int triangle_offset,
                       int triangle_count)
    : pool_(pool),
      vertex_offset_(vertex_offset),
      vertex_count_(vertex_count),
      triangle_offset_(triangle_offset),
      triangle_count_(triangle_count) {}

PooledMesh::~PooledMesh() { pool_->ReleaseMesh(*this); }

MeshDraw PooledMesh::GetDraw(int first_triangle, int triangle_count,
                             int draw_index) const {
  RENDER_ASSERT(first_triangle >= 0 && triangle_count >= 0 &&
                first_triangle + triangle_count <= triangle_count_);
  RENDER_ASSERT(draw_index >= 0);
  return {
      .index_count = static_cast<uint32_t>(triangle_count * 3),
      .instance_count = 1,
      .first_index =
          static_cast<uint32_t>((triangle_offset_ + first_triangle) * 3),
      .vertex_offset = vertex_offset_,
      .first_instance = static_cast<uint32_t>(draw_index),
  };
}

//==============================================================================
// MeshPool
//==============================================================================

MeshPool::MeshPool(RenderInternal, ResourceEntry entry,
                   const VertexType* vertex_type, IndexType index_type,
                   int vertex_capacity, int triangle_capacity,
                   std::unique_ptr<MeshPoolBuffers> buffers)
    : Resource(std::move(entry)),
      vertex_type_(vertex_type),
      index_type_(index_type),
      buffers_(std::move(buffers)),
      vertices_(vertex_capacity),
      triangles_(triangle_capacity),
      waiting_meshes_(256, 256) {}

MeshPool::~MeshPool() {
  absl::MutexLock lock(&mutex_);
  CHECK_EQ(mesh_count_, 0) << "Mesh pool deleted while pooled meshes remain";
}

MeshPoolStats MeshPool::GetStats() const {
  absl::MutexLock lock(&mutex_);
  MeshPoolStats stats;
  stats.mesh_count = mesh_count_;
  stats.vertices = GetSpace(vertices_);
  stats.triangles = GetSpace(triangles_);
  stats.vertices.waiting = waiting_vertices_;
  stats.triangles.waiting = waiting_triangles_;
  return stats;
}

std::unique_ptr<PooledMesh> MeshPool::DoCreateMesh(
    TypeKey* vertex_type, const void* vertices, int vertex_count,
    IndexType index_type, const void* indices, int index_count) {
  if (vertex_type != vertex_type_->GetType()) {
    LOG(ERROR) << "Invalid vertex type for mesh pool";
    return nullptr;
  }
  if (index_type != index_type_) {
    LOG(ERROR) << "Invalid index type for mesh pool, whose indices are "
               << GetIndexSize(index_type_) * 8 << " bits";
    return nullptr;
  }
  if (vertex_count < 1 || index_count < 3 || index_count % 3 != 0) {
    LOG(ERROR) << "Invalid mesh for mesh pool, with " << vertex_count
               << " vertices and " << index_count
               << " indices, as it needs at least one whole triangle";
    return nullptr;
  }
  if (index_type_ == IndexType::kUint16 && vertex_count > kMaxMeshVertices) {
    LOG(ERROR) << "Invalid mesh for mesh pool, as " << vertex_count
               << " vertices is more than the maximum of " << kMaxMeshVertices
               << " with 16-bit indices";
    return nullptr;
  }

  const int triangle_count = index_count / 3;
  const std::optional<MeshOffsets> offsets =
      PlaceMesh(vertex_count, triangle_count);
  if (!offsets.has_value()) {
    return nullptr;
  }
  auto mesh = std::make_unique<PooledMesh>(
      RenderInternal{}, this, offsets->vertex_offset, vertex_count,
      offsets->triangle_offset, triangle_count);

  // The mesh's space is its own, so it is written outside the lock. If the
  // write fails, destroying the mesh releases its space.
  if (!buffers_->WriteMesh(offsets->vertex_offset, vertices, vertex_count,
                           offsets->triangle_offset * 3, indices,
                           index_count)) {
    LOG(ERROR) << "Failed to write mesh to mesh pool";
    return nullptr;
  }
  return mesh;
}

std::optional<MeshPool::MeshOffsets> MeshPool::PlaceMesh(int vertex_count,
                                                         int triangle_count) {
  absl::MutexLock lock(&mutex_);
  FreeWaitingMeshes();
  if (vertices_.GetLargestGap() < vertex_count ||
      triangles_.GetLargestGap() < triangle_count) {
    if (!Grow(vertex_count, triangle_count)) {
      return std::nullopt;
    }
  }
  ++mesh_count_;
  return MeshOffsets{*vertices_.Place(vertex_count),
                     *triangles_.Place(triangle_count)};
}

bool MeshPool::Grow(int vertex_count, int triangle_count) {
  const int vertex_capacity =
      GetGrownCapacity(vertices_, vertex_count, kMaxVertices);
  const int triangle_capacity =
      GetGrownCapacity(triangles_, triangle_count, kMaxTriangles);
  if (vertex_capacity == 0 || triangle_capacity == 0) {
    LOG(ERROR) << "Mesh pool can't grow to hold " << vertex_count
               << " more vertices and " << triangle_count << " more triangles";
    return false;
  }
  if (!buffers_->Grow(vertex_capacity, triangle_capacity * 3)) {
    LOG(ERROR) << "Failed to grow mesh pool to " << vertex_capacity
               << " vertices and " << triangle_capacity << " triangles";
    return false;
  }
  if (vertex_capacity > vertices_.GetCapacity()) {
    vertices_.Grow(vertex_capacity);
  }
  if (triangle_capacity > triangles_.GetCapacity()) {
    triangles_.Grow(triangle_capacity);
  }
  return true;
}

void MeshPool::FreeWaitingMeshes() {
  while (!waiting_meshes_.empty()) {
    const WaitingMesh& mesh = waiting_meshes_.front();
    if (!buffers_->CanReuse(mesh.ticket)) {
      break;
    }
    vertices_.Remove(mesh.vertex_offset, mesh.vertex_count);
    triangles_.Remove(mesh.triangle_offset, mesh.triangle_count);
    waiting_vertices_ -= mesh.vertex_count;
    waiting_triangles_ -= mesh.triangle_count;
    waiting_meshes_.pop();
  }
}

void MeshPool::ReleaseMesh(const PooledMesh& mesh) {
  absl::MutexLock lock(&mutex_);
  waiting_meshes_.push({buffers_->GetReuseTicket(), mesh.vertex_offset_,
                        mesh.vertex_count_, mesh.triangle_offset_,
                        mesh.triangle_count_});
  waiting_vertices_ += mesh.vertex_count_;
  waiting_triangles_ += mesh.triangle_count_;
  --mesh_count_;
}

}  // namespace gb
