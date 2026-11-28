// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_format.h"

#include <cstdint>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;

TEST(WebGpuFormatTest, Formats) {
  EXPECT_EQ(GetWebGpuFormat(TextureFormat::kRgba8Srgb),
            wgpu::TextureFormat::RGBA8UnormSrgb);
  EXPECT_EQ(GetWebGpuFormat(TextureFormat::kRgba8),
            wgpu::TextureFormat::RGBA8Unorm);
  EXPECT_EQ(GetWebGpuFormat(TextureFormat::kRg8),
            wgpu::TextureFormat::RG8Unorm);
  EXPECT_EQ(GetWebGpuFormat(TextureFormat::kR8), wgpu::TextureFormat::R8Unorm);
  EXPECT_EQ(GetWebGpuFormat(TextureFormat::kRgba4),
            wgpu::TextureFormat::RGBA8Unorm);
  EXPECT_EQ(GetWebGpuFormat(TextureFormat::kDepth),
            wgpu::TextureFormat::Depth32Float);
}

TEST(WebGpuFormatTest, TexelSizes) {
  EXPECT_EQ(GetWebGpuTexelSize(TextureFormat::kRgba8Srgb), 4);
  EXPECT_EQ(GetWebGpuTexelSize(TextureFormat::kRgba8), 4);
  EXPECT_EQ(GetWebGpuTexelSize(TextureFormat::kRg8), 2);
  EXPECT_EQ(GetWebGpuTexelSize(TextureFormat::kR8), 1);
  EXPECT_EQ(GetWebGpuTexelSize(TextureFormat::kRgba4), 4);
  EXPECT_EQ(GetWebGpuTexelSize(TextureFormat::kDepth), 4);
}

TEST(WebGpuFormatTest, ExpandRgba4) {
  const std::vector<uint16_t> texels = {0x0000, 0xFFFF, 0x1234, 0xF00A};
  std::vector<uint8_t> out(texels.size() * 4);
  ExpandRgba4(texels, out.data());
  EXPECT_THAT(out, ElementsAre(0, 0, 0, 0,          //
                               255, 255, 255, 255,  //
                               17, 34, 51, 68,      //
                               255, 0, 0, 170));
}

TEST(WebGpuFormatTest, WgslBindings) {
  EXPECT_EQ(GetWgslBinding(0, WgslBindingKind::kResource), 0);
  EXPECT_EQ(GetWgslBinding(0, WgslBindingKind::kSampler), 1);
  EXPECT_EQ(GetWgslBinding(3, WgslBindingKind::kResource), 6);
  EXPECT_EQ(GetWgslBinding(3, WgslBindingKind::kSampler), 7);
  EXPECT_EQ(GetWgslBinding(kMaxWebGpuBindingIndex, WgslBindingKind::kSampler),
            999);
}

}  // namespace
}  // namespace gb
