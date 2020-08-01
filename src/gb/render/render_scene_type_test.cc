// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_scene_type.h"

#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAreArray;

class RenderSceneTypeTest : public RenderTest {};

TEST_F(RenderSceneTypeTest, NoBindings) {
  TestRenderSceneType test_scene_type({});
  RenderSceneType& scene_type = test_scene_type;
  const RenderSceneType& const_scene_type = scene_type;

  EXPECT_THAT(scene_type.GetBindings(), IsEmpty());
  EXPECT_NE(scene_type.GetDefaultSceneBindingData(), nullptr);
  EXPECT_EQ(scene_type.GetDefaultSceneBindingData(),
            const_scene_type.GetDefaultSceneBindingData());
  EXPECT_NE(scene_type.GetDefaultMaterialBindingData(), nullptr);
  EXPECT_EQ(scene_type.GetDefaultMaterialBindingData(),
            const_scene_type.GetDefaultMaterialBindingData());
  EXPECT_NE(scene_type.GetDefaultInstanceBindingData(), nullptr);
  EXPECT_EQ(scene_type.GetDefaultInstanceBindingData(),
            const_scene_type.GetDefaultInstanceBindingData());

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderSceneTypeTest, WithBindings) {
  CreateSystem();
  auto* constants_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constants_1 = render_system_->RegisterConstantsType<Vector2>("1");
  ASSERT_NE(constants_0, nullptr);
  ASSERT_NE(constants_1, nullptr);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kScene, 0)
          .SetConstants(constants_0),
      Binding()
          .SetShaders(ShaderType::kFragment)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetTexture(),
      Binding()
          .SetShaders({ShaderType::kVertex, ShaderType::kFragment})
          .SetLocation(BindingSet::kInstance, 1)
          .SetConstants(constants_1),
  };
  TestRenderSceneType test_scene_type(bindings);
  RenderSceneType& scene_type = test_scene_type;

  EXPECT_THAT(scene_type.GetBindings(), UnorderedElementsAreArray(bindings));

  EXPECT_TRUE(scene_type.GetDefaultSceneBindingData()->IsConstants<Vector3>(0));
  EXPECT_FALSE(
      scene_type.GetDefaultSceneBindingData()->IsConstants<Vector2>(1));

  EXPECT_TRUE(scene_type.GetDefaultMaterialBindingData()->IsTexture(0));
  EXPECT_FALSE(
      scene_type.GetDefaultMaterialBindingData()->IsConstants<Vector2>(1));

  EXPECT_TRUE(
      scene_type.GetDefaultInstanceBindingData()->IsConstants<Vector2>(1));
  EXPECT_FALSE(
      scene_type.GetDefaultInstanceBindingData()->IsConstants<Vector3>(0));
  EXPECT_FALSE(scene_type.GetDefaultInstanceBindingData()->IsTexture(0));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
