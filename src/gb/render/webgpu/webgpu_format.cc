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

void ExpandRgba4(absl::Span<const uint16_t> texels, uint8_t* out) {
  for (const uint16_t texel : texels) {
    *out++ = static_cast<uint8_t>(((texel >> 12) & 0xF) * 17);
    *out++ = static_cast<uint8_t>(((texel >> 8) & 0xF) * 17);
    *out++ = static_cast<uint8_t>(((texel >> 4) & 0xF) * 17);
    *out++ = static_cast<uint8_t>((texel & 0xF) * 17);
  }
}

}  // namespace gb
