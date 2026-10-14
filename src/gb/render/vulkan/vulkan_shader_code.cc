// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_shader_code.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/vulkan/vulkan_backend.h"

namespace gb {

std::unique_ptr<VulkanShaderCode> VulkanShaderCode::Create(
    VulkanInternal, VulkanBackend* backend, const void* code,
    int64_t code_size) {
  // SPIR-V is a sequence of 32-bit words, which Vulkan reads in place.
  if (code_size <= 0 || code_size % int64_t{sizeof(uint32_t)} != 0) {
    LOG(ERROR) << "Shader code is not a whole number of 32-bit words";
    return nullptr;
  }
  std::vector<uint32_t> aligned_code;
  if (reinterpret_cast<uintptr_t>(code) % alignof(uint32_t) != 0) {
    aligned_code.resize(code_size / sizeof(uint32_t));
    std::memcpy(aligned_code.data(), code, code_size);
    code = aligned_code.data();
  }
  auto [result, shader] = backend->GetDevice().createShaderModule(
      vk::ShaderModuleCreateInfo().setCodeSize(code_size).setPCode(
          static_cast<const uint32_t*>(code)));
  if (result != vk::Result::eSuccess) {
    return nullptr;
  }

  return absl::WrapUnique(new VulkanShaderCode(backend, shader));
}

VulkanShaderCode::~VulkanShaderCode() {
  backend_->GetGarbageCollector()->Dispose(shader_);
}

}  // namespace gb
