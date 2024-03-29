// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/image/image_view.h"

namespace gb {

ImageView::ImageView(int width, int height, void* pixels)
    : width_(width), height_(height), pixels_(pixels) {}
ImageView::ImageView(int width, int height, void* pixels,
                     Callback<void(bool modified)> on_delete)
    : width_(width),
      height_(height),
      pixels_(pixels),
      on_delete_(std::move(on_delete)) {}

ImageView::~ImageView() {
  if (on_delete_) {
    on_delete_(modified_);
  }
}

void ImageView::ConstRegion::GetAll(void* pixels, size_t size_in_bytes) const {
  size_t copy_size = width_ * height_ * sizeof(uint32_t);
  if (copy_size > size_in_bytes) {
    copy_size = size_in_bytes;
  }

  if (width_ == stride_) {
    std::memcpy(pixels, pixels_, copy_size);
    return;
  }

  const uint8_t* src_row = static_cast<const uint8_t*>(pixels_);
  const size_t src_row_size = stride_ * sizeof(uint32_t);
  uint8_t* dst_row = static_cast<uint8_t*>(pixels);
  const size_t dst_row_size = width_ * sizeof(uint32_t);
  for (int y = 0; y < height_; ++y) {
    if (dst_row_size >= copy_size) {
      std::memcpy(dst_row, src_row, copy_size);
      return;
    }
    std::memcpy(dst_row, src_row, dst_row_size);
    copy_size -= dst_row_size;
    dst_row += dst_row_size;
    src_row += src_row_size;
  }
}

void ImageView::Region::SetAll(const void* pixels, size_t size_in_bytes) {
  size_t copy_size = width_ * height_ * sizeof(uint32_t);
  if (size_in_bytes < copy_size) {
    copy_size = size_in_bytes;
  }
  if (copy_size == 0) {
    return;
  }
  *modified_ = true;

  if (width_ == stride_) {
    std::memcpy(pixels_, pixels, copy_size);
    return;
  }

  const uint8_t* src_row = static_cast<const uint8_t*>(pixels);
  const size_t src_row_size = width_ * sizeof(uint32_t);
  uint8_t* dst_row = static_cast<uint8_t*>(pixels_);
  const size_t dst_row_size = stride_ * sizeof(uint32_t);
  for (int y = 0; y < height_; ++y) {
    if (src_row_size >= copy_size) {
      std::memcpy(dst_row, src_row, copy_size);
      return;
    }
    std::memcpy(dst_row, src_row, src_row_size);
    copy_size -= src_row_size;
    dst_row += dst_row_size;
    src_row += src_row_size;
  }
}

void ImageView::Region::Clear(Pixel pixel) {
  *modified_ = true;
  if (pixel.Packed() == 0) {
    if (width_ == stride_) {
      std::memset(pixels_, 0, width_ * height_ * sizeof(uint32_t));
    } else {
      Pixel* dst_row = static_cast<Pixel*>(pixels_);
      for (int y = 0; y < height_; ++y) {
        std::memset(dst_row, 0, width_ * sizeof(uint32_t));
        dst_row += stride_;
      }
    }
  } else {
    Pixel* dst_row = static_cast<Pixel*>(pixels_);
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_; ++x) {
        dst_row[x] = pixel;
      }
      dst_row += stride_;
    }
  }
}

}  // namespace gb
