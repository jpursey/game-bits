// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_STORAGE_BUFFER_H_
#define GB_RENDER_STORAGE_BUFFER_H_

#include <memory>

#include "absl/types/span.h"
#include "gb/base/type_info.h"
#include "gb/render/render_buffer.h"
#include "gb/render/render_types.h"
#include "gb/resource/resource.h"

namespace gb {

// A storage buffer is an array of structured values that shaders read, all of
// one type registered with RenderSystem::RegisterConstantsType(). Its count is
// set when it is created.
//
// Storage buffers are bound to shaders via binding data (see BindingData), as
// a readonly buffer block in GLSL, and can also be changed depending on their
// data volatility. The type's C++ layout must match GLSL's std430 layout,
// which a type that matches std140 (as constants do) usually also does.
//
// This class is thread-compatible.
class StorageBuffer final : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the data volatility for the values, specifying how often they are
  // likely to be edited and the relative memory overhead of doing so.
  DataVolatility GetVolatility() const { return buffer_->GetVolatility(); }

  // Returns the type of each value.
  const RenderDataType* GetType() const { return type_; }

  // Returns the number of values.
  int GetCount() const { return buffer_->GetSize(); }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Sets every value to zero.
  bool Clear();

  // Writes values.size() values, starting at the value at index.
  //
  // Returns false if Type is not the buffer's type, or the values don't fit.
  //
  // Note: A kStaticWrite buffer's values are not readable, so unless this sets
  // every value, it may fail if the buffer is currently being (or was
  // recently) rendered.
  template <typename Type>
  bool Set(int index, absl::Span<const Type> values);

  // Reads values.size() values, starting at the value at index.
  //
  // Returns false if Type is not the buffer's type, the values don't fit, or
  // the buffer has kStaticWrite volatility.
  template <typename Type>
  bool Get(int index, absl::Span<Type> values) const;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  // The buffer holds values of the type's size, and its size is the count.
  StorageBuffer(RenderInternal, ResourceEntry entry, const RenderDataType* type,
                std::unique_ptr<RenderBuffer> buffer);

  RenderBuffer* GetBuffer(RenderInternal) const { return buffer_.get(); }

 private:
  ~StorageBuffer() override;

  // Returns true if "count" values of "type" at "index" are within the buffer,
  // logging why not otherwise.
  bool ValidateRange(TypeKey* type, int index, int count) const;

  bool DoSet(TypeKey* type, int index, const void* values, int count);
  bool DoGet(TypeKey* type, int index, void* values, int count) const;

  const RenderDataType* const type_;
  const std::unique_ptr<RenderBuffer> buffer_;
};

template <typename Type>
inline bool StorageBuffer::Set(int index, absl::Span<const Type> values) {
  return DoSet(TypeKey::Get<Type>(), index, values.data(),
               static_cast<int>(values.size()));
}

template <typename Type>
inline bool StorageBuffer::Get(int index, absl::Span<Type> values) const {
  return DoGet(TypeKey::Get<Type>(), index, values.data(),
               static_cast<int>(values.size()));
}

}  // namespace gb

#endif  // GB_RENDER_STORAGE_BUFFER_H_
