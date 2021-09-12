// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/texture.h"

namespace gb {

bool Texture::Clip(int* x, int* y, int* width, int* height,
                   const void** pixels) {
  if (*x >= width_ || *y >= height_ || *width <= 0 || *height <= 0 ||
      *x + *width <= 0 || *y + *height <= 0) {
    return true;
  }
  const int stride = *width;
  if (*x + *width > width_) {
    *width = width_ - *x;
  }
  if (*y + *height > height_) {
    *height = height_ - *y;
  }
  if (*x < 0) {
    if (pixels != nullptr) {
      *pixels = static_cast<const Pixel*>(*pixels) - *x;
    }
    *width += *x;
    *x = 0;
  }
  if (*y < 0) {
    if (pixels != nullptr) {
      *pixels = static_cast<const Pixel*>(*pixels) - (*y * stride);
    }
    *height += *y;
    *y = 0;
  }
  return false;
}

bool Texture::Clear(Pixel pixel) {
  if (editing_) {
    LOG(ERROR) << "Failed to clear pixels, as an ImageView is still active";
    return false;
  }
  return DoClear(0, 0, width_, height_, pixel);
}

bool Texture::Set(const void* pixels, size_t size_in_bytes) {
  if (editing_) {
    LOG(ERROR) << "Failed to set pixels, as an ImageView is still active";
    return false;
  }
  if (size_in_bytes < width_ * height_ * sizeof(Pixel)) {
    LOG(ERROR) << "Failed to set pixels, as provided buffer is too small";
    return false;
  }

  return DoSet(0, 0, width_, height_, pixels, width_);
}

bool Texture::ClearRegion(int x, int y, int width, int height, Pixel pixel) {
  if (editing_) {
    LOG(ERROR) << "Failed to clear pixels, as an ImageView is still active";
    return false;
  }

  if (Clip(&x, &y, &width, &height)) {
    return true;
  }
  return DoClear(x, y, width, height, pixel);
}

bool Texture::SetRegion(int x, int y, int width, int height, const void* pixels,
                        size_t size_in_bytes) {
  if (editing_) {
    LOG(ERROR) << "Failed to set pixels, as an ImageView is still active";
    return false;
  }
  if (width <= 0 || height <= 0) {
    return true;
  }
  if (size_in_bytes < width * height * sizeof(Pixel)) {
    LOG(ERROR) << "Failed to set pixels, as provided buffer is too small";
    return false;
  }

  const int stride = width;
  if (Clip(&x, &y, &width, &height, &pixels)) {
    return true;
  }
  return DoSet(x, y, width, height, pixels, stride);
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
