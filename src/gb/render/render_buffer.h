// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_BUFFER_H_
#define GB_RENDER_RENDER_BUFFER_H_

#include <stdint.h>

#include <memory>

#include "gb/render/render_assert.h"
#include "gb/render/render_types.h"

namespace gb {

// This class defines a render buffer for a specific graphics API.
//
// This is an internal class called by other render classes to access the
// underlying graphics API and GPU.
//
// Derived classes should assume that all method arguments are already valid. No
// additional checking is required, outside of limits that are specific to the
// derived class implementation or underlying graphics API or GPU.
//
// This class and all derived classes must be thread-compatible.
class RenderBuffer {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  RenderBuffer(const RenderBuffer&) = delete;
  RenderBuffer(RenderBuffer&&) = delete;
  RenderBuffer& operator=(const RenderBuffer&) = delete;
  RenderBuffer& operator=(RenderBuffer&&) = delete;
  virtual ~RenderBuffer() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  DataVolatility GetVolatility() const { return volatility_; }
  int GetValueSize() const { return value_size_; }
  int GetCapacity() const { return capacity_; }
  int GetSize() const { return size_; }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Returns true if the buffer is locked (Set and Edit will fail).
  bool IsEditing() const { return view_ != nullptr; }

  // Clear the buffer with zero, optionally resetting the size.
  //
  // If specified, size is in number of values (not the size in bytes). This
  // cannot exceed the capacity of the buffer. This will do nothing if a
  // RenderBufferView currently exists for this buffer.
  bool Clear() { return Clear(size_); }
  bool Clear(int size);

  // Explicitly resize the buffer.
  //
  // Size is in number of values (not the size in bytes). This cannot exceed the
  // capacity of the buffer. This will do nothing if a RenderBufferView
  // currently exists for this buffer.
  //
  // If the size is increased, the new space will be filled with zero. If the
  // size is decreased, there is no change to the data in the buffer.
  bool Resize(int size);

  // Explicitly set the data in the buffer.
  //
  // Size is in number of values (not the size in bytes). This cannot exceed the
  // capacity of the buffer. This will do nothing if a RenderBufferView
  // currently exists for this buffer.
  bool Set(const void* data, int size);

  // Attempts to lock the buffer, allowing the data to be modified on the CPU.
  //
  // When the view is destructed, the modified data will become visible to the
  // render system on the next draw call that uses this buffer. For kStaticWrite
  // volatility, this will always return null.
  //
  // Only one view may exist at any given time, and subsequent calls will result
  // in a failure (this will return null). Similary, Set will fail if a
  // RenderBufferView is currently active.
  std::unique_ptr<RenderBufferView> Edit();

 protected:
  RenderBuffer(DataVolatility volatility, int value_size, int capacity)
      : volatility_(volatility), value_size_(value_size), capacity_(capacity) {}

  //----------------------------------------------------------------------------
  // Derived class interface
  //----------------------------------------------------------------------------

  // Implemenentation for Resize()
  //
  // This will never be called if editing is process (DoEdit was called but
  // OnEditEnd was not).
  virtual bool DoClear(int offset, int size) = 0;

  // Write new data to the buffer, returning true if the write was begun
  // successfully.
  //
  // This will never be called if editing is process (DoEdit was called but
  // OnEditEnd was not).
  //
  // If this returns true, and the buffer is readable (it isn't kStaticWrite
  // volatility), then it must reflect the data written. Returning true also
  // requires that the data will be transferred to the GPU before this buffer is
  // used in rendering -- althouth it does not imply the transfer has happened
  // yet.
  virtual bool DoSet(const void* data, int size) = 0;

  // Return an editable pointer to the buffer.
  //
  // This will never be called for kStaticWrite volatility buffers, or if
  // editing is already in process.
  //
  // This should return null on error. If this returns non-null, then the data
  // should be considered volatile and not be uploaded to the GPU until
  // OnEditEnd is called. OnEditEnd will only be called if this returns a
  // non-null value.
  virtual void* DoEditBegin() = 0;

  // Called to indicate editing has completed.
  //
  // The data should be transferred to the GPU before this buffer is used in
  // rendering. If modified is false, then no editing took place, and no
  // transfer is needed.
  virtual void OnEditEnd(bool modified) = 0;

 private:
  friend class RenderBufferView;

  const DataVolatility volatility_;
  const int value_size_;
  const int capacity_;
  int size_ = 0;
  RenderBufferView* view_ = nullptr;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_BUFFER_H_
