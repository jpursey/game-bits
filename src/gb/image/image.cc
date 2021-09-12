// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/image/image.h"

#include "gb/base/allocator.h"

namespace gb {

namespace {

Pixel* AllocPixels(int width, int height) {
  return static_cast<Pixel*>(GetDefaultAllocator()->Alloc(
      width * height * sizeof(Pixel), alignof(Pixel)));
}

Pixel* NewPixels(int width, int height, Pixel clear_pixel) {
  Pixel* pixels = AllocPixels(width, height);
  Pixel* end = pixels + (width * height);
  for (Pixel* pixel = pixels; pixel != end; ++pixel) {
    *pixel = clear_pixel;
  }
  return pixels;
}

}  // namespace

Image::Image(int width, int height)
    : view_(width, height, AllocPixels(width, height)) {}

Image::Image(int width, int height, Pixel pixel)
    : view_(width, height, NewPixels(width, height, pixel)) {}

Image::Image(int width, int height, void* pixels,
             Callback<void(void*)> free_pixels)
    : view_(width, height, pixels), free_pixels_(std::move(free_pixels)) {}

Image::~Image() {
  void* pixels = const_cast<void*>(view_.GetRawPixels());
  if (pixels != nullptr) {
    if (free_pixels_) {
      free_pixels_(pixels);
    } else {
      GetDefaultAllocator()->Free(pixels);
    }
  }
}

}  // namespace gb
