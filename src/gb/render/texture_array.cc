// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/texture_array.h"

#include <cstdint>

#include "absl/log/log.h"

namespace gb {

bool TextureArray::Clear(int index, Pixel pixel) {
  if (index < 0 || index >= count_) {
    LOG(ERROR) << "Invalid index for texture array Clear: " << index
               << " (count is " << count_ << ").";
    return false;
  }
  const uint32_t texel = PixelToTexel(format_, pixel);
  return DoClear(index, &texel);
}

bool TextureArray::Set(int index, const void* texels, int64_t size_in_bytes) {
  if (index < 0 || index >= count_) {
    LOG(ERROR) << "Invalid index for texture array Set: " << index
               << " (count is " << count_ << ").";
    return false;
  }
  if (size_in_bytes < GetTextureSizeInBytes(format_, width_, height_)) {
    LOG(ERROR) << "Failed to set texels, as provided buffer is too small";
    return false;
  }

  return DoSet(index, texels);
}

bool TextureArray::CanGet(int index) const {
  if (index < 0 || index >= count_) {
    LOG(ERROR) << "Invalid index for texture array Get: " << index
               << " (count is " << count_ << ").";
    return false;
  }
  if (volatility_ == DataVolatility::kStaticWrite) {
    LOG(ERROR)
        << "Texture array cannot be read as its volatility is kStaticWrite";
    return false;
  }
  return true;
}

bool TextureArray::ValidatePixelFormat(std::string_view operation) const {
  if (IsPixelFormat(format_)) {
    return true;
  }
  LOG(ERROR) << "Texture array " << operation
             << " of pixels failed, as its texels are not pixels";
  return false;
}

bool TextureArray::Get(int index, void* out_texels, int64_t size_in_bytes) {
  if (!CanGet(index)) {
    return false;
  }
  if (size_in_bytes < GetTextureSizeInBytes(format_, width_, height_)) {
    LOG(ERROR) << "Failed to get texels, as provided buffer is too small";
    return false;
  }
  return DoGet(index, out_texels);
}

}  // namespace gb
