// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_SHADER_CODE_H_
#define GB_RENDER_WEBGPU_WEBGPU_SHADER_CODE_H_

#include <cstdint>
#include <memory>

#include "gb/render/shader_code.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

// WebGPU implementation of shader code: a shader module made from WGSL text,
// whose entry point is "main".
//
// This class is thread-compatible.
class WebGpuShaderCode final : public ShaderCode {
 public:
  // Creates the shader module from `code_size` bytes of WGSL text. Returns
  // null, having logged the browser's message, if the WGSL has an error.
  static std::unique_ptr<WebGpuShaderCode> Create(WebGpuBackend* backend,
                                                  const void* code,
                                                  int64_t code_size);

  const wgpu::ShaderModule& Get() const { return module_; }

 private:
  explicit WebGpuShaderCode(wgpu::ShaderModule module)
      : module_(std::move(module)) {}

  const wgpu::ShaderModule module_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_SHADER_CODE_H_
