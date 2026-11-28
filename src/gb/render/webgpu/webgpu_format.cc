// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_format.h"

namespace gb {

wgpu::TextureFormat GetWebGpuFormat(TextureFormat format) {
  switch (format) {
    case TextureFormat::kRgba8Srgb:
      return wgpu::TextureFormat::RGBA8UnormSrgb;
    case TextureFormat::kRgba8:
    case TextureFormat::kRgba4:
      return wgpu::TextureFormat::RGBA8Unorm;
    case TextureFormat::kRg8:
      return wgpu::TextureFormat::RG8Unorm;
    case TextureFormat::kR8:
      return wgpu::TextureFormat::R8Unorm;
    case TextureFormat::kDepth:
      return wgpu::TextureFormat::Depth32Float;
  }
  return wgpu::TextureFormat::Undefined;
}

int GetWebGpuTexelSize(TextureFormat format) {
  if (format == TextureFormat::kRgba4) {
    return 4;
  }
  return GetTexelSize(format);
}

WebGpuVertexFormat GetWebGpuVertexFormat(ShaderValue value) {
  using Format = wgpu::VertexFormat;
  constexpr Format kNone = Format(0);
  switch (value) {
    case ShaderValue::kFloat:
      return {Format::Float32, 4};
    case ShaderValue::kVec2:
      return {Format::Float32x2, 8};
    case ShaderValue::kVec3:
      return {Format::Float32x3, 12};
    case ShaderValue::kVec4:
      return {Format::Float32x4, 16};
    case ShaderValue::kColor:
      return {Format::Unorm8x4, 4};
    case ShaderValue::kI8Norm2:
      return {Format::Snorm8x2, 2};
    case ShaderValue::kI8Norm3:
      return {kNone, 3};
    case ShaderValue::kI8Norm4:
      return {Format::Snorm8x4, 4};
    case ShaderValue::kI16Norm2:
      return {Format::Snorm16x2, 4};
    case ShaderValue::kI16Norm3:
      return {kNone, 6};
    case ShaderValue::kI16Norm4:
      return {Format::Snorm16x4, 8};
    case ShaderValue::kU8Norm2:
      return {Format::Unorm8x2, 2};
    case ShaderValue::kU8Norm3:
      return {kNone, 3};
    case ShaderValue::kU8Norm4:
      return {Format::Unorm8x4, 4};
    case ShaderValue::kU16Norm2:
      return {Format::Unorm16x2, 4};
    case ShaderValue::kU16Norm3:
      return {kNone, 6};
    case ShaderValue::kU16Norm4:
      return {Format::Unorm16x4, 8};
    case ShaderValue::kInt8:
      return {Format::Sint8, 1};
    case ShaderValue::kI8Vec2:
      return {Format::Sint8x2, 2};
    case ShaderValue::kI8Vec3:
      return {kNone, 3};
    case ShaderValue::kI8Vec4:
      return {Format::Sint8x4, 4};
    case ShaderValue::kInt16:
      return {Format::Sint16, 2};
    case ShaderValue::kI16Vec2:
      return {Format::Sint16x2, 4};
    case ShaderValue::kI16Vec3:
      return {kNone, 6};
    case ShaderValue::kI16Vec4:
      return {Format::Sint16x4, 8};
    case ShaderValue::kInt:
      return {Format::Sint32, 4};
    case ShaderValue::kIVec2:
      return {Format::Sint32x2, 8};
    case ShaderValue::kIVec3:
      return {Format::Sint32x3, 12};
    case ShaderValue::kIVec4:
      return {Format::Sint32x4, 16};
    case ShaderValue::kUint8:
      return {Format::Uint8, 1};
    case ShaderValue::kU8Vec2:
      return {Format::Uint8x2, 2};
    case ShaderValue::kU8Vec3:
      return {kNone, 3};
    case ShaderValue::kU8Vec4:
      return {Format::Uint8x4, 4};
    case ShaderValue::kUint16:
      return {Format::Uint16, 2};
    case ShaderValue::kU16Vec2:
      return {Format::Uint16x2, 4};
    case ShaderValue::kU16Vec3:
      return {kNone, 6};
    case ShaderValue::kU16Vec4:
      return {Format::Uint16x4, 8};
    case ShaderValue::kUint:
      return {Format::Uint32, 4};
    case ShaderValue::kUVec2:
      return {Format::Uint32x2, 8};
    case ShaderValue::kUVec3:
      return {Format::Uint32x3, 12};
    case ShaderValue::kUVec4:
      return {Format::Uint32x4, 16};
  }
  return {};
}

void ExpandRgba4(absl::Span<const uint16_t> texels, uint8_t* out) {
  for (const uint16_t texel : texels) {
    *out++ = static_cast<uint8_t>(((texel >> 12) & 0xF) * 17);
    *out++ = static_cast<uint8_t>(((texel >> 8) & 0xF) * 17);
    *out++ = static_cast<uint8_t>(((texel >> 4) & 0xF) * 17);
    *out++ = static_cast<uint8_t>((texel & 0xF) * 17);
  }
}

}  // namespace gb
