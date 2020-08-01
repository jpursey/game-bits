// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/local_binding_data.h"

#include "gb/render/binding_data.h"
#include "gb/render/render_test.h"
#include "gb/render/test_binding_data.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Contains;

class LocalBindingDataTest : public RenderTest {};

TEST_F(LocalBindingDataTest, ConstructionNoBindings) {
  LocalBindingData binding_data(GetAccessToken(), BindingSet::kScene, {});
  EXPECT_EQ(binding_data.GetSet(), BindingSet::kScene);
  EXPECT_EQ(binding_data.GetPipeline(GetAccessToken()), nullptr);
}

TEST_F(LocalBindingDataTest, ReadWriteConstants) {
  CreateSystem();
  auto* constant_type_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constant_type_1 = render_system_->RegisterConstantsType<Vector2>("1");
  ASSERT_NE(constant_type_0, nullptr);
  ASSERT_NE(constant_type_1, nullptr);

  LocalBindingData binding_data(GetAccessToken(), BindingSet::kScene,
                                {Binding()
                                     .SetShaders(ShaderType::kVertex)
                                     .SetLocation(BindingSet::kScene, 0)
                                     .SetConstants(constant_type_0),
                                 Binding()
                                     .SetShaders(ShaderType::kVertex)
                                     .SetLocation(BindingSet::kScene, 1)
                                     .SetConstants(constant_type_1)});
  EXPECT_TRUE(binding_data.IsConstants<Vector3>(0));
  EXPECT_FALSE(binding_data.IsConstants<Vector2>(0));
  EXPECT_TRUE(binding_data.IsConstants<Vector2>(1));
  EXPECT_FALSE(binding_data.IsConstants<Vector3>(1));

  Vector3 data_0 = {5.0f, 5.0f, 5.0f};
  binding_data.GetConstants(0, &data_0);
  EXPECT_EQ(data_0.x, 0.0f);
  EXPECT_EQ(data_0.y, 0.0f);
  EXPECT_EQ(data_0.z, 0.0f);

  binding_data.SetConstants(0, Vector3{1.0f, 2.0f, 3.0f});
  binding_data.GetConstants(0, &data_0);
  EXPECT_EQ(data_0.x, 1.0f);
  EXPECT_EQ(data_0.y, 2.0f);
  EXPECT_EQ(data_0.z, 3.0f);

  Vector2 data_1 = {5.0f, 5.0f};
  binding_data.GetConstants(1, &data_1);
  EXPECT_EQ(data_1.x, 0.0f);
  EXPECT_EQ(data_1.y, 0.0f);

  binding_data.SetConstants(1, Vector2{4.0f, 5.0f});
  binding_data.GetConstants(1, &data_1);
  EXPECT_EQ(data_1.x, 4.0f);
  EXPECT_EQ(data_1.y, 5.0f);
}

TEST_F(LocalBindingDataTest, ReadWriteTextures) {
  CreateSystem();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  ASSERT_NE(texture, nullptr);

  LocalBindingData binding_data(GetAccessToken(), BindingSet::kScene,
                                {Binding()
                                     .SetShaders(ShaderType::kFragment)
                                     .SetLocation(BindingSet::kScene, 2)
                                     .SetTexture()});
  EXPECT_FALSE(binding_data.IsTexture(0));
  EXPECT_FALSE(binding_data.IsTexture(1));
  EXPECT_TRUE(binding_data.IsTexture(2));

  EXPECT_EQ(binding_data.GetTexture(2), nullptr);
  binding_data.SetTexture(2, texture.Get());
  EXPECT_EQ(binding_data.GetTexture(2), texture.Get());
}

TEST_F(LocalBindingDataTest, Validation) {
  CreateSystem();
  auto texture_0 =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  auto constant_type_2 = render_system_->RegisterConstantsType<Vector2>("2");
  ASSERT_NE(texture_0, nullptr);
  ASSERT_NE(constant_type_2, nullptr);
  LocalBindingData binding_data(GetAccessToken(), BindingSet::kScene,
                                {Binding()
                                     .SetShaders(ShaderType::kFragment)
                                     .SetLocation(BindingSet::kScene, 0)
                                     .SetTexture(),
                                 Binding()
                                     .SetShaders(ShaderType::kVertex)
                                     .SetLocation(BindingSet::kScene, 2)
                                     .SetConstants(constant_type_2)});

  // Success
  EXPECT_TRUE(binding_data.IsTexture(0));
  EXPECT_TRUE(binding_data.IsConstants<Vector2>(2));

  // Wrong type
  EXPECT_FALSE(binding_data.IsConstants<Vector2>(0));
  EXPECT_FALSE(binding_data.IsConstants<Vector3>(2));
  EXPECT_FALSE(binding_data.IsTexture(2));

  // Unassigned binding index
  EXPECT_FALSE(binding_data.IsTexture(1));
  EXPECT_FALSE(binding_data.IsConstants<Vector2>(1));

  // Out of range
  EXPECT_FALSE(binding_data.IsTexture(-1));
  EXPECT_FALSE(binding_data.IsTexture(3));
  EXPECT_FALSE(binding_data.IsConstants<Vector3>(-1));
  EXPECT_FALSE(binding_data.IsConstants<Vector3>(3));
}

TEST_F(LocalBindingDataTest, GetDependencies) {
  CreateSystem();
  auto texture_0 =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  auto constant_type_1 = render_system_->RegisterConstantsType<Vector2>("1");
  auto texture_2 =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  ASSERT_NE(texture_0, nullptr);
  ASSERT_NE(constant_type_1, nullptr);
  ASSERT_NE(texture_2, nullptr);

  LocalBindingData binding_data(GetAccessToken(), BindingSet::kScene,
                                {Binding()
                                     .SetShaders(ShaderType::kFragment)
                                     .SetLocation(BindingSet::kScene, 0)
                                     .SetTexture(),
                                 Binding()
                                     .SetShaders(ShaderType::kVertex)
                                     .SetLocation(BindingSet::kScene, 1)
                                     .SetConstants(constant_type_1),
                                 Binding()
                                     .SetShaders(ShaderType::kFragment)
                                     .SetLocation(BindingSet::kScene, 2)
                                     .SetTexture()});
  EXPECT_TRUE(binding_data.IsTexture(0));
  EXPECT_TRUE(binding_data.IsConstants<Vector2>(1));
  EXPECT_TRUE(binding_data.IsTexture(2));

  // Get single dependency
  ResourceDependencyList dependencies;
  binding_data.SetTexture(0, texture_0.Get());
  binding_data.GetDependencies(&dependencies);
  EXPECT_EQ(dependencies.size(), 1);
  EXPECT_THAT(dependencies, Contains(texture_0.Get()));

  // Only adds dependencies (does not reset dependencies)
  dependencies.clear();
  dependencies.push_back(texture_2.Get());
  binding_data.GetDependencies(&dependencies);
  EXPECT_EQ(dependencies.size(), 2);
  EXPECT_THAT(dependencies, Contains(texture_0.Get()));
  EXPECT_THAT(dependencies, Contains(texture_2.Get()));

  // Multiple dependencies
  dependencies.clear();
  binding_data.SetTexture(2, texture_2.Get());
  binding_data.GetDependencies(&dependencies);
  EXPECT_EQ(dependencies.size(), 2);
  EXPECT_THAT(dependencies, Contains(texture_0.Get()));
  EXPECT_THAT(dependencies, Contains(texture_2.Get()));
}

TEST_F(LocalBindingDataTest, CopyConstruction) {
  CreateSystem();
  auto texture_0 =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  auto constant_type_2 = render_system_->RegisterConstantsType<Vector2>("2");
  ASSERT_NE(texture_0, nullptr);
  ASSERT_NE(constant_type_2, nullptr);
  LocalBindingData src_binding_data(GetAccessToken(), BindingSet::kScene,
                                    {Binding()
                                         .SetShaders(ShaderType::kFragment)
                                         .SetLocation(BindingSet::kScene, 0)
                                         .SetTexture(),
                                     Binding()
                                         .SetShaders(ShaderType::kVertex)
                                         .SetLocation(BindingSet::kScene, 2)
                                         .SetConstants(constant_type_2)});
  src_binding_data.SetTexture(0, texture_0.Get());
  src_binding_data.SetConstants(2, Vector2{1.0f, 2.0f});

  LocalBindingData dst_binding_data(GetAccessToken(), src_binding_data);

  EXPECT_TRUE(dst_binding_data.IsTexture(0));
  EXPECT_TRUE(dst_binding_data.IsConstants<Vector2>(2));
  EXPECT_EQ(dst_binding_data.GetTexture(0), texture_0.Get());
  Vector2 constants = {5.0f, 5.0f};
  dst_binding_data.GetConstants(2, &constants);
  EXPECT_EQ(constants.x, 1.0f);
  EXPECT_EQ(constants.y, 2.0f);

  EXPECT_TRUE(src_binding_data.IsTexture(0));
  EXPECT_TRUE(src_binding_data.IsConstants<Vector2>(2));
  EXPECT_EQ(src_binding_data.GetTexture(0), texture_0.Get());
  constants = {5.0f, 5.0f};
  src_binding_data.GetConstants(2, &constants);
  EXPECT_EQ(constants.x, 1.0f);
  EXPECT_EQ(constants.y, 2.0f);
}

TEST_F(LocalBindingDataTest, CopyTo) {
  CreateSystem();
  auto texture_0 =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  auto constant_type_1 = render_system_->RegisterConstantsType<Vector3>("3");
  auto constant_type_2 = render_system_->RegisterConstantsType<Vector2>("2");
  ASSERT_NE(texture_0, nullptr);
  ASSERT_NE(constant_type_1, nullptr);
  ASSERT_NE(constant_type_2, nullptr);

  LocalBindingData src_binding_data(GetAccessToken(), BindingSet::kScene,
                                    {Binding()
                                         .SetShaders(ShaderType::kFragment)
                                         .SetLocation(BindingSet::kScene, 0)
                                         .SetTexture(),
                                     Binding()
                                         .SetShaders(ShaderType::kVertex)
                                         .SetLocation(BindingSet::kScene, 2)
                                         .SetConstants(constant_type_2)});
  src_binding_data.SetTexture(0, texture_0.Get());
  src_binding_data.SetConstants(2, Vector2{1.0f, 2.0f});

  TestBindingData dst_binding_data(nullptr, BindingSet::kScene,
                                   {Binding()
                                        .SetShaders(ShaderType::kFragment)
                                        .SetLocation(BindingSet::kScene, 0)
                                        .SetTexture(),
                                    Binding()
                                        .SetShaders(ShaderType::kVertex)
                                        .SetLocation(BindingSet::kScene, 1)
                                        .SetConstants(constant_type_1),
                                    Binding()
                                        .SetShaders(ShaderType::kVertex)
                                        .SetLocation(BindingSet::kScene, 2)
                                        .SetConstants(constant_type_2)});
  dst_binding_data.SetConstants(1, Vector3{-1.0f, -2.0f, -3.0f});
  dst_binding_data.SetConstants(2, Vector2{3.0f, 4.0f});

  src_binding_data.CopyTo(&dst_binding_data);

  EXPECT_TRUE(dst_binding_data.IsTexture(0));
  EXPECT_TRUE(dst_binding_data.IsConstants<Vector3>(1));
  EXPECT_TRUE(dst_binding_data.IsConstants<Vector2>(2));

  EXPECT_EQ(dst_binding_data.GetTexture(0), texture_0.Get());

  Vector3 constants_1 = {5.0f, 5.0f, 5.0f};
  dst_binding_data.GetConstants(1, &constants_1);
  EXPECT_EQ(constants_1.x, -1.0f);
  EXPECT_EQ(constants_1.y, -2.0f);
  EXPECT_EQ(constants_1.z, -3.0f);

  Vector2 constants_2 = {5.0f, 5.0f};
  dst_binding_data.GetConstants(2, &constants_2);
  EXPECT_EQ(constants_2.x, 1.0f);
  EXPECT_EQ(constants_2.y, 2.0f);
}

}  // namespace
}  // namespace gb
