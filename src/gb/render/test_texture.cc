// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_texture.h"

#include <cstring>

namespace gb {

TestTexture::TestTexture(Config* config, ResourceEntry entry,
                         DataVolatility volatility, int width, int height,
                         const SamplerOptions& options, TextureFormat format)
    : Texture(std::move(entry), volatility, width, height, options, format),
      config_(config),
      texels_(GetTextureSizeInBytes(format, width, height), 0xFF) {}

std::vector<Pixel> TestTexture::GetPixelRegion(int x, int y, int width,
                                               int height) const {
  const absl::Span<const Pixel> pixels = GetPixels();
  std::vector<Pixel> region(width * height);
  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      region[j * width + i] = pixels[(y + j) * GetWidth() + (x + i)];
    }
  }
  return region;
}

std::vector<uint32_t> TestTexture::GetPackedPixelRegion(int x, int y, int width,
                                                        int height) const {
  const absl::Span<const Pixel> pixels = GetPixels();
  std::vector<uint32_t> region(width * height);
  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      region[j * width + i] = pixels[(y + j) * GetWidth() + (x + i)].Packed();
    }
  }
  return region;
}

bool TestTexture::DoClear(int x, int y, int width, int height,
                          const void* texel) {
  if (config_->fail_clear) {
    return false;
  }
  modify_count_ += 1;

  if (editing_ || x < 0 || y < 0 || width < 0 || height < 0 ||
      x + width > GetWidth() || y + height > GetHeight()) {
    invalid_call_count_ += 1;
    return false;
  }

  const int texel_size = GetTexelSize(GetFormat());
  uint8_t* dst_texels = texels_.data() + (y * GetWidth() + x) * texel_size;
  for (int row = 0; row < height; ++row) {
    for (int i = 0; i < width; ++i) {
      std::memcpy(dst_texels + i * texel_size, texel, texel_size);
    }
    dst_texels += GetWidth() * texel_size;
  }
  return true;
}

bool TestTexture::DoSet(int x, int y, int width, int height, const void* texels,
                        int stride) {
  if (config_->fail_set) {
    return false;
  }
  modify_count_ += 1;

  if (editing_ || x < 0 || y < 0 || width < 0 || height < 0 ||
      x + width > GetWidth() || y + height > GetHeight() || stride < width) {
    invalid_call_count_ += 1;
    return false;
  }

  const int texel_size = GetTexelSize(GetFormat());
  const uint8_t* src_texels = static_cast<const uint8_t*>(texels);
  uint8_t* dst_texels = texels_.data() + (y * GetWidth() + x) * texel_size;
  for (int row = 0; row < height; ++row) {
    std::memcpy(dst_texels, src_texels, width * texel_size);
    dst_texels += GetWidth() * texel_size;
    src_texels += stride * texel_size;
  }
  return true;
}

bool TestTexture::DoGet(void* out_texels) {
  if (config_->fail_get) {
    return false;
  }
  if (editing_) {
    invalid_call_count_ += 1;
    return false;
  }
  std::memcpy(out_texels, texels_.data(), texels_.size());
  return true;
}

void* TestTexture::DoEditBegin() {
  if (config_->fail_edit_begin) {
    return nullptr;
  }
  if (editing_) {
    invalid_call_count_ += 1;
    return nullptr;
  }
  editing_ = true;
  return texels_.data();
}

void TestTexture::OnEditEnd(bool modified) {
  if (modified) {
    modify_count_ += 1;
  }
  if (!editing_) {
    invalid_call_count_ += 1;
    return;
  }
  editing_ = false;
}

}  // namespace gb
