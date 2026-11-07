// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_FORMAT_H_
#define GB_RENDER_VULKAN_VULKAN_FORMAT_H_

#include "gb/render/render_types.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

inline bool IsDepthFormat(vk::Format format) {
  return format == vk::Format::eD32Sfloat ||
         format == vk::Format::eD32SfloatS8Uint ||
         format == vk::Format::eD24UnormS8Uint;
}

inline bool IsStencilFormat(vk::Format format) {
  return format == vk::Format::eD32SfloatS8Uint ||
         format == vk::Format::eD24UnormS8Uint;
}

// The Vulkan format of an image holding texels in a TextureFormat, and how its
// view reads them as the TextureFormat describes.
struct VulkanTextureFormat {
  vk::Format format = vk::Format::eUndefined;
  vk::ComponentMapping components = {};
};

inline VulkanTextureFormat GetVulkanTextureFormat(TextureFormat format) {
  switch (format) {
    case TextureFormat::kRgba8Srgb:
      return {vk::Format::eR8G8B8A8Srgb};
    case TextureFormat::kRgba8:
      return {vk::Format::eR8G8B8A8Unorm};
    case TextureFormat::kRg8:
      return {vk::Format::eR8G8Unorm};
    case TextureFormat::kR8:
      return {vk::Format::eR8Unorm};
    case TextureFormat::kRgba4:
      // Every device supports this, unlike eR4G4B4A4UnormPack16. Its bits are
      // blue, green, red, alpha from the top, so the view swaps red and blue.
      return {vk::Format::eB4G4R4A4UnormPack16,
              vk::ComponentMapping(
                  vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eIdentity,
                  vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eIdentity)};
    case TextureFormat::kDepth:
      // Render targets aren't supported yet, so there is no depth format.
      return {};
  }
  return {};
}

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_FORMAT_H_
