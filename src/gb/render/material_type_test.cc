// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/material_type.h"

#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;

class MaterialTypeTest : public RenderTest {};

TEST_F(MaterialTypeTest, CreateAsResourcePtr) {
  CreateSystem();
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  ResourcePtr<MaterialType> material_type = render_system_->CreateMaterialType(
      binding_layout, vertex_type, vertex_shader.Get(), fragment_shader.Get());
  ASSERT_NE(material_type, nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, CreateInResourceSet) {
  CreateSystem();
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  ResourceSet resource_set;
  MaterialType* material_type = render_system_->CreateMaterialType(
      &resource_set, binding_layout, vertex_type, vertex_shader.Get(),
      fragment_shader.Get());
  ASSERT_NE(material_type, nullptr);
  EXPECT_EQ(resource_set.Get<Shader>(vertex_shader->GetResourceId()),
            vertex_shader.Get());
  EXPECT_EQ(resource_set.Get<Shader>(fragment_shader->GetResourceId()),
            fragment_shader.Get());
  EXPECT_EQ(resource_set.Get<MaterialType>(material_type->GetResourceId()),
            material_type);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithNullResources) {
  CreateSystem();
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {{ShaderValue::kVec3, 0}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  EXPECT_EQ(
      render_system_->CreateMaterialType(
          nullptr, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          binding_layout, nullptr, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               nullptr, fragment_shader.Get()),
            nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               fragment_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(), nullptr),
            nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               vertex_shader.Get()),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithVertexMismatch) {
  CreateSystem();
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {{ShaderValue::kVec2, 0}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {{ShaderValue::kVec3, 1}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {{ShaderValue::kVec3, 0}, {ShaderValue::kVec2, 1}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {{ShaderValue::kVec3, -1}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithShaderInputOutputMismatch) {
  CreateSystem();
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {{ShaderValue::kVec3, 0}},
      {{ShaderValue::kVec3, 0}, {ShaderValue::kVec2, 1}});
  ASSERT_NE(vertex_shader, nullptr);

  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {{ShaderValue::kVec2, 0}, {ShaderValue::kVec2, 1}}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {{ShaderValue::kVec3, 0}, {ShaderValue::kVec3, 1}}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {},
      {{ShaderValue::kVec3, 0},
       {ShaderValue::kVec2, 1},
       {ShaderValue::kFloat, 2}},
      {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithBindingLayoutVertexBindingMismatch) {
  CreateSystem();
  auto* constants_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constants_2 = render_system_->RegisterConstantsType<Vector2>("2");
  auto* constants_other =
      render_system_->RegisterConstantsType<Vector4>("other");
  ASSERT_NE(constants_0, nullptr);
  ASSERT_NE(constants_2, nullptr);
  ASSERT_NE(constants_other, nullptr);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kView, 0)
          .SetConstants(constants_0),
      Binding()
          .SetShaders(ShaderType::kFragment)
          .SetLocation(BindingSet::kMaterial, 1)
          .SetTexture(),
      Binding()
          .SetShaders({ShaderType::kVertex, ShaderType::kFragment})
          .SetLocation(BindingSet::kInstance, 2)
          .SetConstants(constants_2),
  };
  auto* binding_layout =
      render_system_->RegisterBindingLayout("layout", bindings);
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {bindings[0].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {bindings[0].SetTexture()}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {bindings[1]
           .SetShaders(ShaderType::kVertex)
           .SetConstants(constants_other)},
      {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {bindings[2].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {bindings[2].SetTexture()}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithBindingLayoutFragmentBindingMismatch) {
  CreateSystem();
  auto* constants_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constants_2 = render_system_->RegisterConstantsType<Vector2>("2");
  auto* constants_other =
      render_system_->RegisterConstantsType<Vector4>("other");
  ASSERT_NE(constants_0, nullptr);
  ASSERT_NE(constants_2, nullptr);
  ASSERT_NE(constants_other, nullptr);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kView, 0)
          .SetConstants(constants_0),
      Binding()
          .SetShaders(ShaderType::kFragment)
          .SetLocation(BindingSet::kMaterial, 1)
          .SetTexture(),
      Binding()
          .SetShaders({ShaderType::kVertex, ShaderType::kFragment})
          .SetLocation(BindingSet::kInstance, 2)
          .SetConstants(constants_2),
  };
  auto* binding_layout =
      render_system_->RegisterBindingLayout("layout", bindings);
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);

  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[0]
           .SetShaders(ShaderType::kFragment)
           .SetConstants(constants_other)},
      {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[0].SetTexture()}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[1].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[2].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[2].SetTexture()}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithVertexFragmentBindingMismatch) {
  CreateSystem();
  auto* constants_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constants_2 = render_system_->RegisterConstantsType<Vector2>("2");
  auto* constants_other =
      render_system_->RegisterConstantsType<Vector4>("other");
  ASSERT_NE(constants_0, nullptr);
  ASSERT_NE(constants_2, nullptr);
  ASSERT_NE(constants_other, nullptr);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kView, 0)
          .SetConstants(constants_0),
      Binding()
          .SetShaders({ShaderType::kVertex, ShaderType::kFragment})
          .SetLocation(BindingSet::kMaterial, 1)
          .SetTexture(),
      Binding()
          .SetShaders({ShaderType::kVertex, ShaderType::kFragment})
          .SetLocation(BindingSet::kInstance, 2)
          .SetConstants(constants_2),
  };
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      bindings, {}, {});
  ASSERT_NE(vertex_shader, nullptr);

  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[0]
           .SetShaders(ShaderType::kFragment)
           .SetConstants(constants_other)},
      {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[0].SetTexture()}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[1].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[2].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {bindings[2].SetTexture()}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithNullPipeline) {
  CreateSystem();
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  state_.fail_create_pipeline = true;
  ASSERT_EQ(render_system_->CreateMaterialType(binding_layout, vertex_type,
                                               vertex_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, Properties) {
  CreateSystem();
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  auto material_type = render_system_->CreateMaterialType(
      binding_layout, vertex_type, vertex_shader.Get(), fragment_shader.Get());
  ASSERT_NE(material_type, nullptr);
  const MaterialType* const_material_type = material_type.Get();
  EXPECT_EQ(const_material_type->GetConfig(), MaterialConfig());
  EXPECT_EQ(const_material_type->GetVertexShader(), vertex_shader.Get());
  EXPECT_EQ(const_material_type->GetFragmentShader(), fragment_shader.Get());
  EXPECT_EQ(const_material_type->GetVertexType(), vertex_type);
  EXPECT_NE(const_material_type->GetDefaultMaterialBindingData(), nullptr);
  EXPECT_EQ(const_material_type->GetDefaultMaterialBindingData(),
            material_type->GetDefaultMaterialBindingData());
  EXPECT_NE(const_material_type->GetDefaultInstanceBindingData(), nullptr);
  EXPECT_EQ(const_material_type->GetDefaultInstanceBindingData(),
            material_type->GetDefaultInstanceBindingData());
  EXPECT_NE(const_material_type->GetPipeline(GetAccessToken()), nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, ExplicitMaterialConfig) {
  CreateSystem();
  auto* binding_layout = render_system_->RegisterBindingLayout("layout", {});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  const MaterialConfig config = MaterialConfig()
                                    .SetCullMode(CullMode::kFront)
                                    .SetDepthMode(DepthMode::kTest)
                                    .SetBlendMode(BlendMode::kPremultiplied)
                                    .SetColorChannels({})
                                    .SetDepthBias(-1.0f, 0.5f);
  auto material_type = render_system_->CreateMaterialType(
      binding_layout, vertex_type, vertex_shader.Get(), fragment_shader.Get(),
      config);
  ASSERT_NE(material_type, nullptr);
  const MaterialType* const_material_type = material_type.Get();
  EXPECT_EQ(const_material_type->GetConfig(), config);

  auto* test_pipeline = static_cast<TestRenderPipeline*>(
      const_material_type->GetPipeline(GetAccessToken()));
  EXPECT_EQ(test_pipeline->GetConfig(), config);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, BindingData) {
  CreateSystem();
  auto* constants_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constants_2 = render_system_->RegisterConstantsType<Vector2>("2");
  ASSERT_NE(constants_0, nullptr);
  ASSERT_NE(constants_2, nullptr);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kView, 0)
          .SetConstants(constants_0),
      Binding()
          .SetShaders({ShaderType::kVertex, ShaderType::kFragment})
          .SetLocation(BindingSet::kMaterial, 1)
          .SetTexture(),
      Binding()
          .SetShaders({ShaderType::kVertex, ShaderType::kFragment})
          .SetLocation(BindingSet::kInstance, 2)
          .SetConstants(constants_2),
  };
  auto material_type = CreateMaterialType(bindings);
  ASSERT_NE(material_type, nullptr);

  EXPECT_FALSE(
      material_type->GetDefaultMaterialBindingData()->IsConstants<Vector3>(0));
  EXPECT_TRUE(material_type->GetDefaultMaterialBindingData()->IsTexture(1));
  EXPECT_FALSE(
      material_type->GetDefaultMaterialBindingData()->IsConstants<Vector2>(2));

  EXPECT_FALSE(
      material_type->GetDefaultInstanceBindingData()->IsConstants<Vector3>(0));
  EXPECT_FALSE(material_type->GetDefaultInstanceBindingData()->IsTexture(1));
  EXPECT_TRUE(
      material_type->GetDefaultInstanceBindingData()->IsConstants<Vector2>(2));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, CombinesSharedBindings) {
  CreateSystem();
  auto* constants_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(constants_type, nullptr);
  const Binding layout_binding =
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetConstants(constants_type, DataVolatility::kStaticReadWrite);
  auto* binding_layout =
      render_system_->RegisterBindingLayout("layout", {layout_binding});
  ASSERT_NE(binding_layout, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {layout_binding}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {Binding(layout_binding)
           .SetShaders(ShaderType::kFragment)
           .SetConstants(constants_type, DataVolatility::kPerFrame)},
      {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  auto material_type = render_system_->CreateMaterialType(
      binding_layout, vertex_type, vertex_shader.Get(), fragment_shader.Get());
  ASSERT_NE(material_type, nullptr);
  const Binding expected_binding =
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetConstants(constants_type, DataVolatility::kPerFrame);
  EXPECT_THAT(material_type->GetBindings(), ElementsAre(expected_binding));
  auto* test_pipeline = static_cast<TestRenderPipeline*>(
      material_type->GetPipeline(GetAccessToken()));
  EXPECT_THAT(test_pipeline->GetBindings(), ElementsAre(expected_binding));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, CreateCopiesBindingLayoutDefaults) {
  CreateSystem();
  auto* constants_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(constants_type, nullptr);
  auto* binding_layout = render_system_->RegisterBindingLayout(
      "layout", {Binding()
                     .SetShaders(ShaderType::kVertex)
                     .SetLocation(BindingSet::kMaterial, 0)
                     .SetConstants(constants_type),
                 Binding()
                     .SetShaders(ShaderType::kFragment)
                     .SetLocation(BindingSet::kInstance, 1)
                     .SetTexture()});
  ASSERT_NE(binding_layout, nullptr);
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 4, 4);
  ASSERT_NE(texture, nullptr);
  binding_layout->GetDefaultMaterialBindingData()->SetConstants<Vector2>(
      0, {1, 2});
  binding_layout->GetDefaultInstanceBindingData()->SetTexture(1, texture.Get());
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  auto material_type = render_system_->CreateMaterialType(
      binding_layout, vertex_type, vertex_shader.Get(), fragment_shader.Get());
  ASSERT_NE(material_type, nullptr);
  Vector2 constants = {0, 0};
  material_type->GetDefaultMaterialBindingData()->GetConstants<Vector2>(
      0, &constants);
  EXPECT_EQ(constants, Vector2({1, 2}));
  EXPECT_EQ(material_type->GetDefaultInstanceBindingData()->GetTexture(1),
            texture.Get());

  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
