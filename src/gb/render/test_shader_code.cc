// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_shader_code.h"

namespace gb {

TestShaderCode::TestShaderCode(ResourceEntry entry, const void* code,
                               int64_t code_size)
    : ShaderCode(std::move(entry)) {
  code_.assign(static_cast<const char*>(code), code_size);
}

TestShaderCode::~TestShaderCode() {}

}  // namespace gb
