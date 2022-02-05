// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MESH_H_
#define GB_RENDER_MESH_H_

#include <algorithm>
#include <memory>

#include "absl/types/span.h"
#include "gb/render/binding_data.h"
#include "gb/render/material.h"
#include "gb/render/mesh_view.h"
#include "gb/render/render_assert.h"
#include "gb/render/render_buffer.h"
#include "gb/render/render_buffer_view.h"
#include "gb/render/render_types.h"
#include "gb/resource/resource.h"
#include "glog/logging.h"

namespace gb {

// A mesh defines a set of triangles for a specified vertex type.
//
// Mesh is defined by a list of vertices and a list of indices into the vertex
// list. Each triple of indices specifies a triangle in the mesh. The data
// associated with a vertex is defined by the vertex type.
//
// A mesh may be used with any material that uses the same vertex type.
//
// This class is thread-compatible.
class Mesh final : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properies
  //----------------------------------------------------------------------------

  // Returns the layout type for vertices in this mesh.
  const VertexType* GetVertexType() const { return vertex_type_; }

  // Returns the data volatility for the vertex and index data.
  //
  // Note: kStaticWrite mesh cannot be edited interactively, and only may be
  // replaced with entirely new data.
  DataVolatility GetVolatility() const { return volatility_; }

  // Current counts and limits of the mesh data.
  //
  // Counts may change when replacing the mesh via Set or when editing a mesh.
  // Capacity is set at mesh creation, and can only be modified after the fact
  // by replacing all the data in the mesh via Set.
  int GetVertexCount() const;
  int GetVertexCapacity() const;
  int GetTriangleCount() const;
  int GetTriangleCapacity() const;

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Sets vertices of the specified according to the vertex layout expected by
  // the material.
  //
  // If the number of vertices or indices or the associated minimum capacities
  // exceeeds the current capacities of the mesh, they will be reallocated to
  // accomodate the new sizes.
  //
  // This returns false if the new mesh data could not be set, the vertex is the
  // wrong type, or a MeshView is active. Under some circumscances, this may be
  // an unrecoverable failure, in which case the vertex and triangle counts are
  // reset to zero.
  template <typename Vertex>
  bool Set(absl::Span<const Vertex> vertices,
           absl::Span<const uint16_t> indices, int min_vertex_capacity = 0,
           int min_triangle_capacity = 0);
  template <typename Vertex>
  bool Set(absl::Span<const Vertex> vertices,
           absl::Span<const Triangle> triangles, int min_vertex_capacity = 0,
           int min_triangle_capacity = 0);

  // Returns an editable view onto the mesh.
  //
  // This may be called for kPerFrame or kStaticReadWrite volatility mesh only.
  // Any changes to a view are propagated to the mesh when the MeshView is
  // destructed, and will be visible the next time RenderSystem::EndFrame is
  // called.
  //
  // Only one MeshView may be active at any given time. If Edit is called again
  // before the a previous MeshView is destructed, the resulting MeshView will
  // be null. The MeshView will also be null if the mesh volatility is
  // kStaticWrite.
  std::unique_ptr<MeshView> Edit();

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  Mesh(RenderInternal, ResourceEntry entry, RenderBackend* backend,
       const VertexType* vertex_type, DataVolatility volatility,
       std::unique_ptr<RenderBuffer> vertex_buffer,
       std::unique_ptr<RenderBuffer> index_buffer);

  RenderBuffer* GetVertexBuffer(RenderInternal) const {
    return vertex_buffer_.get();
  }
  RenderBuffer* GetIndexBuffer(RenderInternal) const {
    return index_buffer_.get();
  }

 private:
  ~Mesh() override;

  bool DoSet(const void* vertex_data, int vertex_count, int vertex_capacity,
             const void* index_data, int index_count, int index_capacity);
  bool DoEdit(std::unique_ptr<RenderBuffer>, std::unique_ptr<RenderBuffer>);

  RenderBackend* const backend_;
  const VertexType* const vertex_type_;
  const DataVolatility volatility_;
  std::unique_ptr<RenderBuffer> vertex_buffer_;
  std::unique_ptr<RenderBuffer> index_buffer_;
};

template <typename Vertex>
bool Mesh::Set(absl::Span<const Vertex> vertices,
               absl::Span<const uint16_t> indices, int min_vertex_capacity,
               int min_triangle_capacity) {
  RENDER_ASSERT(indices.size() % 3 == 0);
  if (TypeKey::Get<Vertex>() != vertex_type_->GetType()) {
    LOG(ERROR) << "Invalid vertex type for mesh";
    return false;
  }
  const int vertex_count = static_cast<int>(vertices.size());
  const int index_count = static_cast<int>(indices.size());
  const int vertex_capacity = std::max({vertex_count, min_vertex_capacity, 3});
  const int index_capacity =
      std::max({index_count, min_triangle_capacity * 3, 3});
  return DoSet(vertices.data(), vertex_count, vertex_capacity, indices.data(),
               index_count, index_capacity);
}

template <typename Vertex>
bool Mesh::Set(absl::Span<const Vertex> vertices,
               absl::Span<const Triangle> triangles, int min_vertex_capacity,
               int min_triangle_capacity) {
  if (TypeKey::Get<Vertex>() != vertex_type_->GetType()) {
    LOG(ERROR) << "Invalid vertex type for mesh";
    return false;
  }
  const int vertex_count = static_cast<int>(vertices.size());
  const int index_count = static_cast<int>(triangles.size()) * 3;
  const int vertex_capacity = std::max({vertex_count, min_vertex_capacity, 3});
  const int index_capacity =
      std::max({index_count, min_triangle_capacity * 3, 3});
  return DoSet(vertices.data(), vertex_count, vertex_capacity,
               reinterpret_cast<const uint16_t*>(triangles.data()), index_count,
               index_capacity);
}

}  // namespace gb

#endif  // GB_RENDER_MESH_H_
