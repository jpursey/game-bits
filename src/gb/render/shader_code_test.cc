// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/shader_code.h"

#include "absl/strings/str_cat.h"
#include "gb/render/render_test.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class ShaderCodeTest : public RenderTest {};

TEST_F(ShaderCodeTest, CreateAsResourcePtr) {
  CreateSystem();
  ResourcePtr<ShaderCode> shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  ASSERT_NE(shader_code, nullptr);
  EXPECT_EQ(static_cast<TestShaderCode*>(shader_code.Get())->GetCode(),
            absl::StrCat(kVertexShaderCode));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(ShaderCodeTest, CreateInResourceSet) {
  CreateSystem();
  ResourceSet resource_set;
  ShaderCode* shader_code = render_system_->CreateShaderCode(
      &resource_set, kVertexShaderCode.data(), kVertexShaderCode.size());
  ASSERT_NE(shader_code, nullptr);
  EXPECT_EQ(static_cast<TestShaderCode*>(shader_code)->GetCode(),
            absl::StrCat(kVertexShaderCode));
  EXPECT_EQ(resource_set.Get<ShaderCode>(shader_code->GetResourceId()),
            shader_code);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
