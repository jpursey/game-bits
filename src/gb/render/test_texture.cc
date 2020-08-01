// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_texture.h"

namespace gb {

TestTexture::TestTexture(Config* config, ResourceEntry entry,
                         DataVolatility volatility, int width, int height)
    : Texture(std::move(entry), volatility, width, height), config_(config) {
  const size_t size = width * height * sizeof(Pixel);
  pixels_ = static_cast<Pixel*>(std::malloc(size));
  std::memset(pixels_, 0xFF, size);
}

TestTexture::~TestTexture() { std::free(pixels_); }

std::vector<Pixel> TestTexture::GetPixelRegion(int x, int y, int width,
                                               int height) const {
  std::vector<Pixel> pixels(width * height);
  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      pixels[j * width + i] = pixels_[(y + j) * GetWidth() + (x + i)];
    }
  }
  return pixels;
}

std::vector<uint32_t> TestTexture::GetPackedPixelRegion(int x, int y, int width,
                                                        int height) const {
  std::vector<uint32_t> pixels(width * height);
  for (int j = 0; j < height; ++j) {
    for (int i = 0; i < width; ++i) {
      pixels[j * width + i] = pixels_[(y + j) * GetWidth() + (x + i)].Packed();
    }
  }
  return pixels;
}

bool TestTexture::DoClear(int x, int y, int width, int height, Pixel pixel) {
  if (config_->fail_clear) {
    return false;
  }
  modify_count_ += 1;

  if (editing_ || x < 0 || y < 0 || width < 0 || height < 0 ||
      x + width > GetWidth() || y + height > GetHeight()) {
    invalid_call_count_ += 1;
    return false;
  }

  Pixel* dst_pixels = pixels_;
  dst_pixels += y * GetWidth() + x;
  for (int row = 0; row < height; ++row) {
    for (int x = 0; x < width; ++x) {
      dst_pixels[x] = pixel;
    }
    dst_pixels += GetWidth();
  }
  return true;
}

bool TestTexture::DoSet(int x, int y, int width, int height, const void* pixels,
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

  const Pixel* src_pixels = static_cast<const Pixel*>(pixels);
  Pixel* dst_pixels = pixels_;
  dst_pixels += y * GetWidth() + x;
  for (int row = 0; row < height; ++row) {
    std::memcpy(dst_pixels, src_pixels, width * sizeof(Pixel));
    dst_pixels += GetWidth();
    src_pixels += stride;
  }
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
  return pixels_;
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
