// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/shader.h"

#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Contains;
using ::testing::UnorderedElementsAreArray;

class ShaderTest : public RenderTest {};

TEST_F(ShaderTest, CreateAsResourcePtr) {
  CreateSystem();
  auto shader_code = render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                                      kVertexShaderCode.size());
  ResourcePtr<Shader> shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code), {}, {}, {});
  ASSERT_NE(shader, nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(ShaderTest, CreateInResourceSet) {
  CreateSystem();
  ResourceSet resource_set;
  auto shader_code = render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                                      kVertexShaderCode.size());
  Shader* shader = render_system_->CreateShader(
      &resource_set, ShaderType::kVertex, std::move(shader_code), {}, {}, {});
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(resource_set.Get<Shader>(shader->GetResourceId()), shader);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(ShaderTest, FailCreate) {
  auto shader =
      render_system_->CreateShader(ShaderType::kVertex, nullptr, {}, {}, {});
  EXPECT_EQ(shader, nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(ShaderTest, TypeAndCodeProperties) {
  CreateSystem();

  // Vertex shader.
  auto shader_code = render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                                      kVertexShaderCode.size());
  auto* shader_code_ptr = shader_code.get();
  auto shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code), {}, {}, {});
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->GetType(), ShaderType::kVertex);
  EXPECT_EQ(shader->GetCode(), shader_code_ptr);

  // Fragment shader.
  shader_code = render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                                 kFragmentShaderCode.size());
  shader_code_ptr = shader_code.get();
  shader = render_system_->CreateShader(ShaderType::kFragment,
                                        std::move(shader_code), {}, {}, {});
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->GetType(), ShaderType::kFragment);
  EXPECT_EQ(shader->GetCode(), shader_code_ptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(ShaderTest, InputOutputBindingProperties) {
  CreateSystem();
  auto* constants_type =
      render_system_->RegisterConstantsType<Vector4>("constants");
  ASSERT_NE(constants_type, nullptr);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kMaterial, 1)
          .SetTexture(),
      Binding()
          .SetShaders({ShaderType::kVertex, ShaderType::kFragment})
          .SetLocation(BindingSet::kInstance, 0)
          .SetConstants(constants_type, DataVolatility::kPerFrame)};
  std::vector<ShaderParam> inputs = {{ShaderValue::kVec3, 0},
                                     {ShaderValue::kVec2, 1}};
  std::vector<ShaderParam> outputs = {{ShaderValue::kVec4, 0},
                                      {ShaderValue::kFloat, 1}};
  auto shader_code = render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                                      kVertexShaderCode.size());
  auto shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code), bindings, inputs, outputs);
  ASSERT_NE(shader, nullptr);
  EXPECT_THAT(shader->GetBindings(), UnorderedElementsAreArray(bindings));
  EXPECT_THAT(shader->GetInputs(), UnorderedElementsAreArray(inputs));
  EXPECT_THAT(shader->GetOutputs(), UnorderedElementsAreArray(outputs));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(ShaderTest, InvalidBindings) {
  CreateSystem();
  auto shader_code = render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                                      kVertexShaderCode.size());
  auto shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code), {Binding()}, {}, {});

  EXPECT_EQ(shader, nullptr);

  Binding binding = Binding()
                        .SetShaders(ShaderType::kFragment)
                        .SetLocation(BindingSet::kMaterial, 1)
                        .SetTexture();
  shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code), {binding}, {}, {});
  EXPECT_EQ(shader, nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(ShaderTest, RedundantBindingsAreIgnored) {
  CreateSystem();
  Binding binding = Binding()
                        .SetShaders(ShaderType::kVertex)
                        .SetLocation(BindingSet::kMaterial, 1)
                        .SetTexture();
  auto shader_code = render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                                      kVertexShaderCode.size());
  auto shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code), {binding, binding}, {}, {});
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->GetBindings().size(), 1);
  EXPECT_THAT(shader->GetBindings(), Contains(binding));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(ShaderTest, IncompatibleBindings) {
  CreateSystem();
  auto* constants_vec2 =
      render_system_->RegisterConstantsType<Vector2>("constants_vec2");
  auto* constants_vec3 =
      render_system_->RegisterConstantsType<Vector3>("constants_vec3");
  Binding binding =
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kMaterial, 1)
          .SetConstants(constants_vec2, DataVolatility::kStaticWrite);
  auto shader_code = render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                                      kVertexShaderCode.size());

  auto shader =
      render_system_->CreateShader(ShaderType::kVertex, std::move(shader_code),
                                   {binding, binding.SetTexture()}, {}, {});
  EXPECT_EQ(shader, nullptr);

  shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code),
      {binding,
       binding.SetConstants(constants_vec3, DataVolatility::kStaticWrite)},
      {}, {});
  EXPECT_EQ(shader, nullptr);

  shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code),
      {binding,
       binding.SetConstants(constants_vec2, DataVolatility::kStaticReadWrite)},
      {}, {});
  EXPECT_EQ(shader, nullptr);

  shader = render_system_->CreateShader(
      ShaderType::kVertex, std::move(shader_code),
      {binding,
       binding.SetConstants(constants_vec2, DataVolatility::kPerFrame)},
      {}, {});
  EXPECT_EQ(shader, nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
