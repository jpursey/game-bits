// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mesh_view.h"

#include <cstring>

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
    std::memmove(vertex_view_->ModifyData(index),
                 vertex_view_->GetData(index + count),
                 remaining * vertex_view_->GetValueSize());
  }
  vertex_view_->Resize(index + remaining);
  return count;
}

int MeshView::DoSetTriangles(int index, const void* indices, int count) {
  const int size = GetTriangleCount();
  const int capacity = GetTriangleCapacity();
  RENDER_ASSERT(index >= 0 && index <= size);
  if (index + count > capacity) {
    count = capacity - index;
  }
  if (count == 0) {
    return 0;
  }
  if (index + count > size) {
    index_view_->Resize((index + count) * 3);
  }
  std::memcpy(index_view_->ModifyData(index * 3), indices,
              count * sizeof(Triangle));
  return count;
}

int MeshView::RemoveTriangles(int index, int count) {
  const int size = GetTriangleCount();
  RENDER_ASSERT(index >= 0 && index <= size && count >= 0);
  if (index + count > size) {
    count = size - index;
  }
  if (count == 0) {
    return 0;
  }
  const int remaining = size - (index + count);
  if (remaining > 0) {
    std::memmove(index_view_->ModifyData(index * 3),
                 index_view_->GetData((index + count) * 3),
                 remaining * sizeof(Triangle));
  }
  index_view_->Resize((index + remaining) * 3);
  return count;
}

}  // namespace gb
