// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_BINDING_DATA_H_
#define GB_RENDER_VULKAN_VULKAN_BINDING_DATA_H_

#include <deque>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "gb/render/binding_data.h"
#include "gb/render/vulkan/vulkan_render_buffer.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// This is the Vulkan implementation of BindingData.
//
// This class is thread-compatible.
class VulkanBindingData : public BindingData {
 public:
  struct TextureInfo {
    VulkanTexture* texture;
    int bound[kMaxFramesInFlight];
  };
  struct ConstantsInfo {
    const RenderDataType* type;
    VulkanRenderBuffer* buffer;
    int bound[kMaxFramesInFlight];
  };
  struct DataItem {
    DataItem() : binding_type(BindingType::kNone) {}
    explicit DataItem(VulkanTexture* texture_resource)
        : binding_type(BindingType::kTexture) {
      texture.texture = texture_resource;
      std::memset(texture.bound, 0, sizeof(texture.bound));
    }
    DataItem(const RenderDataType* type, VulkanRenderBuffer* buffer)
        : binding_type(BindingType::kConstants) {
      constants.type = type;
      constants.buffer = buffer;
      std::memset(constants.bound, 0, sizeof(constants.bound));
    }

    BindingType binding_type;
    union {
      TextureInfo texture;
      ConstantsInfo constants;
    };
  };

  // Empty binding data.
  VulkanBindingData(VulkanInternal, VulkanBindingDataFactory* factory,
                    RenderPipeline* pipeline, BindingSet set);

  // Binding data without uniform buffers.
  VulkanBindingData(
      VulkanInternal, VulkanBindingDataFactory* factory,
      RenderPipeline* pipeline, BindingSet set, std::vector<DataItem> data,
      std::array<vk::DescriptorSet, kMaxFramesInFlight> descriptor_sets);

  // Binding data with uniform buffers.
  VulkanBindingData(
      VulkanInternal, VulkanBindingDataFactory* factory,
      RenderPipeline* pipeline, BindingSet set, int buffer_group,
      int buffer_offset_index, std::vector<DataItem> data,
      std::array<vk::DescriptorSet, kMaxFramesInFlight> descriptor_sets);

  ~VulkanBindingData() override;

  int GetBufferGroup() const { return buffer_group_; }
  std::vector<uint32_t> GetBufferOffsets() const { return buffer_offsets_; }
  vk::DescriptorSet GetDescriptorSet(int frame_index) const {
    return descriptor_sets_[frame_index];
  }

  // Collect all required descriptor set updates and affected render buffers.
  void OnRender(VulkanRenderState* state);

 protected:
  bool Validate(int index, gb::TypeKey* type) const override;
  void DoSet(int index, const void* value) override;
  void DoGet(int index, void* value) const override;
  void DoGetDependencies(
      gb::ResourceDependencyList* dependencies) const override;

 private:
  const gb::WeakPtr<VulkanBindingDataFactory> factory_;
  const int buffer_group_ = 0;
  const int buffer_offset_index_ = 0;
  std::vector<DataItem> data_;
  std::vector<uint32_t> buffer_offsets_;
  std::array<vk::DescriptorSet, kMaxFramesInFlight> descriptor_sets_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_BINDING_DATA_H_
