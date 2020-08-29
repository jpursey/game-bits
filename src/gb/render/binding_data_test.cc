// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/binding_data.h"

#include "gb/render/render_test.h"
#include "gb/render/test_binding_data.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Contains;

class BindingDataTest : public RenderTest {};

TEST_F(BindingDataTest, ConstructionNullPipelineNoBindings) {
  TestBindingData binding_data(nullptr, BindingSet::kScene, {});
  EXPECT_EQ(binding_data.GetSet(), BindingSet::kScene);
  EXPECT_EQ(binding_data.GetPipeline(GetAccessToken()), nullptr);
}

TEST_F(BindingDataTest, ConstructionWithPipelineNoBindings) {
  CreateSystem();
  auto pipeline = CreatePipeline({}, {});
  ASSERT_NE(pipeline, nullptr);

  TestBindingData binding_data(pipeline.get(), BindingSet::kMaterial, {});
  EXPECT_EQ(binding_data.GetSet(), BindingSet::kMaterial);
  EXPECT_EQ(binding_data.GetPipeline(GetAccessToken()), pipeline.get());
}

TEST_F(BindingDataTest, ReadWriteConstants) {
  CreateSystem();
  auto* constant_type_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constant_type_1 = render_system_->RegisterConstantsType<Vector2>("1");
  ASSERT_NE(constant_type_0, nullptr);
  ASSERT_NE(constant_type_1, nullptr);

  TestBindingData binding_data(nullptr, BindingSet::kScene,
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

  Vector3 data_0;
  binding_data.SetConstants(0, Vector3{1.0f, 2.0f, 3.0f});
  binding_data.GetConstants(0, &data_0);
  EXPECT_EQ(data_0.x, 1.0f);
  EXPECT_EQ(data_0.y, 2.0f);
  EXPECT_EQ(data_0.z, 3.0f);

  Vector2 data_1;
  binding_data.SetConstants(1, Vector2{4.0f, 5.0f});
  binding_data.GetConstants(1, &data_1);
  EXPECT_EQ(data_1.x, 4.0f);
  EXPECT_EQ(data_1.y, 5.0f);
}

TEST_F(BindingDataTest, ReadWriteTextures) {
  CreateSystem();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  ASSERT_NE(texture, nullptr);

  TestBindingData binding_data(nullptr, BindingSet::kScene,
                               {Binding()
                                    .SetShaders(ShaderType::kFragment)
                                    .SetLocation(BindingSet::kScene, 2)
                                    .SetTexture()});
  EXPECT_FALSE(binding_data.IsTexture(0));
  EXPECT_FALSE(binding_data.IsTexture(1));
  EXPECT_TRUE(binding_data.IsTexture(2));

  binding_data.SetTexture(2, texture.Get());
  EXPECT_EQ(binding_data.GetTexture(2), texture.Get());
}

TEST_F(BindingDataTest, GetDependencies) {
  CreateSystem();
  auto texture_0 =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  auto constant_type_1 = render_system_->RegisterConstantsType<Vector2>("1");
  auto texture_2 =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 16, 16);
  ASSERT_NE(texture_0, nullptr);
  ASSERT_NE(constant_type_1, nullptr);
  ASSERT_NE(texture_2, nullptr);

  TestBindingData binding_data(nullptr, BindingSet::kScene,
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

  ResourceDependencyList dependencies;
  binding_data.SetTexture(0, texture_0.Get());
  binding_data.SetTexture(2, texture_2.Get());
  binding_data.GetDependencies(&dependencies);
  EXPECT_EQ(dependencies.size(), 2);
  EXPECT_THAT(dependencies, Contains(texture_0.Get()));
  EXPECT_THAT(dependencies, Contains(texture_2.Get()));
}

}  // namespace
}  // namespace gb
