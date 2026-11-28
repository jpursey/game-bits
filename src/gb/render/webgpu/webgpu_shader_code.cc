// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_shader_code.h"

#include <string_view>

#include "absl/memory/memory.h"

namespace gb {

std::unique_ptr<WebGpuShaderCode> WebGpuShaderCode::Create(
    WebGpuBackend* backend, const void* code, int64_t code_size) {
  wgpu::ShaderSourceWGSL source;
  source.code = std::string_view(static_cast<const char*>(code),
                                 static_cast<size_t>(code_size));
  wgpu::ShaderModuleDescriptor descriptor;
  descriptor.nextInChain = &source;
  wgpu::ShaderModule module;
  if (!backend->CreateInErrorScope("shader code", [&] {
        module = backend->GetDevice().CreateShaderModule(&descriptor);
      })) {
    return nullptr;
  }
  return absl::WrapUnique(new WebGpuShaderCode(std::move(module)));
}

}  // namespace gb
