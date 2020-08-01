// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_buffer.h"

#include "gb/render/render_buffer_view.h"

namespace gb {

bool RenderBuffer::Clear(int size) {
  RENDER_ASSERT(size >= 0 && size <= capacity_);
  if (IsEditing()) {
    return false;
  }
  if (size > 0 && !DoClear(0, size)) {
    return false;
  }
  size_ = size;
  return true;
}

bool RenderBuffer::Resize(int size) {
  RENDER_ASSERT(size >= 0 && size <= capacity_);
  if (IsEditing()) {
    return false;
  }
  if (size > size_ && !DoClear(size_, size - size_)) {
    return false;
  }
  size_ = size;
  return true;
}

bool RenderBuffer::Set(const void* data, int size) {
  RENDER_ASSERT(size >= 0 && size <= capacity_);
  RENDER_ASSERT(size == 0 || data != nullptr);
  if (IsEditing()) {
    return false;
  }
  if (size > 0 && !DoSet(data, size)) {
    return false;
  }
  size_ = size;
  return true;
}

std::unique_ptr<RenderBufferView> RenderBuffer::Edit() {
  if (IsEditing() || volatility_ == DataVolatility::kStaticWrite) {
    return nullptr;
  }
  void* data = DoEditBegin();
  if (data == nullptr) {
    return nullptr;
  }
  auto view = std::make_unique<RenderBufferView>(RenderInternal{}, this, data);
  view_ = view.get();
  return view;
}

}  // namespace gb
