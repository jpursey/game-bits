// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/texture.h"

#include <cstdint>
#include <memory>

#include "absl/log/check.h"
#include "absl/log/log.h"

namespace gb {

Texture::~Texture() {
  CHECK(!editing_) << "Texture deleted while an ImageView is active";
}

bool Texture::Clip(int* x, int* y, int* width, int* height,
                   const void** texels) {
  // The ends are computed in 64 bits, as the region may be as large as an int
  // allows.
  const int64_t end_x = int64_t{*x} + *width;
  const int64_t end_y = int64_t{*y} + *height;
  if (*x >= width_ || *y >= height_ || *width <= 0 || *height <= 0 ||
      end_x <= 0 || end_y <= 0) {
    return true;
  }
  const int texel_size = GetTexelSize(format_);
  const int stride = *width;
  if (end_x > width_) {
    *width = width_ - *x;
  }
  if (end_y > height_) {
    *height = height_ - *y;
  }
  if (*x < 0) {
    if (texels != nullptr) {
      *texels = static_cast<const uint8_t*>(*texels) - int64_t{*x} * texel_size;
    }
    *width += *x;
    *x = 0;
  }
  if (*y < 0) {
    if (texels != nullptr) {
      *texels = static_cast<const uint8_t*>(*texels) -
                int64_t{*y} * stride * texel_size;
    }
    *height += *y;
    *y = 0;
  }
  return false;
}

bool Texture::ValidatePixelFormat(std::string_view operation) const {
  if (IsPixelFormat(format_)) {
    return true;
  }
  LOG(ERROR) << "Texture " << operation
             << " of pixels failed, as its texels are not pixels";
  return false;
}

bool Texture::Clear(Pixel pixel) {
  return ClearRegion(0, 0, width_, height_, pixel);
}

bool Texture::Set(const void* texels, int64_t size_in_bytes) {
  if (editing_) {
    LOG(ERROR) << "Failed to set texels, as an ImageView is still active";
    return false;
  }
  if (size_in_bytes < GetTextureSizeInBytes(format_, width_, height_)) {
    LOG(ERROR) << "Failed to set texels, as provided buffer is too small";
    return false;
  }

  return DoSet(0, 0, width_, height_, texels, width_);
}

bool Texture::Get(void* out_texels, int64_t size_in_bytes) {
  if (editing_) {
    LOG(ERROR) << "Failed to get texels, as an ImageView is still active";
    return false;
  }
  if (volatility_ == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Texture cannot be read as its volatility is kStaticWrite";
    return false;
  }
  if (size_in_bytes < GetTextureSizeInBytes(format_, width_, height_)) {
    LOG(ERROR) << "Failed to get texels, as provided buffer is too small";
    return false;
  }
  return DoGet(out_texels);
}

bool Texture::ClearRegion(int x, int y, int width, int height, Pixel pixel) {
  if (editing_) {
    LOG(ERROR) << "Failed to clear texels, as an ImageView is still active";
    return false;
  }

  if (Clip(&x, &y, &width, &height)) {
    return true;
  }
  const uint32_t texel = PixelToTexel(format_, pixel);
  return DoClear(x, y, width, height, &texel);
}

bool Texture::SetRegion(int x, int y, int width, int height, const void* texels,
                        int64_t size_in_bytes) {
  if (editing_) {
    LOG(ERROR) << "Failed to set texels, as an ImageView is still active";
    return false;
  }
  if (width <= 0 || height <= 0) {
    return true;
  }
  if (size_in_bytes < GetTextureSizeInBytes(format_, width, height)) {
    LOG(ERROR) << "Failed to set texels, as provided buffer is too small";
    return false;
  }

  const int stride = width;
  if (Clip(&x, &y, &width, &height, &texels)) {
    return true;
  }
  return DoSet(x, y, width, height, texels, stride);
}

std::unique_ptr<ImageView> Texture::Edit() {
  if (editing_) {
    LOG(ERROR) << "ImageView cannot be created as an existing ImageView is "
                  "still active";
    return nullptr;
  }

  if (volatility_ == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Texture cannot be edited as its volatility is kStaticWrite";
    return nullptr;
  }

  if (!ValidatePixelFormat("Edit")) {
    return nullptr;
  }

  void* pixels = DoEditBegin();
  if (pixels == nullptr) {
    LOG(ERROR) << "Failed to create ImageView for texture";
    return nullptr;
  }

  editing_ = true;
  return std::make_unique<ImageView>(
      width_, height_, pixels,
      [this](bool modified) { OnViewDeleted(modified); });
}

void Texture::OnViewDeleted(bool modified) {
  editing_ = false;
  OnEditEnd(modified);
}

}  // namespace gb
