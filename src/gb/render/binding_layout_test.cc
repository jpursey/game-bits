// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/binding_layout.h"

#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAreArray;

class BindingLayoutTest : public RenderTest {};

TEST_F(BindingLayoutTest, NoBindings) {
  TestBindingLayout test_binding_layout({});
  BindingLayout& binding_layout = test_binding_layout;
  const BindingLayout& const_binding_layout = binding_layout;

  EXPECT_THAT(binding_layout.GetBindings(), IsEmpty());
  EXPECT_NE(binding_layout.GetDefaultViewBindingData(), nullptr);
  EXPECT_EQ(binding_layout.GetDefaultViewBindingData(),
            const_binding_layout.GetDefaultViewBindingData());
  EXPECT_NE(binding_layout.GetDefaultMaterialBindingData(), nullptr);
  EXPECT_EQ(binding_layout.GetDefaultMaterialBindingData(),
            const_binding_layout.GetDefaultMaterialBindingData());
  EXPECT_NE(binding_layout.GetDefaultInstanceBindingData(), nullptr);
  EXPECT_EQ(binding_layout.GetDefaultInstanceBindingData(),
            const_binding_layout.GetDefaultInstanceBindingData());

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(BindingLayoutTest, WithBindings) {
  CreateSystem();
  auto* constants_0 = render_system_->RegisterConstantsType<Vector3>("0");
  auto* constants_1 = render_system_->RegisterConstantsType<Vector2>("1");
  ASSERT_NE(constants_0, nullptr);
  ASSERT_NE(constants_1, nullptr);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kView, 0)
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
  TestBindingLayout test_binding_layout(bindings);
  BindingLayout& binding_layout = test_binding_layout;

  EXPECT_THAT(binding_layout.GetBindings(),
              UnorderedElementsAreArray(bindings));

  EXPECT_TRUE(
      binding_layout.GetDefaultViewBindingData()->IsConstants<Vector3>(0));
  EXPECT_FALSE(
      binding_layout.GetDefaultViewBindingData()->IsConstants<Vector2>(1));

  EXPECT_TRUE(binding_layout.GetDefaultMaterialBindingData()->IsTexture(0));
  EXPECT_FALSE(
      binding_layout.GetDefaultMaterialBindingData()->IsConstants<Vector2>(1));

  EXPECT_TRUE(
      binding_layout.GetDefaultInstanceBindingData()->IsConstants<Vector2>(1));
  EXPECT_FALSE(
      binding_layout.GetDefaultInstanceBindingData()->IsConstants<Vector3>(0));
  EXPECT_FALSE(binding_layout.GetDefaultInstanceBindingData()->IsTexture(0));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
