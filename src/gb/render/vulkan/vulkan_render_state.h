// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_RENDER_STATE_H_
#define GB_RENDER_VULKAN_VULKAN_RENDER_STATE_H_

#include <map>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "gb/render/draw_list.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// This structure contains all state necessary to render a scene in the
// VulkanBackend.
//
// This is updated by draw commands and is also passed into each render resource
// during VulkanBackend::EndFrame to collect all updates required before
// rendering begins.
struct VulkanRenderState {
  using InstanceDraw = std::vector<VulkanBindingData*>;
  using InstanceGroupDraw = absl::flat_hash_map<int, InstanceDraw>;
  using IndexDraw = absl::flat_hash_map<VulkanRenderBuffer*, InstanceGroupDraw>;
  using VertexDraw = absl::flat_hash_map<VulkanRenderBuffer*, IndexDraw>;
  struct MaterialDraw {
    absl::flat_hash_map<VulkanBindingData*, VertexDraw> mesh;
    std::vector<DrawCommand> commands;
  };
  using PipelineDraw = absl::flat_hash_map<VulkanRenderPipeline*, MaterialDraw>;
  using SceneDraw = absl::flat_hash_map<VulkanScene*, PipelineDraw>;
  using SceneGroupDraw = std::map<int, SceneDraw>;

  struct ImageBarrier {
    ImageBarrier(vk::Image image, uint32_t mip_level_count, uint32_t layer = 0)
        : image(image), mip_level_count(mip_level_count), layer(layer) {}
    vk::Image image;
    uint32_t mip_level_count;
    uint32_t layer;
  };

  struct ImageUpdate {
    ImageUpdate(vk::Buffer src_buffer, uint32_t src_offset, vk::Image dst_image,
                uint32_t mip_level, uint32_t image_width, uint32_t image_height,
                uint32_t image_layer = 0)
        : src_buffer(src_buffer),
          src_offset(src_offset),
          dst_image(dst_image),
          mip_level(mip_level),
          image_width(image_width),
          image_height(image_height),
          image_layer(image_layer),
          region_x(0),
          region_y(0),
          region_width(image_width),
          region_height(image_height) {}
    ImageUpdate(vk::Buffer src_buffer, uint32_t src_offset, vk::Image dst_image,
                uint32_t mip_level, uint32_t image_width, uint32_t image_height,
                uint32_t image_layer, int32_t region_x, int32_t region_y,
                uint32_t region_width, uint32_t region_height)
        : src_buffer(src_buffer),
          src_offset(src_offset),
          dst_image(dst_image),
          mip_level(mip_level),
          image_width(image_width),
          image_height(image_height),
          image_layer(image_layer),
          region_x(region_x),
          region_y(region_y),
          region_width(region_width),
          region_height(region_height) {}
    vk::Buffer src_buffer;
    uint32_t src_offset;
    vk::Image dst_image;
    uint32_t mip_level;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_layer;
    int32_t region_x;
    int32_t region_y;
    uint32_t region_width;
    uint32_t region_height;
  };

  struct BufferUpdate {
    BufferUpdate(vk::Buffer src_buffer, vk::Buffer dst_buffer,
                 vk::AccessFlags dst_access, vk::DeviceSize copy_size)
        : src_buffer(src_buffer),
          dst_buffer(dst_buffer),
          dst_access(dst_access),
          copy_size(copy_size) {}
    vk::Buffer src_buffer;
    vk::Buffer dst_buffer;
    vk::AccessFlags dst_access;
    vk::DeviceSize copy_size;
  };

  struct SetImageUpdate {
    SetImageUpdate(vk::DescriptorSet descriptor_set, uint32_t binding,
                   vk::Sampler sampler, vk::ImageView image_view)
        : descriptor_set(descriptor_set),
          binding(binding),
          info(sampler, image_view, vk::ImageLayout::eShaderReadOnlyOptimal) {}
    vk::DescriptorSet descriptor_set;
    uint32_t binding;
    vk::DescriptorImageInfo info;
  };

  struct SetBufferUpdate {
    SetBufferUpdate(vk::DescriptorSet descriptor_set, uint32_t binding,
                    vk::Buffer buffer, vk::DeviceSize size)
        : descriptor_set(descriptor_set),
          binding(binding),
          info(buffer, 0, size) {}
    vk::DescriptorSet descriptor_set;
    uint32_t binding;
    vk::DescriptorBufferInfo info;
  };

  // Frame being renderered.
  int frame = 0;

  // Draw lists.
  SceneGroupDraw draw;

  // Accumulates all resources participating in a frame.
  absl::flat_hash_set<VulkanBindingData*> binding_data;
  absl::flat_hash_set<VulkanRenderBuffer*> buffers;
  absl::flat_hash_set<VulkanTexture*> textures;
  absl::flat_hash_set<VulkanTextureArray*> texture_arrays;

  // Accumulates all updates to resources in a frame.
  std::vector<ImageBarrier> image_barriers;
  std::vector<ImageUpdate> image_updates;
  std::vector<BufferUpdate> buffer_updates;
  std::vector<SetImageUpdate> set_image_updates;
  std::vector<SetBufferUpdate> set_buffer_updates;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_RENDER_STATE_H_
