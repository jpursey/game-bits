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
                   const void** pixels) {
  // The ends are computed in 64 bits, as the region may be as large as an int
  // allows.
  const int64_t end_x = int64_t{*x} + *width;
  const int64_t end_y = int64_t{*y} + *height;
  if (*x >= width_ || *y >= height_ || *width <= 0 || *height <= 0 ||
      end_x <= 0 || end_y <= 0) {
    return true;
  }
  const int stride = *width;
  if (end_x > width_) {
    *width = width_ - *x;
  }
  if (end_y > height_) {
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
      *pixels = static_cast<const Pixel*>(*pixels) - int64_t{*y} * stride;
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
  if (static_cast<int64_t>(size_in_bytes) <
      int64_t{width} * height * static_cast<int64_t>(sizeof(Pixel))) {
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
