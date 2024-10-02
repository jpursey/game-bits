// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mesh.h"

#include "absl/log/log.h"
#include "gb/render/render_backend.h"

namespace gb {

Mesh::Mesh(RenderInternal, ResourceEntry entry, RenderBackend* backend,
           const VertexType* vertex_type, DataVolatility volatility,
           std::unique_ptr<RenderBuffer> vertex_buffer,
           std::unique_ptr<RenderBuffer> index_buffer)
    : Resource(std::move(entry)),
      backend_(backend),
      vertex_type_(vertex_type),
      volatility_(volatility),
      vertex_buffer_(std::move(vertex_buffer)),
      index_buffer_(std::move(index_buffer)) {}

Mesh::~Mesh() {}

int Mesh::GetVertexCount() const { return vertex_buffer_->GetSize(); }

int Mesh::GetVertexCapacity() const { return vertex_buffer_->GetCapacity(); }

int Mesh::GetTriangleCount() const { return index_buffer_->GetSize() / 3; }

int Mesh::GetTriangleCapacity() const {
  return index_buffer_->GetCapacity() / 3;
}

bool Mesh::DoSet(const void* vertex_data, int vertex_count, int vertex_capacity,
                 const void* index_data, int index_count, int index_capacity) {
  if (vertex_buffer_->IsEditing() || index_buffer_->IsEditing()) {
    LOG(ERROR) << "Failed to write new mesh, as a MeshView is currently active";
    return false;
  }

  std::unique_ptr<RenderBuffer> new_vertex_buffer;
  std::unique_ptr<RenderBuffer> new_index_buffer;
  if (vertex_buffer_->GetCapacity() < vertex_capacity) {
    new_vertex_buffer = backend_->CreateVertexBuffer(
        {}, volatility_, vertex_buffer_->GetValueSize(), vertex_capacity);
    if (new_vertex_buffer == nullptr) {
      LOG(ERROR) << "Failed to create new vertex buffer for mesh";
      return false;
    }
  }
  if (index_buffer_->GetCapacity() < index_capacity) {
    new_index_buffer =
        backend_->CreateIndexBuffer({}, volatility_, index_capacity);
    if (new_index_buffer == nullptr) {
      LOG(ERROR) << "Failed to create new index buffer for mesh";
      return false;
    }
  }
  if (new_vertex_buffer != nullptr) {
    vertex_buffer_ = std::move(new_vertex_buffer);
  }
  if (new_index_buffer != nullptr) {
    index_buffer_ = std::move(new_index_buffer);
  }

  if (!vertex_buffer_->Set(vertex_data, vertex_count) ||
      !index_buffer_->Set(index_data, index_count)) {
    LOG(ERROR) << "Failed to update vertex or index buffer, resetting both "
                  "buffers to zero";
    vertex_buffer_->Resize(0);
    index_buffer_->Resize(0);
    return false;
  }

  return true;
}

std::unique_ptr<MeshView> Mesh::Edit() {
  if (vertex_buffer_->IsEditing() || index_buffer_->IsEditing()) {
    LOG(ERROR)
        << "MeshView cannot be created as an existing MeshView is still active";
    return nullptr;
  }

  auto vertex_view = vertex_buffer_->Edit();
  auto index_view = index_buffer_->Edit();
  if (vertex_view == nullptr || index_view == nullptr) {
    LOG(ERROR) << "Failed to create MeshView for mesh";
    return nullptr;
  }

  return std::make_unique<MeshView>(RenderInternal{}, vertex_type_->GetType(),
                                    std::move(vertex_view),
                                    std::move(index_view));
}

}  // namespace gb
