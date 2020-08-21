// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_SHADER_CODE_H_
#define GB_RENDER_TEST_SHADER_CODE_H_

#include "gb/render/shader_code.h"

namespace gb {

class TestShaderCode final : public ShaderCode {
 public:
  TestShaderCode(const void* code, int64_t code_size);
  ~TestShaderCode() override;

  const std::string& GetCode() const { return code_; }

 private:
  std::string code_;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_SHADER_CODE_H_
