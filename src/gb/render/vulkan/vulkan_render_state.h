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
#include "gb/render/render_pass.h"
#include "gb/render/vulkan/vulkan_image.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// This structure contains all state necessary to render a frame in the
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
  using ViewDraw = absl::flat_hash_map<VulkanView*, PipelineDraw>;
  using ViewGroupDraw = std::map<int, ViewDraw>;
  using PassDraw = absl::flat_hash_map<RenderPass*, ViewGroupDraw>;

  // Prepares a layer of an image for its updates. If keep_contents is false,
  // the updates replace every pixel of every mip level.
  struct ImageBarrier {
    ImageBarrier(VulkanImage* image, uint32_t mip_level_count, uint32_t layer,
                 bool keep_contents)
        : image(image),
          mip_level_count(mip_level_count),
          layer(layer),
          keep_contents(keep_contents) {}
    VulkanImage* image;
    uint32_t mip_level_count;
    uint32_t layer;
    bool keep_contents;
  };

  // Copies a region of a mip level from src_buffer, where the region's first
  // texel is at src_offset, each row is image_width texels after the last, and
  // each slice (in a 3D image) is image_height rows after the last.
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
          region_offset(0, 0, 0),
          region_extent(image_width, image_height, 1) {}
    ImageUpdate(vk::Buffer src_buffer, uint32_t src_offset, vk::Image dst_image,
                uint32_t mip_level, uint32_t image_width, uint32_t image_height,
                uint32_t image_layer, vk::Offset3D region_offset,
                vk::Extent3D region_extent)
        : src_buffer(src_buffer),
          src_offset(src_offset),
          dst_image(dst_image),
          mip_level(mip_level),
          image_width(image_width),
          image_height(image_height),
          image_layer(image_layer),
          region_offset(region_offset),
          region_extent(region_extent) {}
    vk::Buffer src_buffer;
    uint32_t src_offset;
    vk::Image dst_image;
    uint32_t mip_level;
    uint32_t image_width;
    uint32_t image_height;
    uint32_t image_layer;
    vk::Offset3D region_offset;
    vk::Extent3D region_extent;
  };

  // Copies copy_size bytes at src_offset in src_buffer to dst_offset in
  // dst_buffer, which is then read with dst_access.
  struct BufferUpdate {
    BufferUpdate(vk::Buffer src_buffer, vk::DeviceSize src_offset,
                 vk::Buffer dst_buffer, vk::DeviceSize dst_offset,
                 vk::DeviceSize copy_size, vk::AccessFlags dst_access)
        : src_buffer(src_buffer),
          dst_buffer(dst_buffer),
          dst_access(dst_access),
          src_offset(src_offset),
          dst_offset(dst_offset),
          copy_size(copy_size) {}
    vk::Buffer src_buffer;
    vk::Buffer dst_buffer;
    vk::AccessFlags dst_access;
    vk::DeviceSize src_offset;
    vk::DeviceSize dst_offset;
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

  // Binds `size` bytes from the start of `buffer`, which may be VK_WHOLE_SIZE.
  struct SetBufferUpdate {
    SetBufferUpdate(vk::DescriptorSet descriptor_set, uint32_t binding,
                    vk::DescriptorType type, vk::Buffer buffer,
                    vk::DeviceSize size)
        : descriptor_set(descriptor_set),
          binding(binding),
          type(type),
          info(buffer, 0, size) {}
    vk::DescriptorSet descriptor_set;
    uint32_t binding;
    vk::DescriptorType type;
    vk::DescriptorBufferInfo info;
  };

  // Frame being renderered.
  int frame = 0;

  // Draw lists for each pass, keyed by the pass, or null for the frame.
  PassDraw draw;

  // Accumulates all resources participating in a frame.
  absl::flat_hash_set<VulkanBindingData*> binding_data;
  absl::flat_hash_set<VulkanRenderBuffer*> buffers;
  absl::flat_hash_set<VulkanTexture*> textures;
  absl::flat_hash_set<VulkanTextureArray*> texture_arrays;
  absl::flat_hash_set<VulkanVolumeTexture*> volume_textures;

  // Accumulates all updates to resources in a frame.
  std::vector<ImageBarrier> image_barriers;
  std::vector<ImageUpdate> image_updates;
  std::vector<BufferUpdate> buffer_updates;
  std::vector<SetImageUpdate> set_image_updates;
  std::vector<SetBufferUpdate> set_buffer_updates;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_RENDER_STATE_H_
