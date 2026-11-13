// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_TYPES_H_
#define GB_RENDER_VULKAN_VULKAN_TYPES_H_

#include <utility>

#include "gb/render/render_assert.h"

// Must be after renderer/render_assert.h
#include <vulkan/vulkan.hpp>

#include "gb/base/access_token.h"

namespace gb {

class VulkanBackend;
class VulkanBindingData;
class VulkanBindingDataFactory;
class VulkanBindingLayout;
class VulkanBuffer;
class VulkanDescriptorPool;
class VulkanFrame;
class VulkanImage;
class VulkanMeshPoolBuffers;
class VulkanRenderBuffer;
class VulkanRenderPipeline;
struct VulkanRenderState;
class VulkanShaderCode;
class VulkanTexture;
class VulkanTextureArray;
class VulkanView;
class VulkanVolumeTexture;
class VulkanWindow;

GB_BEGIN_ACCESS_TOKEN(VulkanInternal)
friend class VulkanBackend;
friend class VulkanBindingDataFactory;
friend class VulkanBindingLayout;
friend class VulkanMeshPoolBuffers;
friend class VulkanRenderPipeline;
GB_END_ACCESS_TOKEN()

enum class VulkanBufferType {
  kUniform,
  kVertex,
  kIndex,
  kStorage,
};

// Defines the maximum number frames being processed at any given time.
inline constexpr int kMaxFramesInFlight = 2;

// These define the descriptor pool sizes and buffer slots for the different
// binding set types.
inline constexpr int kMaxViewsPerGroup = 10;
inline constexpr int kMaxMaterialsPerGroup = 50;
inline constexpr int kMaxInstancesPerGroup = 250;

// The attachments of a render pass with one subpass. A pipeline created for a
// render pass draws in any render pass of the same layout.
struct VulkanPassLayout {
  bool operator==(const VulkanPassLayout&) const = default;

  template <typename H>
  friend H AbslHashValue(H h, const VulkanPassLayout& layout) {
    return H::combine(std::move(h), layout.color_format, layout.depth_format,
                      layout.sample_count);
  }

  vk::Format color_format = vk::Format::eUndefined;  // eUndefined for none.
  vk::Format depth_format = vk::Format::eUndefined;  // eUndefined for none.
  vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1;
};

// A render pass with one subpass, and its layout.
struct VulkanRenderPassInfo {
  vk::RenderPass render_pass;
  VulkanPassLayout layout;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_TYPES_H_
