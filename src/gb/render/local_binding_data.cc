// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/local_binding_data.h"

namespace gb {

LocalBindingData::LocalBindingData(RenderInternal, BindingSet set,
                                   absl::Span<const Binding> bindings)
    : BindingData(nullptr, set) {
  int binding_count = 0;
  for (const auto& binding : bindings) {
    if (binding.index >= binding_count) {
      binding_count = binding.index + 1;
    }
  }
  if (binding_count == 0) {
    return;
  }
  data_.resize(binding_count);

  const RenderDataType* const texture_type = GetTextureDataType();
  size_t size = 0;
  for (const auto& binding : bindings) {
    const RenderDataType*& type =
        std::get<const RenderDataType*>(data_[binding.index]);
    if (type != nullptr) {
      continue;
    }
    switch (binding.binding_type) {
      case BindingType::kConstants:
        type = binding.constants_type;
        break;
      case BindingType::kTexture:
        type = texture_type;
        break;
      default:
        LOG(FATAL) << "Unhandled binding type in LocalBindingData constructor";
    }
    size += type->GetSize();
  }
  backing_buffer_ = new uint8_t[size];
  std::memset(backing_buffer_, 0, size);

  size_t offset = 0;
  for (auto& value : data_) {
    auto* type = std::get<const RenderDataType*>(value);
    if (type == nullptr) {
      continue;
    }
    std::get<void*>(value) = backing_buffer_ + offset;
    offset += type->GetSize();
  }
}

LocalBindingData::LocalBindingData(RenderInternal,
                                   const LocalBindingData& other)
    : BindingData(nullptr, other.GetSet()),
      data_(other.data_),
      backing_buffer_size_(other.backing_buffer_size_) {
  if (backing_buffer_size_ == 0) {
    return;
  }
  backing_buffer_ = new uint8_t[backing_buffer_size_];
  std::memcpy(backing_buffer_, other.backing_buffer_, backing_buffer_size_);

  size_t offset = 0;
  for (auto& value : data_) {
    auto* type = std::get<const RenderDataType*>(value);
    if (type == nullptr) {
      continue;
    }
    std::get<void*>(value) = backing_buffer_ + offset;
    offset += type->GetSize();
  }
}

LocalBindingData::~LocalBindingData() { delete[] backing_buffer_; }

const RenderDataType* LocalBindingData::GetTextureDataType() {
  static RenderDataType type({}, "", TypeKey::Get<Texture*>(), sizeof(Texture*));
  return &type;
}

void LocalBindingData::CopyTo(BindingData* binding_data) const {
  const int count = static_cast<int>(data_.size());
  for (int i = 0; i < count; ++i) {
    const auto* type = std::get<const RenderDataType*>(data_[i]);
    if (type == nullptr) {
      continue;
    }
    binding_data->SetInternal({}, i, type->GetType(),
                              std::get<void*>(data_[i]));
  }
}

bool LocalBindingData::Validate(int index, TypeKey* type) const {
  if (index < 0 || index >= static_cast<int>(data_.size())) {
    return false;
  }
  const auto* actual_type = std::get<const RenderDataType*>(data_[index]);
  return actual_type != nullptr && actual_type->GetType() == type;
}

void LocalBindingData::DoSet(int index, const void* value) {
  const auto* type = std::get<const RenderDataType*>(data_[index]);
  std::memcpy(std::get<void*>(data_[index]), value, type->GetSize());
}

void LocalBindingData::DoGet(int index, void* value) const {
  const auto* type = std::get<const RenderDataType*>(data_[index]);
  std::memcpy(value, std::get<void*>(data_[index]), type->GetSize());
}

void LocalBindingData::DoGetDependencies(
    ResourceDependencyList* dependencies) const {
  const RenderDataType* const texture_type = GetTextureDataType();
  for (const auto& value : data_) {
    if (std::get<const RenderDataType*>(value) == texture_type) {
      Texture** texture = static_cast<Texture**>(std::get<void*>(value));
      if (*texture != nullptr) {
        dependencies->push_back(*texture);
      }
    }
  }
}

}  // namespace gb
