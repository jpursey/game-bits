// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/local_binding_data.h"

#include <cstring>

#include "absl/log/log.h"

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
  slots_.resize(binding_count);

  const RenderDataType* const texture_type = GetTextureDataType();
  const RenderDataType* const texture_array_type = GetTextureArrayDataType();
  for (const auto& binding : bindings) {
    Slot& slot = slots_[binding.index];
    if (slot.type != nullptr) {
      continue;
    }
    switch (binding.binding_type) {
      case BindingType::kConstants:
        slot.type = binding.constants_type;
        break;
      case BindingType::kTexture:
        slot.type = texture_type;
        break;
      case BindingType::kTextureArray:
        slot.type = texture_array_type;
        break;
      default:
        LOG(FATAL) << "Unhandled binding type in LocalBindingData constructor";
    }
  }

  // Values are only accessed with memcpy, so they are packed without
  // alignment.
  int size = 0;
  for (Slot& slot : slots_) {
    if (slot.type != nullptr) {
      slot.offset = size;
      size += slot.type->GetSize();
    }
  }
  buffer_.resize(size);
}

LocalBindingData::LocalBindingData(RenderInternal,
                                   const LocalBindingData& other)
    : BindingData(nullptr, other.GetSet()),
      slots_(other.slots_),
      buffer_(other.buffer_) {}

LocalBindingData::~LocalBindingData() = default;

const RenderDataType* LocalBindingData::GetTextureDataType() {
  static RenderDataType type({}, "", TypeKey::Get<Texture*>(),
                             sizeof(Texture*));
  return &type;
}

const RenderDataType* LocalBindingData::GetTextureArrayDataType() {
  static RenderDataType type({}, "", TypeKey::Get<TextureArray*>(),
                             sizeof(TextureArray*));
  return &type;
}

void LocalBindingData::CopyTo(BindingData* binding_data) const {
  const int count = static_cast<int>(slots_.size());
  for (int i = 0; i < count; ++i) {
    const Slot& slot = slots_[i];
    if (slot.type == nullptr) {
      continue;
    }
    binding_data->SetInternal({}, i, slot.type->GetType(),
                              buffer_.data() + slot.offset);
  }
}

bool LocalBindingData::Validate(int index, TypeKey* type) const {
  if (index < 0 || index >= static_cast<int>(slots_.size())) {
    return false;
  }
  const auto* actual_type = slots_[index].type;
  return actual_type != nullptr && actual_type->GetType() == type;
}

void LocalBindingData::DoSet(int index, const void* value) {
  const Slot& slot = slots_[index];
  std::memcpy(buffer_.data() + slot.offset, value, slot.type->GetSize());
}

void LocalBindingData::DoGet(int index, void* value) const {
  const Slot& slot = slots_[index];
  std::memcpy(value, buffer_.data() + slot.offset, slot.type->GetSize());
}

void LocalBindingData::DoGetDependencies(
    ResourceDependencyList* dependencies) const {
  const RenderDataType* const texture_type = GetTextureDataType();
  const RenderDataType* const texture_array_type = GetTextureArrayDataType();
  for (const Slot& slot : slots_) {
    if (slot.type == texture_type) {
      Texture* texture = nullptr;
      std::memcpy(&texture, buffer_.data() + slot.offset, sizeof(texture));
      if (texture != nullptr) {
        dependencies->push_back(texture);
      }
    } else if (slot.type == texture_array_type) {
      TextureArray* texture_array = nullptr;
      std::memcpy(&texture_array, buffer_.data() + slot.offset,
                  sizeof(texture_array));
      if (texture_array != nullptr) {
        dependencies->push_back(texture_array);
      }
    }
  }
}

}  // namespace gb
