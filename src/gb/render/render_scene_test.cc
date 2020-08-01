// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_scene.h"

#include "gb/render/render_test.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class RenderSceneTest : public RenderTest {};

TEST_F(RenderSceneTest, Properties) {
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
  TestRenderScene test_scene(&test_scene_type, 1);
  RenderScene& scene = test_scene;
  const RenderScene& const_scene = scene;

  EXPECT_EQ(const_scene.GetType(), &test_scene_type);
  EXPECT_EQ(const_scene.GetOrder(), 1);
  ASSERT_NE(scene.GetSceneBindingData(), nullptr);
  EXPECT_EQ(scene.GetSceneBindingData(), const_scene.GetSceneBindingData());
  EXPECT_TRUE(scene.GetSceneBindingData()->IsConstants<Vector3>(0));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
