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

TEST(WebGpuFormatTest, VertexFormats) {
  using Format = wgpu::VertexFormat;
  const struct {
    ShaderValue value;
    Format format;
    int size;
  } kCases[] = {
      {ShaderValue::kFloat, Format::Float32, 4},
      {ShaderValue::kVec2, Format::Float32x2, 8},
      {ShaderValue::kVec3, Format::Float32x3, 12},
      {ShaderValue::kVec4, Format::Float32x4, 16},
      {ShaderValue::kColor, Format::Unorm8x4, 4},
      {ShaderValue::kI8Norm2, Format::Snorm8x2, 2},
      {ShaderValue::kI8Norm4, Format::Snorm8x4, 4},
      {ShaderValue::kI16Norm2, Format::Snorm16x2, 4},
      {ShaderValue::kI16Norm4, Format::Snorm16x4, 8},
      {ShaderValue::kU8Norm2, Format::Unorm8x2, 2},
      {ShaderValue::kU8Norm4, Format::Unorm8x4, 4},
      {ShaderValue::kU16Norm2, Format::Unorm16x2, 4},
      {ShaderValue::kU16Norm4, Format::Unorm16x4, 8},
      {ShaderValue::kInt8, Format::Sint8, 1},
      {ShaderValue::kI8Vec2, Format::Sint8x2, 2},
      {ShaderValue::kI8Vec4, Format::Sint8x4, 4},
      {ShaderValue::kInt16, Format::Sint16, 2},
      {ShaderValue::kI16Vec2, Format::Sint16x2, 4},
      {ShaderValue::kI16Vec4, Format::Sint16x4, 8},
      {ShaderValue::kInt, Format::Sint32, 4},
      {ShaderValue::kIVec2, Format::Sint32x2, 8},
      {ShaderValue::kIVec3, Format::Sint32x3, 12},
      {ShaderValue::kIVec4, Format::Sint32x4, 16},
      {ShaderValue::kUint8, Format::Uint8, 1},
      {ShaderValue::kU8Vec2, Format::Uint8x2, 2},
      {ShaderValue::kU8Vec4, Format::Uint8x4, 4},
      {ShaderValue::kUint16, Format::Uint16, 2},
      {ShaderValue::kU16Vec2, Format::Uint16x2, 4},
      {ShaderValue::kU16Vec4, Format::Uint16x4, 8},
      {ShaderValue::kUint, Format::Uint32, 4},
      {ShaderValue::kUVec2, Format::Uint32x2, 8},
      {ShaderValue::kUVec3, Format::Uint32x3, 12},
      {ShaderValue::kUVec4, Format::Uint32x4, 16},
  };
  for (const auto& test_case : kCases) {
    const WebGpuVertexFormat format = GetWebGpuVertexFormat(test_case.value);
    EXPECT_EQ(format.format, test_case.format)
        << static_cast<int>(test_case.value);
    EXPECT_EQ(format.size, test_case.size) << static_cast<int>(test_case.value);
  }
}

TEST(WebGpuFormatTest, ThreeChannelNarrowVertexFormatsAreUndefined) {
  const struct {
    ShaderValue value;
    int size;
  } kCases[] = {
      {ShaderValue::kI8Norm3, 3}, {ShaderValue::kI16Norm3, 6},
      {ShaderValue::kU8Norm3, 3}, {ShaderValue::kU16Norm3, 6},
      {ShaderValue::kI8Vec3, 3},  {ShaderValue::kI16Vec3, 6},
      {ShaderValue::kU8Vec3, 3},  {ShaderValue::kU16Vec3, 6},
  };
  for (const auto& test_case : kCases) {
    const WebGpuVertexFormat format = GetWebGpuVertexFormat(test_case.value);
    EXPECT_EQ(format.format, wgpu::VertexFormat(0))
        << static_cast<int>(test_case.value);
    EXPECT_EQ(format.size, test_case.size) << static_cast<int>(test_case.value);
  }
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
