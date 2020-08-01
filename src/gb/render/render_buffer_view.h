// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_BUFFER_VIEW_H_
#define GB_RENDER_RENDER_BUFFER_VIEW_H_

#include "gb/render/render_buffer.h"

namespace gb {

// This class implements an editable view on a render buffer.
//
// This is an internal class called by other render classes to access the
// underlying graphics API and GPU.
//
// This class is thread-compatible.
class RenderBufferView final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  RenderBufferView(RenderInternal, RenderBuffer* buffer, void* data)
      : buffer_(buffer), data_(data), size_(buffer->size_) {}
  RenderBufferView(const RenderBufferView&) = delete;
  RenderBufferView(RenderBufferView&&) = delete;
  RenderBufferView& operator=(const RenderBufferView&) = delete;
  RenderBufferView& operator=(RenderBufferView&&) = delete;
  ~RenderBufferView() {
    buffer_->size_ = size_;
    buffer_->OnEditEnd(modified_);
    buffer_->view_ = nullptr;
  }

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Properties of the underlying buffer.
  int GetCapacity() const { return buffer_->capacity_; }
  int GetSize() const { return size_; }
  int GetValueSize() const { return buffer_->value_size_; }

  // Returns true if a modifying function was called on the buffer view.
  bool IsModified() const { return modified_; }

  //----------------------------------------------------------------------------
  // Buffer data access
  //----------------------------------------------------------------------------

  // Returns a read-only pointer to the specified data.
  const void* GetData(int index = 0) const {
    return static_cast<uint8_t*>(data_) + index * buffer_->GetValueSize();
  }

  // Returns a writable pointer to the specified data.
  void* ModifyData(int index = 0) {
    modified_ = true;
    return const_cast<void*>(GetData(index));
  }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Resizes the buffer.
  //
  // The new size must not be larger than the buffer capacity. This does not
  // clear or alter the data in any way.
  void Resize(int new_size) {
    modified_ = modified_ || (new_size > size_);
    size_ = new_size;
  }

 private:
  RenderBuffer* const buffer_;
  void* const data_ = nullptr;
  int size_ = 0;
  bool modified_ = false;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_BUFFER_VIEW_H_
