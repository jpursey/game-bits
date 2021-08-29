// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/texture_array.h"

namespace gb {

bool TextureArray::Clear(int index, Pixel pixel) {
  if (index < 0 || index >= count_) {
    LOG(ERROR) << "Invalid index for texture array Clear: " << index
               << " (count is " << count_ << ").";
    return false;
  }
  return DoClear(index, pixel);
}

bool TextureArray::Set(int index, const void* pixels, size_t size_in_bytes) {
  if (index < 0 || index >= count_) {
    LOG(ERROR) << "Invalid index for texture array Set: " << index
               << " (count is " << count_ << ").";
    return false;
  }
  if (size_in_bytes < width_ * height_ * sizeof(Pixel)) {
    LOG(ERROR) << "Failed to set pixels, as provided buffer is too small";
    return false;
  }

  return DoSet(index, pixels);
}

bool TextureArray::Get(int index, void* out_pixels, size_t size_in_bytes) {
  if (index < 0 || index >= count_) {
    LOG(ERROR) << "Invalid index for texture array Get: " << index
               << " (count is " << count_ << ").";
    return false;
  }
  if (volatility_ == DataVolatility::kStaticWrite) {
    LOG(ERROR)
        << "Texture array cannot be read as its volatility is kStaticWrite";
    return nullptr;
  }
  if (size_in_bytes < width_ * height_ * sizeof(Pixel)) {
    LOG(ERROR) << "Failed to get pixels, as provided buffer is too small";
    return false;
  }
  return DoGet(index, out_pixels);
}

}  // namespace gb
