// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_target.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "gb/render/render_test.h"
#include "gb/render/test_binding_data.h"
#include "gb/render/test_render_target.h"
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
using ::testing::Pair;
using ::testing::SizeIs;

auto LogTextHas(std::string_view text) {
  return Field(&LogRecorder::Message::text, HasSubstr(std::string(text)));
}

class RenderTargetTest : public RenderTest {
 protected:
  // Begins and ends a frame, resizing the targets that follow it.
  void DrawFrame() {
    ASSERT_TRUE(render_system_->BeginFrame());
    render_system_->EndFrame();
  }

  static TestRenderTarget* AsTest(RenderTarget* render_target) {
    return static_cast<TestRenderTarget*>(render_target);
  }
};

TEST_F(RenderTargetTest, CreateAsResourcePtr) {
  CreateSystem();
  ResourcePtr<RenderTarget> render_target = render_system_->CreateRenderTarget(
      TextureFormat::kRgba8, RenderTargetSize::Pixels(32, 16));
  ASSERT_NE(render_target, nullptr);
  EXPECT_TRUE(render_target->IsRenderTarget());
  EXPECT_EQ(render_target->GetFormat(), TextureFormat::kRgba8);
  EXPECT_EQ(render_target->GetSize(), RenderTargetSize::Pixels(32, 16));
  EXPECT_EQ(render_target->GetWidth(), 32);
  EXPECT_EQ(render_target->GetHeight(), 16);
  EXPECT_EQ(render_target->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(render_target->GetSamplerOptions(), kRenderTargetSamplerOptions);

  // A render target is held as a texture as any other is.
  ResourcePtr<Texture> texture = render_target;
  EXPECT_EQ(texture.Get(), render_target.Get());
  render_target.Reset();
  EXPECT_TRUE(texture->IsRenderTarget());
}

TEST_F(RenderTargetTest, CreateInResourceSet) {
  CreateSystem();
  ResourceSet resource_set;
  RenderTarget* render_target = render_system_->CreateRenderTarget(
      &resource_set, TextureFormat::kDepth, RenderTargetSize::Pixels(8, 8));
  ASSERT_NE(render_target, nullptr);
  EXPECT_EQ(resource_set.Get<RenderTarget>(render_target->GetResourceId()),
            render_target);
}

TEST_F(RenderTargetTest, TexturesAreNotRenderTargets) {
  CreateSystem();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 8, 8);
  ASSERT_NE(texture, nullptr);
  EXPECT_FALSE(texture->IsRenderTarget());
}

TEST_F(RenderTargetTest, CreateWithSamplerOptions) {
  CreateSystem();
  const SamplerOptions options =
      SamplerOptions().SetMipmap(false).SetFilter(false).SetAddressMode(
          SamplerAddressMode::kClampBorder, Pixel(255, 255, 255));
  auto render_target = render_system_->CreateRenderTarget(
      TextureFormat::kR8, RenderTargetSize::Pixels(8, 8), options);
  ASSERT_NE(render_target, nullptr);
  EXPECT_EQ(render_target->GetSamplerOptions(), options);
}

TEST_F(RenderTargetTest, CreateInEachFormat) {
  CreateSystem();
  for (TextureFormat format :
       {TextureFormat::kRgba8Srgb, TextureFormat::kRgba8, TextureFormat::kRg8,
        TextureFormat::kR8, TextureFormat::kDepth}) {
    auto render_target = render_system_->CreateRenderTarget(
        format, RenderTargetSize::Pixels(8, 8));
    ASSERT_NE(render_target, nullptr) << static_cast<int>(format);
    EXPECT_EQ(render_target->GetFormat(), format);
  }
  for (TextureFormat format :
       {TextureFormat::kRgba4, static_cast<TextureFormat>(100)}) {
    EXPECT_EQ(render_system_->CreateRenderTarget(
                  format, RenderTargetSize::Pixels(8, 8)),
              nullptr)
        << static_cast<int>(format);
  }
  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(RenderTargetTest, OnlyRenderTargetsHaveDepth) {
  CreateSystem();
  EXPECT_EQ(
      render_system_->CreateTexture(DataVolatility::kStaticWrite, 8, 8,
                                    SamplerOptions(), TextureFormat::kDepth),
      nullptr);
  EXPECT_EQ(render_system_->CreateTextureArray(DataVolatility::kStaticWrite, 2,
                                               8, 8, SamplerOptions(),
                                               TextureFormat::kDepth),
            nullptr);
  EXPECT_EQ(render_system_->CreateVolumeTexture(
                DataVolatility::kStaticWrite, 8, 8, 8,
                SamplerOptions().SetMipmap(false), TextureFormat::kDepth),
            nullptr);
}

TEST_F(RenderTargetTest, CreateValidatesPixels) {
  CreateSystem();
  for (const auto& [width, height] :
       {std::pair{1, 1}, std::pair{kMaxTextureWidth, 1},
        std::pair{1, kMaxTextureHeight}}) {
    EXPECT_NE(render_system_->CreateRenderTarget(
                  TextureFormat::kR8, RenderTargetSize::Pixels(width, height)),
              nullptr)
        << width << "x" << height;
  }
  for (const auto& [width, height] :
       {std::pair{0, 1}, std::pair{1, 0}, std::pair{-1, 1},
        std::pair{kMaxTextureWidth + 1, 1},
        std::pair{1, kMaxTextureHeight + 1}}) {
    EXPECT_EQ(render_system_->CreateRenderTarget(
                  TextureFormat::kR8, RenderTargetSize::Pixels(width, height)),
              nullptr)
        << width << "x" << height;
  }
}

TEST_F(RenderTargetTest, CreateValidatesFrameScale) {
  CreateSystem();
  for (float scale : {1.0f, 0.5f, 0.001f}) {
    EXPECT_NE(render_system_->CreateRenderTarget(
                  TextureFormat::kR8, RenderTargetSize::Frame(scale)),
              nullptr)
        << scale;
  }
  for (float scale :
       {0.0f, -0.5f, 1.01f, 2.0f, std::numeric_limits<float>::quiet_NaN()}) {
    EXPECT_EQ(render_system_->CreateRenderTarget(
                  TextureFormat::kR8, RenderTargetSize::Frame(scale)),
              nullptr)
        << scale;
  }
}

TEST_F(RenderTargetTest, CreateWithoutMipmapsOrTiles) {
  CreateSystem();
  EXPECT_EQ(render_system_->CreateRenderTarget(TextureFormat::kRgba8,
                                               RenderTargetSize::Pixels(8, 8),
                                               SamplerOptions()),
            nullptr);
  EXPECT_EQ(render_system_->CreateRenderTarget(
                TextureFormat::kRgba8, RenderTargetSize::Pixels(8, 8),
                SamplerOptions(kRenderTargetSamplerOptions).SetTileSize(2)),
            nullptr);
}

TEST_F(RenderTargetTest, FilterOnlyFilterableFormats) {
  state_.unfilterable_formats.insert(TextureFormat::kDepth);
  CreateSystem();
  EXPECT_EQ(render_system_->CreateRenderTarget(TextureFormat::kDepth,
                                               RenderTargetSize::Pixels(8, 8)),
            nullptr);
  EXPECT_NE(render_system_->CreateRenderTarget(
                TextureFormat::kDepth, RenderTargetSize::Pixels(8, 8),
                SamplerOptions(kRenderTargetSamplerOptions).SetFilter(false)),
            nullptr);
  EXPECT_NE(render_system_->CreateRenderTarget(TextureFormat::kR8,
                                               RenderTargetSize::Pixels(8, 8)),
            nullptr);
}

TEST_F(RenderTargetTest, FailCreate) {
  CreateSystem();
  state_.fail_create_render_target = true;
  EXPECT_EQ(render_system_->CreateRenderTarget(TextureFormat::kRgba8,
                                               RenderTargetSize::Frame()),
            nullptr);
}

TEST_F(RenderTargetTest, FrameSizeIsScaledAndRounded) {
  state_.frame_dimensions = {101, 50};
  CreateSystem();
  for (const auto& [scale, width, height] :
       {std::tuple{1.0f, 101, 50}, std::tuple{0.5f, 51, 25},
        std::tuple{0.25f, 25, 13}, std::tuple{0.001f, 1, 1}}) {
    auto render_target = render_system_->CreateRenderTarget(
        TextureFormat::kRgba8, RenderTargetSize::Frame(scale));
    ASSERT_NE(render_target, nullptr) << scale;
    EXPECT_EQ(render_target->GetSize(), RenderTargetSize::Frame(scale));
    EXPECT_EQ(render_target->GetWidth(), width) << scale;
    EXPECT_EQ(render_target->GetHeight(), height) << scale;
  }
}

TEST_F(RenderTargetTest, FrameSizeIsAtLeastOnePixel) {
  state_.frame_dimensions = {0, 0};
  CreateSystem();
  auto render_target = render_system_->CreateRenderTarget(
      TextureFormat::kRgba8, RenderTargetSize::Frame());
  ASSERT_NE(render_target, nullptr);
  EXPECT_EQ(render_target->GetWidth(), 1);
  EXPECT_EQ(render_target->GetHeight(), 1);
}

TEST_F(RenderTargetTest, FollowsTheFrameInBeginFrame) {
  state_.frame_dimensions = {100, 50};
  CreateSystem();
  auto full = render_system_->CreateRenderTarget(TextureFormat::kRgba8,
                                                 RenderTargetSize::Frame());
  auto half = render_system_->CreateRenderTarget(TextureFormat::kDepth,
                                                 RenderTargetSize::Frame(0.5f));
  auto fixed = render_system_->CreateRenderTarget(
      TextureFormat::kRgba8, RenderTargetSize::Pixels(16, 16));
  ASSERT_NE(full, nullptr);
  ASSERT_NE(half, nullptr);
  ASSERT_NE(fixed, nullptr);

  // Nothing is resized while the frame keeps its size.
  DrawFrame();
  DrawFrame();
  EXPECT_THAT(AsTest(full.Get())->GetResizes(), IsEmpty());
  EXPECT_THAT(AsTest(half.Get())->GetResizes(), IsEmpty());

  // A new frame size takes effect in the next BeginFrame().
  state_.frame_dimensions = {200, 120};
  EXPECT_EQ(full->GetWidth(), 100);
  DrawFrame();
  DrawFrame();
  EXPECT_EQ(full->GetWidth(), 200);
  EXPECT_EQ(full->GetHeight(), 120);
  EXPECT_EQ(half->GetWidth(), 100);
  EXPECT_EQ(half->GetHeight(), 60);
  EXPECT_EQ(fixed->GetWidth(), 16);
  EXPECT_EQ(fixed->GetHeight(), 16);
  EXPECT_THAT(AsTest(full.Get())->GetResizes(), ElementsAre(Pair(200, 120)));
  EXPECT_THAT(AsTest(half.Get())->GetResizes(), ElementsAre(Pair(100, 60)));
  EXPECT_THAT(AsTest(fixed.Get())->GetResizes(), IsEmpty());
}

TEST_F(RenderTargetTest, FailedBeginFrameDoesNotResize) {
  state_.frame_dimensions = {100, 50};
  CreateSystem();
  auto render_target = render_system_->CreateRenderTarget(
      TextureFormat::kRgba8, RenderTargetSize::Frame());
  ASSERT_NE(render_target, nullptr);

  state_.frame_dimensions = {200, 100};
  state_.fail_begin_frame = true;
  EXPECT_FALSE(render_system_->BeginFrame());
  EXPECT_EQ(render_target->GetWidth(), 100);
  EXPECT_THAT(AsTest(render_target.Get())->GetResizes(), IsEmpty());
}

TEST_F(RenderTargetTest, FailedResizeKeepsTheSize) {
  LogErrorGuard log_error_guard;
  state_.frame_dimensions = {100, 50};
  CreateSystem();
  auto render_target = render_system_->CreateRenderTarget(
      TextureFormat::kRgba8, RenderTargetSize::Frame());
  ASSERT_NE(render_target, nullptr);

  state_.frame_dimensions = {200, 100};
  state_.render_target_config.fail_resize = true;
  DrawFrame();
  EXPECT_EQ(render_target->GetWidth(), 100);
  EXPECT_EQ(render_target->GetHeight(), 50);
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(LogTextHas("Failed to resize render target")));

  // It is resized once the backend can.
  state_.render_target_config.fail_resize = false;
  DrawFrame();
  EXPECT_EQ(render_target->GetWidth(), 200);
  EXPECT_EQ(render_target->GetHeight(), 100);
}

TEST_F(RenderTargetTest, ReleasedTargetsStopFollowingTheFrame) {
  state_.frame_dimensions = {100, 50};
  CreateSystem();
  auto kept = render_system_->CreateRenderTarget(TextureFormat::kRgba8,
                                                 RenderTargetSize::Frame());
  auto released = render_system_->CreateRenderTarget(TextureFormat::kRgba8,
                                                     RenderTargetSize::Frame());
  ASSERT_NE(kept, nullptr);
  ASSERT_NE(released, nullptr);
  const ResourceId released_id = released->GetResourceId();
  released.Reset();
  EXPECT_EQ(resource_system_->Get<RenderTarget>(released_id), nullptr);

  state_.frame_dimensions = {200, 100};
  DrawFrame();
  EXPECT_EQ(kept->GetWidth(), 200);
}

TEST_F(RenderTargetTest, TexelsCannotBeReadOrWritten) {
  LogErrorGuard log_error_guard;
  CreateSystem();
  auto render_target = render_system_->CreateRenderTarget(
      TextureFormat::kRgba8, RenderTargetSize::Pixels(4, 4));
  ASSERT_NE(render_target, nullptr);
  std::vector<Pixel> pixels(16);

  EXPECT_FALSE(render_target->Clear());
  EXPECT_FALSE(render_target->ClearRegion(0, 0, 2, 2));
  EXPECT_FALSE(render_target->Set(pixels));
  EXPECT_FALSE(render_target->SetRegion(0, 0, 2, 2, pixels));
  EXPECT_FALSE(render_target->Get(
      pixels.data(), static_cast<int64_t>(pixels.size() * sizeof(Pixel))));
  EXPECT_EQ(render_target->Edit(), nullptr);
  EXPECT_THAT(log_error_guard.TakeMessages(), SizeIs(6));
}

TEST_F(RenderTargetTest, CannotBeSaved) {
  LogErrorGuard log_error_guard;
  CreateSystem();
  auto render_target = render_system_->CreateRenderTarget(
      TextureFormat::kRgba8, RenderTargetSize::Pixels(4, 4));
  ASSERT_NE(render_target, nullptr);
  EXPECT_FALSE(
      render_system_->SaveTexture("mem:/target.gbtx", render_target.Get()));
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(LogTextHas("Cannot save render target")));
}

TEST_F(RenderTargetTest, BindsAsATexture) {
  CreateSystem();
  auto render_target = render_system_->CreateRenderTarget(
      TextureFormat::kDepth, RenderTargetSize::Frame());
  ASSERT_NE(render_target, nullptr);

  TestBindingData binding_data(nullptr, BindingSet::kView,
                               {Binding()
                                    .SetShaders(ShaderType::kFragment)
                                    .SetLocation(BindingSet::kView, 0)
                                    .SetTexture()});
  binding_data.SetTexture(0, render_target.Get());
  EXPECT_EQ(binding_data.GetTexture(0), render_target.Get());
}

}  // namespace
}  // namespace gb
