// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_shader_code.h"

#include "gb/render/vulkan/vulkan_backend.h"

namespace gb {

std::unique_ptr<VulkanShaderCode> VulkanShaderCode::Create(
    VulkanInternal, VulkanBackend* backend,
    const void* code, int64_t code_size) {
  auto [result, shader] = backend->GetDevice().createShaderModule(
      vk::ShaderModuleCreateInfo()
          .setCodeSize(code_size)
          .setPCode(static_cast<const uint32_t*>(code)));
  if (result != vk::Result::eSuccess) {
    return nullptr;
  }

  return absl::WrapUnique(new VulkanShaderCode(backend, shader));
}

VulkanShaderCode::~VulkanShaderCode() {
  backend_->GetGarbageCollector()->Dispose(shader_);
}

}  // namespace gb
