// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/storage_buffer.h"

#include <utility>

#include "absl/log/log.h"

namespace gb {

StorageBuffer::StorageBuffer(RenderInternal, ResourceEntry entry,
                             const RenderDataType* type,
                             std::unique_ptr<RenderBuffer> buffer)
    : Resource(std::move(entry)), type_(type), buffer_(std::move(buffer)) {}

StorageBuffer::~StorageBuffer() = default;

bool StorageBuffer::Clear() { return buffer_->Clear(); }

bool StorageBuffer::ValidateRange(TypeKey* type, int index, int count) const {
  if (type != type_->GetType()) {
    LOG(ERROR) << "Invalid type for storage buffer of " << type_->GetName();
    return false;
  }
  if (index < 0 || count < 0 || count > GetCount() - index) {
    LOG(ERROR) << "Invalid range of " << count << " values at index " << index
               << " for storage buffer of " << GetCount() << " values";
    return false;
  }
  return true;
}

bool StorageBuffer::DoSet(TypeKey* type, int index, const void* values,
                          int count) {
  if (!ValidateRange(type, index, count)) {
    return false;
  }
  return buffer_->SetRange(index, values, count);
}

bool StorageBuffer::DoGet(TypeKey* type, int index, void* values,
                          int count) const {
  if (!ValidateRange(type, index, count)) {
    return false;
  }
  if (GetVolatility() == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Cannot read storage buffer with kStaticWrite volatility";
    return false;
  }
  if (!buffer_->Get(index, values, count)) {
    LOG(ERROR) << "Failed to read storage buffer";
    return false;
  }
  return true;
}

}  // namespace gb
