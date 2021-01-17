// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MESH_VIEW_H_
#define GB_RENDER_MESH_VIEW_H_

#include "absl/types/span.h"
#include "gb/base/type_info.h"
#include "gb/render/render_buffer_view.h"
#include "gb/render/render_types.h"

namespace gb {

// A mesh view provides an editable window onto a mesh.
//
// Only one mesh view may be active on a mesh at a time. While a mesh view is
// active, it can be edited freely although the vertex and index/triangle
// capacity is fixed. As the edits are not applied to the underlying mesh on the
// GPU until the mesh view is deleted, the resulting mesh only needs to be valid
// when the mesh view is deleted.
//
// A mesh view can also be used in a read-only fashion. If no Set, Remove, or
// Modify functions are called, then this will not incur any update overhead for
// the mesh.
//
// This class is thread-compatible.
class MeshView final {
 public:
  using Index = uint16_t;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  MeshView(const MeshView& other) = delete;
  MeshView(MeshView&& other) = default;
  MeshView& operator=(const MeshView& other) = delete;
  MeshView& operator=(MeshView&& other) = default;
  ~MeshView() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the current count and capacity of vertices in the mesh.
  //
  // The capacity is fixed, but the count may vary when calling
  // Set/RemoveVertices.
  int GetVertexCount() const;
  int GetVertexCapacity() const;

  // Returns the current count and capacity of triangles in the mesh.
  //
  // The capacity is fixed, but the count may vary when calling
  // Set/RemoveVertices.
  int GetTriangleCount() const;
  int GetTriangleCapacity() const;

  // Returns true if a modifying function was called on the buffer view.
  bool IsModified() const;

  //----------------------------------------------------------------------------
  // Vertex access
  //----------------------------------------------------------------------------

  // Returns a read-only reference to the specified vertex.
  //
  // It is undefined behavior to request an index < 0 or > GetVertexCount();
  template <typename Vertex>
  const Vertex& GetVertex(int index) const;

  // Returns a writable reference to the specified vertex.
  //
  // Calling this function will result in the mesh getting re-uploaded to the
  // GPU, regardless of whether the vertex is actually changed or not. Prefer
  // calling GetVertex, if the vertex will not actually be modified.
  //
  // It is undefined behavior to request an index < 0 or > GetVertexCount();
  template <typename Vertex>
  Vertex& ModifyVertex(int index);

  // Replaces a set of vertices starting at the specified index.
  //
  // Index must less than or equal to the vertex count, however if index +
  // vertices.size() is greater than the current vertex count, then the count
  // will increase to accomodate the new vertices up to the limit of the vertex
  // capacity of the mesh. If the new count would exceed the mesh capacity, only
  // the vertices up to the capacity will be set.
  //
  // This returns the number of vertices actually copied. This may be less than
  // vertices.size() if the capacity is too small.
  template <typename Vertex>
  int SetVertices(int index, absl::Span<const Vertex> vertices);

  // Adds vertices to the end.
  //
  // Returns the number of vertices actually copied. This may be less than
  // vertices.size() if the capacity is too small.
  template <typename Vertex>
  int AddVertices(absl::Span<const Vertex> vertices);

  // Removes a range of vertices, shifting all later vertices back by 'count'.
  //
  // Index must less than or equal to the vertex count, however if index + count
  // is greater that the current vertex count, then all vertices after index
  // will be removed.
  //
  // This returns the number of vertices actually removed.
  int RemoveVertices(int index, int count);

  //----------------------------------------------------------------------------
  // Triangle access
  //----------------------------------------------------------------------------

  // Returns a read-only reference to the specified triangle.
  //
  // It is undefined behavior to request an index < 0 or > GetTriangleCount();
  const Triangle& GetTriangle(int index) const;

  // Returns a writable reference to the specified triangle.
  //
  // Calling this function will result in the mesh getting re-uploaded to the
  // GPU, regardless of whether the triangle is actually changed or not. Prefer
  // calling GetTriangle, if the triangle will not actually be modified.
  //
  // It is undefined behavior to request an index < 0 or > GetTriangleCount();
  Triangle& ModifyTriangle(int index);

  // Replaces a set of triangles starting at the specified index.
  //
  // Index must less than or equal to the triangle count, however if index +
  // triangles.size() is greater than the current triangle count, then the count
  // will increase to accomodate the new triangles up to the limit of the
  // triangle capacity of the mesh. If the new count would exceed the mesh
  // capacity, only the triangles up to the capacity will be set.
  //
  // This returns the number of triangles actually copied. This may be less than
  // triangles.size() if the capacity is too small.
  int SetTriangles(int index, absl::Span<const Triangle> triangles);

  // Replaces a set of triangles starting at the specified triangle index.
  //
  // Index must less than or equal to the triangle count, however If index +
  // indices.size() / 3 is greater than the current triangle count, then the
  // count will increase to accomodate the new triangles up to the limit of the
  // triangle capacity of the mesh. If the new count would exceed the mesh
  // capacity, only the triangles up to the capacity will be set.
  //
  // This returns the number of triangles actually copied. This may be less than
  // indices.size() / 3 if the capacity is too small.
  int SetTriangles(int index, absl::Span<const Index> indices);

  // Adds triangles to the end.
  //
  // Returns the number of triangles actually copied. This may be less than
  // triangles.size() if the capacity is too small.
  int AddTriangles(absl::Span<const Triangle> triangles);

  // Adds triangles to the end.
  //
  // Returns the number of triangles actually copied. This may be less than
  // indices.size() / 3 if the capacity is too small.
  int AddTriangles(absl::Span<const Index> indices);

  // Removes a range of triangles, shifting all later triangles back by 'count'.
  //
  // Index must less than or equal to the triangle count, however if index +
  // count is greater that the current triangle count, then all triangles after
  // index will be removed.
  //
  // This returns the number of triangles actually removed.
  int RemoveTriangles(int index, int count);

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  MeshView(RenderInternal, TypeKey* vertex_type,
           std::unique_ptr<RenderBufferView> vertex_view,
           std::unique_ptr<RenderBufferView> index_view)
      : vertex_type_(vertex_type),
        vertex_view_(std::move(vertex_view)),
        index_view_(std::move(index_view)) {}

  TypeKey* GetVertexType(RenderInternal) const { return vertex_type_; }
  const void* GetVertexData(RenderInternal) const;
  const uint16_t* GetIndexData(RenderInternal) const;

 private:
  int DoSetVertices(int index, const void* vertices, int count);
  int DoSetIndices(int index, const void* indices, int count);

  TypeKey* vertex_type_;
  std::unique_ptr<RenderBufferView> vertex_view_;
  std::unique_ptr<RenderBufferView> index_view_;
};

inline int MeshView::GetVertexCount() const { return vertex_view_->GetSize(); }

inline int MeshView::GetVertexCapacity() const {
  return vertex_view_->GetCapacity();
}

inline int MeshView::GetTriangleCount() const {
  return index_view_->GetSize() / 3;
}

inline int MeshView::GetTriangleCapacity() const {
  return index_view_->GetCapacity() / 3;
}

inline bool MeshView::IsModified() const {
  return vertex_view_->IsModified() || index_view_->IsModified();
}

inline const void* MeshView::GetVertexData(RenderInternal) const {
  return vertex_view_->GetData(0);
}

template <typename Vertex>
inline const Vertex& MeshView::GetVertex(int index) const {
  RENDER_ASSERT(TypeKey::Get<Vertex>() == vertex_type_);
  RENDER_ASSERT(index >= 0 && index < vertex_view_->GetSize());
  return *static_cast<const Vertex*>(vertex_view_->GetData(index));
}

template <typename Vertex>
inline Vertex& MeshView::ModifyVertex(int index) {
  RENDER_ASSERT(TypeKey::Get<Vertex>() == vertex_type_);
  RENDER_ASSERT(index >= 0 && index < vertex_view_->GetSize());
  return *static_cast<Vertex*>(vertex_view_->ModifyData(index));
}

template <typename Vertex>
inline int MeshView::SetVertices(int index, absl::Span<const Vertex> vertices) {
  RENDER_ASSERT(TypeKey::Get<Vertex>() == vertex_type_);
  return DoSetVertices(index, vertices.data(),
                       static_cast<int>(vertices.size()));
}

template <typename Vertex>
inline int MeshView::AddVertices(absl::Span<const Vertex> vertices) {
  RENDER_ASSERT(TypeKey::Get<Vertex>() == vertex_type_);
  return DoSetVertices(GetVertexCount(), vertices.data(),
                       static_cast<int>(vertices.size()));
}

inline const uint16_t* MeshView::GetIndexData(RenderInternal) const {
  return static_cast<const uint16_t*>(index_view_->GetData(0));
}

inline const Triangle& MeshView::GetTriangle(int index) const {
  index *= 3;
  RENDER_ASSERT(index >= 0 && index + 3 <= index_view_->GetSize());
  return *static_cast<const Triangle*>(index_view_->GetData(index));
}

inline Triangle& MeshView::ModifyTriangle(int index) {
  index *= 3;
  RENDER_ASSERT(index >= 0 && index + 3 <= index_view_->GetSize());
  return *static_cast<Triangle*>(index_view_->ModifyData(index));
}

inline int MeshView::SetTriangles(int index,
                                  absl::Span<const Triangle> triangles) {
  return DoSetIndices(index, triangles.data(),
                      static_cast<int>(triangles.size() * 3));
}

inline int MeshView::SetTriangles(int index, absl::Span<const Index> indices) {
  RENDER_ASSERT(indices.size() % 3 == 0);
  return DoSetIndices(index, indices.data(), static_cast<int>(indices.size()));
}

inline int MeshView::AddTriangles(absl::Span<const Triangle> triangles) {
  return DoSetIndices(GetTriangleCount(), triangles.data(),
                      static_cast<int>(triangles.size() * 3));
}

inline int MeshView::AddTriangles(absl::Span<const Index> indices) {
  return DoSetIndices(GetTriangleCount(), indices.data(),
                      static_cast<int>(indices.size()));
}

}  // namespace gb

#endif  // GB_RENDER_MESH_VIEW_H_
