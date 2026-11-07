// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_view.h"

#include "gb/render/render_test.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class RenderViewTest : public RenderTest {};

TEST_F(RenderViewTest, Properties) {
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
  TestRenderView test_view(&test_binding_layout, 1);
  RenderView& view = test_view;
  const RenderView& const_view = view;

  EXPECT_EQ(const_view.GetBindingLayout(), &test_binding_layout);
  EXPECT_EQ(const_view.GetOrder(), 1);
  ASSERT_NE(view.GetViewBindingData(), nullptr);
  EXPECT_EQ(view.GetViewBindingData(), const_view.GetViewBindingData());
  EXPECT_TRUE(view.GetViewBindingData()->IsConstants<Vector3>(0));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderViewTest, CreateCopiesBindingLayoutDefaults) {
  CreateSystem();
  auto* constants_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(constants_type, nullptr);
  auto* binding_layout = render_system_->RegisterBindingLayout(
      "layout", {Binding()
                     .SetShaders(ShaderType::kVertex)
                     .SetLocation(BindingSet::kView, 0)
                     .SetConstants(constants_type)});
  ASSERT_NE(binding_layout, nullptr);
  binding_layout->GetDefaultViewBindingData()->SetConstants<Vector2>(0, {1, 2});

  auto view = render_system_->CreateView(binding_layout);
  ASSERT_NE(view, nullptr);
  Vector2 constants = {0, 0};
  view->GetViewBindingData()->GetConstants<Vector2>(0, &constants);
  EXPECT_EQ(constants, Vector2({1, 2}));

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderViewTest, FailCreateWithNullBindingLayout) {
  CreateSystem();
  EXPECT_EQ(render_system_->CreateView(nullptr), nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
