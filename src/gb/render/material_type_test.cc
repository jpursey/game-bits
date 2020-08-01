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

class MaterialTypeTest : public RenderTest {};

TEST_F(MaterialTypeTest, CreateAsResourcePtr) {
  CreateSystem();
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  ResourcePtr<MaterialType> material_type = render_system_->CreateMaterialType(
      scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get());
  ASSERT_NE(material_type, nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, CreateInResourceSet) {
  CreateSystem();
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  ResourceSet resource_set;
  MaterialType* material_type = render_system_->CreateMaterialType(
      &resource_set, scene_type, vertex_type, vertex_shader.Get(),
      fragment_shader.Get());
  ASSERT_NE(material_type, nullptr);
  EXPECT_EQ(resource_set.Get<ShaderCode>(vertex_shader_code->GetResourceId()),
            vertex_shader_code.Get());
  EXPECT_EQ(resource_set.Get<Shader>(vertex_shader->GetResourceId()),
            vertex_shader.Get());
  EXPECT_EQ(resource_set.Get<ShaderCode>(fragment_shader_code->GetResourceId()),
            fragment_shader_code.Get());
  EXPECT_EQ(resource_set.Get<Shader>(fragment_shader->GetResourceId()),
            fragment_shader.Get());
  EXPECT_EQ(resource_set.Get<MaterialType>(material_type->GetResourceId()),
            material_type);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithNullResources) {
  CreateSystem();
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {},
      {{ShaderValue::kVec3, 0}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  EXPECT_EQ(
      render_system_->CreateMaterialType(
          nullptr, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, nullptr, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(scene_type, vertex_type, nullptr,
                                               fragment_shader.Get()),
            nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(scene_type, vertex_type,
                                               fragment_shader.Get(),
                                               fragment_shader.Get()),
            nullptr);
  EXPECT_EQ(render_system_->CreateMaterialType(scene_type, vertex_type,
                                               vertex_shader.Get(), nullptr),
            nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), vertex_shader.Get()),
      nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithVertexMismatch) {
  CreateSystem();
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  ASSERT_NE(vertex_shader_code, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {},
      {{ShaderValue::kVec2, 0}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  vertex_shader = render_system_->CreateShader(ShaderType::kVertex,
                                               vertex_shader_code.Get(), {},
                                               {{ShaderValue::kVec3, 1}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {},
      {{ShaderValue::kVec3, 0}, {ShaderValue::kVec2, 1}}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithShaderInputOutputMismatch) {
  CreateSystem();
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {},
      {{ShaderValue::kVec3, 0}},
      {{ShaderValue::kVec3, 0}, {ShaderValue::kVec2, 1}});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  ASSERT_NE(fragment_shader_code, nullptr);

  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {},
      {{ShaderValue::kVec2, 0}, {ShaderValue::kVec2, 1}}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {},
      {{ShaderValue::kVec3, 0}, {ShaderValue::kVec3, 1}}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(ShaderType::kFragment,
                                                 fragment_shader_code.Get(), {},
                                                 {{ShaderValue::kVec3, 0},
                                                  {ShaderValue::kVec2, 1},
                                                  {ShaderValue::kFloat, 2}},
                                                 {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithSceneVertexBindingMismatch) {
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
          .SetLocation(BindingSet::kScene, 0)
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
  auto* scene_type = render_system_->RegisterSceneType("scene", bindings);
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  ASSERT_NE(vertex_shader_code, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(),
      {bindings[0].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {bindings[0].SetTexture()},
      {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(),
      {bindings[1]
           .SetShaders(ShaderType::kVertex)
           .SetConstants(constants_other)},
      {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(),
      {bindings[2].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {bindings[2].SetTexture()},
      {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithSceneFragmentBindingMismatch) {
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
          .SetLocation(BindingSet::kScene, 0)
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
  auto* scene_type = render_system_->RegisterSceneType("scene", bindings);
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  ASSERT_NE(fragment_shader_code, nullptr);

  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[0]
           .SetShaders(ShaderType::kFragment)
           .SetConstants(constants_other)},
      {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[0].SetTexture()}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[1].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[2].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[2].SetTexture()}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
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
          .SetLocation(BindingSet::kScene, 0)
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
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), bindings, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  ASSERT_NE(fragment_shader_code, nullptr);

  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[0]
           .SetShaders(ShaderType::kFragment)
           .SetConstants(constants_other)},
      {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[0].SetTexture()}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[1].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[2].SetConstants(constants_other)}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(),
      {bindings[2].SetTexture()}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);
  EXPECT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, FailCreateWithNullPipeline) {
  CreateSystem();
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  state_.fail_create_pipeline = true;
  ASSERT_EQ(
      render_system_->CreateMaterialType(
          scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get()),
      nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTypeTest, Properties) {
  CreateSystem();
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  ASSERT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto vertex_shader = render_system_->CreateShader(
      ShaderType::kVertex, vertex_shader_code.Get(), {}, {}, {});
  ASSERT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto fragment_shader = render_system_->CreateShader(
      ShaderType::kFragment, fragment_shader_code.Get(), {}, {}, {});
  ASSERT_NE(fragment_shader, nullptr);

  auto material_type = render_system_->CreateMaterialType(
      scene_type, vertex_type, vertex_shader.Get(), fragment_shader.Get());
  ASSERT_NE(material_type, nullptr);
  const MaterialType* const_material_type = material_type.Get();
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

TEST_F(MaterialTypeTest, BindingData) {
  CreateSystem();
  auto* constants_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constants_2 = render_system_->RegisterConstantsType<Vector2>("2");
  ASSERT_NE(constants_0, nullptr);
  ASSERT_NE(constants_2, nullptr);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kScene, 0)
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

}  // namespace
}  // namespace gb
