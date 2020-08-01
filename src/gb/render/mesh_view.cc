// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mesh_view.h"

namespace gb {

int MeshView::DoSetVertices(int index, const void* vertices, int count) {
  const int size = vertex_view_->GetSize();
  const int capacity = vertex_view_->GetCapacity();
  RENDER_ASSERT(index >= 0 && index <= size);
  if (index + count > capacity) {
    count = capacity - index;
  }
  if (count == 0) {
    return 0;
  }
  if (index + count > size) {
    vertex_view_->Resize(index + count);
  }
  std::memcpy(vertex_view_->ModifyData(index), vertices,
              count * vertex_view_->GetValueSize());
  return count;
}

int MeshView::RemoveVertices(int index, int count) {
  const int size = vertex_view_->GetSize();
  RENDER_ASSERT(index >= 0 && index <= size && count >= 0);
  if (index + count > size) {
    count = size - index;
  }
  if (count == 0) {
    return 0;
  }
  const int remaining = size - (index + count);
  if (remaining > 0) {
    std::memcpy(vertex_view_->ModifyData(index),
                vertex_view_->GetData(index + count),
                remaining * vertex_view_->GetValueSize());
  }
  vertex_view_->Resize(index + remaining);
  return count;
}

int MeshView::DoSetIndices(int index, const void* indices, int count) {
  index *= 3;
  const int capacity = index_view_->GetCapacity();
  const int size = index_view_->GetSize();
  RENDER_ASSERT(index >= 0 && index <= size);
  if (index + count > capacity) {
    count = capacity - index;
  }
  if (count == 0) {
    return 0;
  }
  if (index + count > size) {
    index_view_->Resize(index + count);
  }
  std::memcpy(index_view_->ModifyData(index), indices, count * sizeof(Index));
  return count;
}

int MeshView::RemoveTriangles(int index, int count) {
  index *= 3;
  count *= 3;
  const int size = static_cast<int>(index_view_->GetSize());
  RENDER_ASSERT(index >= 0 && index <= size && count >= 0);
  if (index + count > size) {
    count = size - index;
  }
  if (count == 0) {
    return 0;
  }
  const int remaining = size - (index + count);
  if (remaining > 0) {
    std::memcpy(index_view_->ModifyData(index),
                index_view_->GetData(index + count), remaining * sizeof(Index));
  }
  index_view_->Resize(index + remaining);
  return count;
}

}  // namespace gb
