// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_SHADER_CODE_H_
#define GB_RENDER_VULKAN_VULKAN_SHADER_CODE_H_

#include "gb/render/shader.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

class VulkanShaderCode final : public ShaderCode {
 public:
  static std::unique_ptr<VulkanShaderCode> Create(VulkanInternal,
                                                  VulkanBackend* backend,
                                                  const void* code,
                                                  int64_t code_size);
  ~VulkanShaderCode() override;

  vk::ShaderModule Get() { return shader_; }

 private:
  VulkanShaderCode(VulkanBackend* backend, vk::ShaderModule shader)
      : backend_(backend), shader_(shader) {}

  VulkanBackend* backend_;
  vk::ShaderModule shader_;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_SHADER_CODE_H_
