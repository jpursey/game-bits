// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_binding_data.h"

#include "gb/render/vulkan/vulkan_binding_data_factory.h"
#include "gb/render/vulkan/vulkan_render_pipeline.h"
#include "gb/render/vulkan/vulkan_texture.h"
#include "glog/logging.h"

namespace gb {

VulkanBindingData::VulkanBindingData(
    VulkanInternal, VulkanBindingDataFactory* factory, RenderPipeline* pipeline,
    BindingSet set, int buffer_group, int buffer_offset_index,
    std::vector<DataItem> data,
    std::array<vk::DescriptorSet, kMaxFramesInFlight> descriptor_sets)
    : BindingData(pipeline, set),
      factory_(factory),
      buffer_group_(buffer_group),
      buffer_offset_index_(buffer_offset_index),
      data_(std::move(data)),
      descriptor_sets_(descriptor_sets) {
  buffer_offsets_.reserve(factory->GetBufferCount());
  for (const DataItem& item : data_) {
    if (item.binding_type != BindingType::kConstants) {
      continue;
    }
    buffer_offsets_.push_back(buffer_offset_index_ *
                              item.constants.buffer->GetAlignSize());
  }
  RENDER_ASSERT(buffer_offsets_.size() == factory->GetBufferCount());
}

VulkanBindingData::VulkanBindingData(
    VulkanInternal, VulkanBindingDataFactory* factory, RenderPipeline* pipeline,
    BindingSet set, std::vector<DataItem> data,
    std::array<vk::DescriptorSet, kMaxFramesInFlight> descriptor_sets)
    : BindingData(pipeline, set),
      factory_(factory),
      data_(std::move(data)),
      descriptor_sets_(descriptor_sets) {}

VulkanBindingData::VulkanBindingData(VulkanInternal,
                                     VulkanBindingDataFactory* factory,
                                     RenderPipeline* pipeline, BindingSet set)
    : BindingData(pipeline, set), factory_(factory) {}

VulkanBindingData::~VulkanBindingData() {
  if (!data_.empty()) {
    auto factory = factory_.Lock();
    if (factory != nullptr) {
      factory->DisposeBindingData(buffer_group_, buffer_offset_index_,
                                  absl::MakeSpan(descriptor_sets_));
    }
  }
}

void VulkanBindingData::OnRender(VulkanRenderState* state) {
  const int index = state->frame % kMaxFramesInFlight;
  const vk::DescriptorSet& descriptor_set = descriptor_sets_[index];
  const int num_bindings = static_cast<int>(data_.size());
  for (int binding = 0; binding < num_bindings; ++binding) {
    DataItem& item = data_[binding];
    if (item.binding_type == BindingType::kTexture) {
      auto& info = item.texture;
      state->textures.insert(info.texture);
      const auto& handle = info.texture->GetImageHandle(state->frame);
      if (info.bound[index] != handle.version) {
        info.bound[index] = handle.version;
        state->set_image_updates.emplace_back(descriptor_set, binding,
                                              info.texture->GetSampler(),
                                              handle.image->GetView());
      }
    } else if (item.binding_type == BindingType::kConstants) {
      auto& info = item.constants;
      state->buffers.insert(info.buffer);
      const auto& handle = info.buffer->GetBufferHandle(state->frame);
      if (info.bound[index] != handle.version) {
        info.bound[index] = handle.version;
        state->set_buffer_updates.emplace_back(descriptor_set, binding,
                                               handle.buffer,
                                               info.buffer->GetValueSize());
      }
    }
  }
}

bool VulkanBindingData::Validate(int index, gb::TypeKey* type) const {
  static gb::TypeKey* texture_type = gb::TypeKey::Get<Texture*>();

  if (index < 0 || index > static_cast<int>(data_.size())) {
    return false;
  }

  const auto& item = data_[index];
  switch (item.binding_type) {
    case BindingType::kNone:
      LOG(ERROR) << "Untyped binding";
      return false;
    case BindingType::kTexture:
      return type == texture_type;
    case BindingType::kConstants:
      return item.constants.type->GetType() == type;
  }
  LOG(FATAL) << "Unhandled binding type for VulkanBindingData";
  return false;
}

void VulkanBindingData::DoSet(int index, const void* value) {
  auto& item = data_[index];
  if (item.binding_type == BindingType::kConstants) {
    item.constants.buffer->SetValue(buffer_offset_index_, value);
  } else {
    auto* texture = *static_cast<VulkanTexture* const*>(value);
    auto& info = item.texture;
    if (texture != info.texture) {
      info.texture = texture;
      for (auto& bound : info.bound) {
        bound = -1;
      }
    }
  }
}

void VulkanBindingData::DoGet(int index, void* value) const {
  const auto& item = data_[index];
  if (item.binding_type == BindingType::kConstants) {
    item.constants.buffer->GetValue(buffer_offset_index_, value);
  } else {
    *static_cast<Texture**>(value) =
        static_cast<Texture*>(item.texture.texture);
  }
}

void VulkanBindingData::DoGetDependencies(
    gb::ResourceDependencyList* dependencies) const {
  for (const auto& item : data_) {
    if (item.binding_type == BindingType::kTexture &&
        item.texture.texture != nullptr) {
      dependencies->push_back(item.texture.texture);
    }
  }
}

}  // namespace gb
