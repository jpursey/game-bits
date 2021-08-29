// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_texture_array.h"

namespace gb {

TestTextureArray::TestTextureArray(Config* config, ResourceEntry entry,
                                   DataVolatility volatility, int count,
                                   int width, int height,
                                   const SamplerOptions& options)
    : TextureArray(std::move(entry), volatility, count, width, height, options),
      config_(config) {
  const size_t size = count * width * height * sizeof(Pixel);
  pixels_ = static_cast<Pixel*>(std::malloc(size));
  std::memset(pixels_, 0xFF, size);
}

TestTextureArray::~TestTextureArray() { std::free(pixels_); }

bool TestTextureArray::DoClear(int index, Pixel pixel) {
  if (config_->fail_clear) {
    return false;
  }
  modify_count_ += 1;

  if (index < 0 || index >= GetCount()) {
    invalid_call_count_ += 1;
    return false;
  }

  const int pixel_count = GetWidth() * GetHeight();
  Pixel* dst_pixels = pixels_ + (index * pixel_count);
  for (int i = 0; i < pixel_count; ++i) {
    dst_pixels[i] = pixel;
  }
  return true;
}

bool TestTextureArray::DoSet(int index, const void* pixels) {
  if (config_->fail_set) {
    return false;
  }
  modify_count_ += 1;

  if (index < 0 || index >= GetCount()) {
    invalid_call_count_ += 1;
    return false;
  }

  const int pixel_count = GetWidth() * GetHeight();
  std::memcpy(pixels_ + (index * pixel_count),
              static_cast<const Pixel*>(pixels), pixel_count * sizeof(Pixel));
  return true;
}

bool TestTextureArray::DoGet(int index, void* out_pixels) {
  if (config_->fail_get) {
    return false;
  }

  if (index < 0 || index >= GetCount()) {
    invalid_call_count_ += 1;
    return false;
  }

  const int pixel_count = GetWidth() * GetHeight();
  std::memcpy(out_pixels, pixels_ + (index * pixel_count),
              pixel_count * sizeof(Pixel));
  return true;
}

}  // namespace gb
