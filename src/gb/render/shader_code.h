// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_SHADER_CODE_H_
#define GB_RENDER_SHADER_CODE_H_

#include "gb/render/render_types.h"
#include "gb/resource/resource.h"

namespace gb {

// Shader code is the API-specific compiled shader.
//
// This is a completely opaque class implemented by a specific render backend,
// and is used to initialize shaders.
//
// This class and all derived classes must be thread-compatible.
class ShaderCode : public Resource {
 public:
  ShaderCode(const ShaderCode&) = delete;
  ShaderCode(ShaderCode&&) = delete;
  ShaderCode& operator=(const ShaderCode&) = delete;
  ShaderCode& operator=(ShaderCode&&) = delete;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  void SetData(RenderInternal, std::vector<uint8_t> data) {
    data_ = std::move(data);
  }
  const std::vector<uint8_t>& GetData(RenderInternal) const { return data_; }

 protected:
  ShaderCode(ResourceEntry entry) : Resource(std::move(entry)) {}
  ~ShaderCode() override = default;

  std::vector<uint8_t> data_;
};

}  // namespace gb

#endif  // GB_RENDER_SHADER_CODE_H_
