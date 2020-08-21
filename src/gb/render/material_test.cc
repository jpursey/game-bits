// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/material.h"

#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class MaterialTest : public RenderTest {};

TEST_F(MaterialTest, CreateAsResourcePtr) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  ASSERT_NE(material_type, nullptr);

  ResourcePtr<Material> material =
      render_system_->CreateMaterial(material_type);
  ASSERT_NE(material, nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTest, CreateInResourceSet) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  ASSERT_NE(material_type, nullptr);

  ResourceSet resource_set;
  Material* material =
      render_system_->CreateMaterial(&resource_set, material_type);
  ASSERT_NE(material, nullptr);
  EXPECT_EQ(resource_set.Get<Shader>(
                material_type->GetVertexShader()->GetResourceId()),
            material_type->GetVertexShader());
  EXPECT_EQ(resource_set.Get<Shader>(
                material_type->GetFragmentShader()->GetResourceId()),
            material_type->GetFragmentShader());
  EXPECT_EQ(resource_set.Get<MaterialType>(material_type->GetResourceId()),
            material_type);
  EXPECT_EQ(resource_set.Get<Material>(material->GetResourceId()), material);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTest, FailCreateWithNullResources) {
  CreateSystem();
  ASSERT_EQ(render_system_->CreateMaterial(nullptr), nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTest, Properties) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  ASSERT_NE(material_type, nullptr);
  auto material = render_system_->CreateMaterial(material_type);
  ASSERT_NE(material, nullptr);
  const Material* const_material = material.Get();

  EXPECT_EQ(const_material->GetType(), material_type);
  EXPECT_NE(const_material->GetMaterialBindingData(), nullptr);
  EXPECT_EQ(const_material->GetMaterialBindingData(),
            material->GetMaterialBindingData());
  EXPECT_NE(const_material->GetDefaultInstanceBindingData(), nullptr);
  EXPECT_EQ(const_material->GetDefaultInstanceBindingData(),
            material->GetDefaultInstanceBindingData());

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MaterialTest, BindingData) {
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
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  ASSERT_NE(texture, nullptr);
  material_type->GetDefaultMaterialBindingData()->SetTexture(1, texture.Get());
  material_type->GetDefaultInstanceBindingData()->SetConstants(
      2, Vector2{1.0f, 2.0f});

  auto material = render_system_->CreateMaterial(material_type);
  ASSERT_NE(material, nullptr);

  auto instance_binding_data = material->CreateInstanceBindingData();
  EXPECT_FALSE(instance_binding_data->IsConstants<Vector3>(0));
  EXPECT_FALSE(instance_binding_data->IsTexture(1));
  EXPECT_TRUE(instance_binding_data->IsConstants<Vector2>(2));
  Vector2 constants = {5.0f, 5.0f};
  instance_binding_data->GetConstants(2, &constants);
  EXPECT_EQ(constants.x, 1.0f);
  EXPECT_EQ(constants.y, 2.0f);

  EXPECT_FALSE(material->GetMaterialBindingData()->IsConstants<Vector3>(0));
  EXPECT_TRUE(material->GetMaterialBindingData()->IsTexture(1));
  EXPECT_FALSE(material->GetMaterialBindingData()->IsConstants<Vector2>(2));

  EXPECT_FALSE(
      material->GetDefaultInstanceBindingData()->IsConstants<Vector3>(0));
  EXPECT_FALSE(material->GetDefaultInstanceBindingData()->IsTexture(1));
  EXPECT_TRUE(
      material->GetDefaultInstanceBindingData()->IsConstants<Vector2>(2));

  material->GetDefaultInstanceBindingData()->SetConstants(2,
                                                          Vector2{3.0f, 4.0f});
  instance_binding_data = material->CreateInstanceBindingData();
  instance_binding_data->GetConstants(2, &constants);
  EXPECT_EQ(constants.x, 3.0f);
  EXPECT_EQ(constants.y, 4.0f);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
