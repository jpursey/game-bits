// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_pass.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gb/render/render_assert.h"
#include "gb/render/render_test.h"
#include "gb/render/test_render_pass.h"
#include "gb/test/log_error_guard.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

auto LogTextHas(std::string_view text) {
  return Field(&LogRecorder::Message::text, HasSubstr(std::string(text)));
}

class RenderPassTest : public RenderTest {
 protected:
  // Creates the system with a frame of kFrameWidth x kFrameHeight.
  static constexpr int kFrameWidth = 64;
  static constexpr int kFrameHeight = 32;
  void CreatePassSystem() {
    state_.frame_dimensions = {kFrameWidth, kFrameHeight};
    CreateSystem();
  }

  ResourcePtr<RenderTarget> CreateColor(
      const RenderTargetSize& size = RenderTargetSize::Frame()) {
    auto target =
        render_system_->CreateRenderTarget(TextureFormat::kRgba8Srgb, size);
    EXPECT_NE(target, nullptr);
    return target;
  }
  ResourcePtr<RenderTarget> CreateDepth(
      const RenderTargetSize& size = RenderTargetSize::Frame()) {
    auto target =
        render_system_->CreateRenderTarget(TextureFormat::kDepth, size);
    EXPECT_NE(target, nullptr);
    return target;
  }

  // Creates a pass of the order, drawing into a color target of its own.
  std::unique_ptr<RenderPass> CreateColorPass(int pass_order) {
    auto pass = render_system_->CreatePass(
        RenderPassConfig().SetColor(CreateColor().Get()), pass_order);
    EXPECT_NE(pass, nullptr);
    return pass;
  }

  // Returns the passes of the last frame begun, in draw order, with null for
  // the frame.
  std::vector<RenderPass*> GetFramePasses() {
    std::vector<RenderPass*> passes;
    for (const auto& item : state_.passes) {
      passes.push_back(item.pass);
    }
    return passes;
  }

  // Creates a material whose view, material, and instance data each bind a
  // texture at index 0, and a mesh for it.
  void CreateTexturedMaterial() {
    const std::vector<Binding> bindings = {
        Binding()
            .SetShaders(ShaderType::kFragment)
            .SetLocation(BindingSet::kView, 0)
            .SetTexture(),
        Binding()
            .SetShaders(ShaderType::kFragment)
            .SetLocation(BindingSet::kMaterial, 0)
            .SetTexture(),
        Binding()
            .SetShaders(ShaderType::kFragment)
            .SetLocation(BindingSet::kInstance, 0)
            .SetTexture(),
    };
    material_ = CreateMaterial(bindings);
    ASSERT_NE(material_, nullptr);
    binding_layout_ = material_->GetType()->GetBindingLayout();
    mesh_ = render_system_->CreateMesh(&temp_resource_set_,
                                       material_->GetType()->GetVertexType(),
                                       DataVolatility::kStaticWrite, 3, 1);
    ASSERT_NE(mesh_, nullptr);
    instance_data_ = material_->CreateInstanceBindingData();
    ASSERT_NE(instance_data_, nullptr);
  }

  Material* material_ = nullptr;
  BindingLayout* binding_layout_ = nullptr;
  Mesh* mesh_ = nullptr;
  std::unique_ptr<BindingData> instance_data_;
};

//------------------------------------------------------------------------------
// Config and creation
//------------------------------------------------------------------------------

TEST_F(RenderPassTest, ConfigSetters) {
  CreatePassSystem();
  auto color = CreateColor();
  auto depth = CreateDepth();
  RenderPassConfig config = RenderPassConfig()
                                .SetColor(color.Get(), true)
                                .SetDepth(depth.Get())
                                .SetClearColor({1, 2, 3, 4})
                                .SetMultisample(true);
  EXPECT_EQ(config.color, color.Get());
  EXPECT_TRUE(config.keep_color);
  EXPECT_EQ(config.depth, depth.Get());
  EXPECT_FALSE(config.keep_depth);
  EXPECT_EQ(config.clear_color, Pixel(1, 2, 3, 4));
  EXPECT_TRUE(config.multisample);

  config.SetDepth(nullptr, true);
  EXPECT_EQ(config.depth, nullptr);
  EXPECT_TRUE(config.keep_depth);
}

TEST_F(RenderPassTest, CreateWithColorAndDepth) {
  CreatePassSystem();
  auto color = CreateColor();
  auto depth = CreateDepth();
  auto pass = render_system_->CreatePass(RenderPassConfig()
                                             .SetColor(color.Get(), true)
                                             .SetDepth(depth.Get())
                                             .SetClearColor({1, 2, 3, 4}),
                                         5);
  ASSERT_NE(pass, nullptr);
  EXPECT_EQ(pass->GetOrder(), 5);
  EXPECT_EQ(pass->GetColorTarget(), color.Get());
  EXPECT_EQ(pass->GetDepthTarget(), depth.Get());
  EXPECT_TRUE(pass->KeepsColor());
  EXPECT_FALSE(pass->KeepsDepth());
  EXPECT_FALSE(pass->IsMultisampled());
  EXPECT_EQ(pass->GetClearColor(), Pixel(1, 2, 3, 4));
  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderPassTest, CreateWithOneTarget) {
  CreatePassSystem();
  auto color = CreateColor();
  auto color_pass =
      render_system_->CreatePass(RenderPassConfig().SetColor(color.Get()));
  ASSERT_NE(color_pass, nullptr);
  EXPECT_EQ(color_pass->GetOrder(), 0);
  EXPECT_EQ(color_pass->GetColorTarget(), color.Get());
  EXPECT_EQ(color_pass->GetDepthTarget(), nullptr);

  auto depth = CreateDepth(RenderTargetSize::Pixels(256, 256));
  auto depth_pass = render_system_->CreatePass(
      RenderPassConfig().SetDepth(depth.Get(), true));
  ASSERT_NE(depth_pass, nullptr);
  EXPECT_EQ(depth_pass->GetColorTarget(), nullptr);
  EXPECT_EQ(depth_pass->GetDepthTarget(), depth.Get());
  EXPECT_TRUE(depth_pass->KeepsDepth());
}

TEST_F(RenderPassTest, CreateMultisampled) {
  CreatePassSystem();
  auto color = CreateColor();
  auto depth = CreateDepth();
  auto pass = render_system_->CreatePass(RenderPassConfig()
                                             .SetColor(color.Get())
                                             .SetDepth(depth.Get())
                                             .SetMultisample(true));
  ASSERT_NE(pass, nullptr);
  EXPECT_TRUE(pass->IsMultisampled());
}

TEST_F(RenderPassTest, KeepsItsTargetsAlive) {
  CreatePassSystem();
  auto color = CreateColor();
  RenderTarget* const color_target = color.Get();
  auto pass =
      render_system_->CreatePass(RenderPassConfig().SetColor(color_target));
  ASSERT_NE(pass, nullptr);
  color.Reset();
  EXPECT_EQ(pass->GetColorTarget(), color_target);
  EXPECT_EQ(color_target->GetWidth(), kFrameWidth);
}

TEST_F(RenderPassTest, SetClearColor) {
  CreatePassSystem();
  auto pass = CreateColorPass(0);
  ASSERT_NE(pass, nullptr);
  EXPECT_EQ(pass->GetClearColor(), Pixel(0, 0, 0, 0));
  pass->SetClearColor({10, 20, 30, 255});
  EXPECT_EQ(pass->GetClearColor(), Pixel(10, 20, 30, 255));
}

TEST_F(RenderPassTest, CreateNeedsATarget) {
  CreatePassSystem();
  LogErrorGuard log_error_guard;
  EXPECT_EQ(render_system_->CreatePass(RenderPassConfig()), nullptr);
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(LogTextHas("neither a color nor a depth target")));
}

TEST_F(RenderPassTest, CreateValidatesTargetFormats) {
  CreatePassSystem();
  auto color = CreateColor();
  auto depth = CreateDepth();
  LogErrorGuard log_error_guard;
  EXPECT_EQ(
      render_system_->CreatePass(RenderPassConfig().SetColor(depth.Get())),
      nullptr);
  EXPECT_EQ(
      render_system_->CreatePass(RenderPassConfig().SetDepth(color.Get())),
      nullptr);
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(LogTextHas("color target is a depth target"),
                          LogTextHas("depth target is not in kDepth")));
}

TEST_F(RenderPassTest, CreateNeedsTargetsOfTheSameSize) {
  CreatePassSystem();
  auto frame_color = CreateColor(RenderTargetSize::Frame());
  auto frame_depth = CreateDepth(RenderTargetSize::Frame());
  auto half_depth = CreateDepth(RenderTargetSize::Frame(0.5f));
  auto pixels_depth =
      CreateDepth(RenderTargetSize::Pixels(kFrameWidth, kFrameHeight));
  auto pixels_color =
      CreateColor(RenderTargetSize::Pixels(kFrameWidth, kFrameHeight));
  auto other_pixels_color =
      CreateColor(RenderTargetSize::Pixels(kFrameWidth, kFrameHeight / 2));

  EXPECT_NE(render_system_->CreatePass(RenderPassConfig()
                                           .SetColor(frame_color.Get())
                                           .SetDepth(frame_depth.Get())),
            nullptr);
  EXPECT_NE(render_system_->CreatePass(RenderPassConfig()
                                           .SetColor(pixels_color.Get())
                                           .SetDepth(pixels_depth.Get())),
            nullptr);

  // A target the frame's size now, in pixels, isn't the same size as one that
  // follows the frame, as only one of them changes with it.
  LogErrorGuard log_error_guard;
  EXPECT_EQ(render_system_->CreatePass(RenderPassConfig()
                                           .SetColor(frame_color.Get())
                                           .SetDepth(half_depth.Get())),
            nullptr);
  EXPECT_EQ(render_system_->CreatePass(RenderPassConfig()
                                           .SetColor(frame_color.Get())
                                           .SetDepth(pixels_depth.Get())),
            nullptr);
  EXPECT_EQ(render_system_->CreatePass(RenderPassConfig()
                                           .SetColor(other_pixels_color.Get())
                                           .SetDepth(pixels_depth.Get())),
            nullptr);
  EXPECT_THAT(log_error_guard.TakeMessages(), SizeIs(3));
}

TEST_F(RenderPassTest, MultisampledPassCannotKeepItsTargets) {
  CreatePassSystem();
  auto color = CreateColor();
  auto depth = CreateDepth();
  LogErrorGuard log_error_guard;
  EXPECT_EQ(
      render_system_->CreatePass(
          RenderPassConfig().SetColor(color.Get(), true).SetMultisample(true)),
      nullptr);
  EXPECT_EQ(
      render_system_->CreatePass(
          RenderPassConfig().SetDepth(depth.Get(), true).SetMultisample(true)),
      nullptr);
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(LogTextHas("multisampled and keeps its targets"),
                          LogTextHas("multisampled and keeps its targets")));
}

TEST_F(RenderPassTest, FailCreate) {
  CreatePassSystem();
  auto color = CreateColor();
  state_.fail_create_pass = true;
  LogErrorGuard log_error_guard;
  EXPECT_EQ(
      render_system_->CreatePass(RenderPassConfig().SetColor(color.Get())),
      nullptr);
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(LogTextHas("Failed to create render pass")));

  // A pass that failed to be created isn't drawn.
  state_.fail_create_pass = false;
  DrawFrame();
  EXPECT_THAT(GetFramePasses(), ElementsAre(nullptr));
}

//------------------------------------------------------------------------------
// Pass order
//------------------------------------------------------------------------------

TEST_F(RenderPassTest, FrameAloneWithoutPasses) {
  CreatePassSystem();
  DrawFrame();
  EXPECT_THAT(GetFramePasses(), ElementsAre(nullptr));
  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderPassTest, PassesAreDrawnInPassOrderBeforeTheFrame) {
  CreatePassSystem();
  auto pass_2 = CreateColorPass(2);
  auto pass_0 = CreateColorPass(0);
  auto pass_negative = CreateColorPass(-1);
  auto pass_1 = CreateColorPass(1);
  DrawFrame();
  EXPECT_THAT(GetFramePasses(),
              ElementsAre(pass_negative.get(), pass_0.get(), pass_1.get(),
                          pass_2.get(), nullptr));
  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderPassTest, PassesOfTheSameOrderAreDrawnInCreationOrder) {
  CreatePassSystem();
  auto first = CreateColorPass(1);
  auto earlier = CreateColorPass(0);
  auto second = CreateColorPass(1);
  auto third = CreateColorPass(1);
  DrawFrame();
  EXPECT_THAT(GetFramePasses(),
              ElementsAre(earlier.get(), first.get(), second.get(), third.get(),
                          nullptr));
}

TEST_F(RenderPassTest, DestroyedPassesAreNotDrawn) {
  CreatePassSystem();
  auto pass_0 = CreateColorPass(0);
  auto pass_1 = CreateColorPass(1);
  DrawFrame();
  EXPECT_THAT(GetFramePasses(),
              ElementsAre(pass_0.get(), pass_1.get(), nullptr));

  pass_0.reset();
  DrawFrame();
  EXPECT_THAT(GetFramePasses(), ElementsAre(pass_1.get(), nullptr));
}

TEST_F(RenderPassTest, PassCreatedDuringAFrameIsDrawnFromTheNext) {
  CreatePassSystem();
  ASSERT_TRUE(render_system_->BeginFrame());
  auto pass = CreateColorPass(0);
  render_system_->EndFrame();
  EXPECT_THAT(GetFramePasses(), ElementsAre(nullptr));

  DrawFrame();
  EXPECT_THAT(GetFramePasses(), ElementsAre(pass.get(), nullptr));
}

//------------------------------------------------------------------------------
// Views
//------------------------------------------------------------------------------

TEST_F(RenderPassTest, ViewsInPassesAndTheFrame) {
  CreatePassSystem();
  CreateTexturedMaterial();
  auto pass = CreateColorPass(0);

  auto pass_view = render_system_->CreateView(pass.get(), binding_layout_, 3);
  ASSERT_NE(pass_view, nullptr);
  EXPECT_EQ(pass_view->GetPass(), pass.get());
  EXPECT_EQ(pass_view->GetOrder(), 3);
  EXPECT_EQ(pass_view->GetBindingLayout(), binding_layout_);

  auto frame_view = render_system_->CreateView(nullptr, binding_layout_);
  ASSERT_NE(frame_view, nullptr);
  EXPECT_EQ(frame_view->GetPass(), nullptr);

  auto default_view = render_system_->CreateView(binding_layout_);
  ASSERT_NE(default_view, nullptr);
  EXPECT_EQ(default_view->GetPass(), nullptr);
}

TEST_F(RenderPassTest, ViewInAPassCopiesBindingLayoutDefaults) {
  CreatePassSystem();
  CreateTexturedMaterial();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 4, 4);
  ASSERT_NE(texture, nullptr);
  binding_layout_->GetDefaultViewBindingData()->SetTexture(0, texture.Get());
  auto pass = CreateColorPass(0);

  auto view = render_system_->CreateView(pass.get(), binding_layout_);
  ASSERT_NE(view, nullptr);
  EXPECT_EQ(view->GetViewBindingData()->GetTexture(0), texture.Get());
}

TEST_F(RenderPassTest, DestroyingAPassAfterItsViews) {
  CreatePassSystem();
  CreateTexturedMaterial();
  auto pass = CreateColorPass(0);
  auto view_a = render_system_->CreateView(pass.get(), binding_layout_);
  auto view_b = render_system_->CreateView(pass.get(), binding_layout_);
  ASSERT_NE(view_a, nullptr);
  ASSERT_NE(view_b, nullptr);
  view_a.reset();
  view_b.reset();
  pass.reset();
  DrawFrame();
  EXPECT_THAT(GetFramePasses(), ElementsAre(nullptr));
}

TEST_F(RenderPassTest, CreateViewInAnotherSystemsPassFails) {
  CreatePassSystem();
  CreateTexturedMaterial();
  TestRenderPass other_pass(RenderPassConfig(), 0);
  LogErrorGuard log_error_guard;
  EXPECT_EQ(render_system_->CreateView(&other_pass, binding_layout_), nullptr);
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(LogTextHas("from another render system")));
}

TEST_F(RenderPassTest, DrawingAViewBeforeItsPassIsDrawnFails) {
  if (!kRenderAssertsCompiled) {
    GTEST_SKIP() << "Render asserts are only compiled in Debug";
  }
  const bool was_enabled = IsRenderAssertEnabled();
  SetRenderAssertEnabled(true);
  CreatePassSystem();
  CreateTexturedMaterial();
  ASSERT_TRUE(render_system_->BeginFrame());
  auto pass = CreateColorPass(0);
  auto view = render_system_->CreateView(pass.get(), binding_layout_);
  ASSERT_NE(view, nullptr);
  EXPECT_DEATH(
      render_system_->Draw(view.get(), mesh_, material_, instance_data_.get()),
      "in_frame_");
  DrawList draw_list;
  draw_list.SetMesh(mesh_, material_, instance_data_.get());
  draw_list.Draw();
  EXPECT_DEATH(render_system_->Draw(view.get(), draw_list), "in_frame_");
  render_system_->EndFrame();
  SetRenderAssertEnabled(was_enabled);

  // From the next frame, it is drawn.
  ASSERT_TRUE(render_system_->BeginFrame());
  render_system_->Draw(view.get(), mesh_, material_, instance_data_.get());
  render_system_->EndFrame();
  ASSERT_THAT(state_.passes, SizeIs(2));
  EXPECT_THAT(state_.passes[0].draws, SizeIs(1));
  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderPassTest, DestroyingAPassWithViewsLeftFails) {
  CreatePassSystem();
  CreateTexturedMaterial();
  auto pass = CreateColorPass(0);
  auto view = render_system_->CreateView(pass.get(), binding_layout_);
  ASSERT_NE(view, nullptr);
  EXPECT_DEATH(pass.reset(), "views left in it");
}

TEST_F(RenderPassTest, DestroyingAPassDuringAFrameFails) {
  CreatePassSystem();
  auto pass = CreateColorPass(0);
  ASSERT_TRUE(render_system_->BeginFrame());
  EXPECT_DEATH(pass.reset(), "destroyed between BeginFrame");
  render_system_->EndFrame();
}

TEST_F(RenderPassTest, DestroyingAPassAfterAFailedBeginFrame) {
  CreatePassSystem();
  auto pass = CreateColorPass(0);
  state_.fail_begin_frame = true;
  EXPECT_FALSE(render_system_->BeginFrame());
  pass.reset();
  state_.fail_begin_frame = false;
  DrawFrame();
  EXPECT_THAT(GetFramePasses(), ElementsAre(nullptr));
}

TEST_F(RenderPassTest, DestroyingTheSystemWithPassesLeftFails) {
  CreatePassSystem();
  auto pass = CreateColorPass(0);
  EXPECT_DEATH(render_system_.reset(),
               "passes must be destroyed before the render system");
}

//------------------------------------------------------------------------------
// Drawing
//------------------------------------------------------------------------------

TEST_F(RenderPassTest, PassWithNothingDrawnIsStillDrawn) {
  CreatePassSystem();
  CreateTexturedMaterial();
  auto pass = CreateColorPass(0);
  auto view = render_system_->CreateView(pass.get(), binding_layout_);
  ASSERT_NE(view, nullptr);
  DrawFrame();
  ASSERT_THAT(state_.passes, SizeIs(2));
  EXPECT_EQ(state_.passes[0].pass, pass.get());
  EXPECT_THAT(state_.passes[0].draws, IsEmpty());
  EXPECT_THAT(state_.passes[0].draw_commands, IsEmpty());
  EXPECT_THAT(state_.passes[0].reads, IsEmpty());
  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderPassTest, DrawsAreInTheirViewsPass) {
  CreatePassSystem();
  CreateTexturedMaterial();
  auto pass = CreateColorPass(0);
  auto pass_view = render_system_->CreateView(pass.get(), binding_layout_);
  auto frame_view = render_system_->CreateView(binding_layout_);
  ASSERT_NE(pass_view, nullptr);
  ASSERT_NE(frame_view, nullptr);

  ASSERT_TRUE(render_system_->BeginFrame());
  render_system_->Draw(frame_view.get(), mesh_, material_,
                       instance_data_.get());
  render_system_->Draw(pass_view.get(), mesh_, material_, instance_data_.get());
  DrawList draw_list;
  draw_list.SetMesh(mesh_, material_, instance_data_.get());
  draw_list.Draw();
  render_system_->Draw(pass_view.get(), draw_list);
  render_system_->EndFrame();

  ASSERT_THAT(state_.passes, SizeIs(2));
  EXPECT_EQ(state_.passes[0].pass, pass.get());
  ASSERT_THAT(state_.passes[0].draws, SizeIs(1));
  EXPECT_EQ(state_.passes[0].draws[0].view, pass_view.get());
  EXPECT_EQ(state_.passes[0].draw_commands.size(),
            draw_list.GetCommands(GetAccessToken()).size());
  EXPECT_EQ(state_.passes[1].pass, nullptr);
  ASSERT_THAT(state_.passes[1].draws, SizeIs(1));
  EXPECT_EQ(state_.passes[1].draws[0].view, frame_view.get());
  EXPECT_THAT(state_.passes[1].draw_commands, IsEmpty());
  EXPECT_EQ(state_.invalid_call_count, 0);

  // Each frame records its own draws.
  DrawFrame();
  ASSERT_THAT(state_.passes, SizeIs(2));
  EXPECT_THAT(state_.passes[0].draws, IsEmpty());
  EXPECT_THAT(state_.passes[1].draws, IsEmpty());
}

// Draws as Terra Diem's world and water would: the world into targets, then
// the frame sampling them.
TEST_F(RenderPassTest, ReadsAreTheTexturesBoundInEachPass) {
  CreatePassSystem();
  CreateTexturedMaterial();
  auto world_color = CreateColor();
  auto world_depth = CreateDepth();
  auto world_pass =
      render_system_->CreatePass(RenderPassConfig()
                                     .SetColor(world_color.Get())
                                     .SetDepth(world_depth.Get()));
  ASSERT_NE(world_pass, nullptr);
  auto world_view =
      render_system_->CreateView(world_pass.get(), binding_layout_);
  auto water_view = render_system_->CreateView(binding_layout_);
  ASSERT_NE(world_view, nullptr);
  ASSERT_NE(water_view, nullptr);

  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 4, 4);
  ASSERT_NE(texture, nullptr);
  auto water_instance_data = material_->CreateInstanceBindingData();
  water_view->GetViewBindingData()->SetTexture(0, world_color.Get());
  water_instance_data->SetTexture(0, world_depth.Get());
  material_->GetMaterialBindingData()->SetTexture(0, texture.Get());

  ASSERT_TRUE(render_system_->BeginFrame());
  render_system_->Draw(world_view.get(), mesh_, material_,
                       instance_data_.get());
  DrawList water;
  water.SetMesh(mesh_, material_, water_instance_data.get());
  water.Draw();
  render_system_->Draw(water_view.get(), water);
  render_system_->EndFrame();

  ASSERT_THAT(state_.passes, SizeIs(2));
  EXPECT_THAT(state_.passes[0].reads, UnorderedElementsAre(texture.Get()));
  EXPECT_THAT(state_.passes[1].reads,
              UnorderedElementsAre(world_color.Get(), world_depth.Get(),
                                   texture.Get()));
  EXPECT_EQ(state_.invalid_call_count, 0);
}

}  // namespace
}  // namespace gb
