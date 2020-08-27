// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_FORMAT_H_
#define GB_RENDER_VULKAN_VULKAN_FORMAT_H_

#include "absl/types/span.h"
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

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_FORMAT_H_