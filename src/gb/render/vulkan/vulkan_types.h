// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_TYPES_H_
#define GB_RENDER_VULKAN_VULKAN_TYPES_H_

#include <atomic>

#include "gb/render/render_assert.h"

// Must be after renderer/render_assert.h
#include <vulkan/vulkan.hpp>

#include "gb/base/access_token.h"

namespace gb {

class VulkanBackend;
class VulkanBindingData;
class VulkanBindingDataFactory;
class VulkanBuffer;
class VulkanDescriptorPool;
class VulkanFrame;
class VulkanImage;
class VulkanRenderBuffer;
class VulkanRenderPipeline;
struct VulkanRenderState;
class VulkanScene;
class VulkanSceneType;
class VulkanShaderCode;
class VulkanTexture;
class VulkanTextureArray;
class VulkanWindow;

GB_BEGIN_ACCESS_TOKEN(VulkanInternal)
friend class VulkanBackend;
friend class VulkanBindingDataFactory;
friend class VulkanRenderPipeline;
friend class VulkanSceneType;
GB_END_ACCESS_TOKEN()

enum class VulkanBufferType {
  kUniform,
  kVertex,
  kIndex,
};

// Defines the maximum number frames being processed at any given time.
inline constexpr int kMaxFramesInFlight = 2;

// These define the descriptor pool sizes and buffer slots for the different
// binding set types.
inline constexpr int kMaxScenesPerGroup = 10;
inline constexpr int kMaxMaterialsPerGroup = 50;
inline constexpr int kMaxInstancesPerGroup = 250;

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_TYPES_H_
