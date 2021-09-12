// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMAGE_IMAGE_H_
#define GB_IMAGE_IMAGE_H_

#include "absl/types/span.h"
#include "gb/image/image_types.h"
#include "gb/image/image_view.h"
#include "gb/image/pixel.h"

namespace gb {

// An image defines a 2D image of RGBA pixels in memory.
//
// This class is thread-compatible.
class Image final {
 public:
  // Creates a new image with the specified width and height. This only does
  // allocation, so pixel values are unspecified.
  Image(int width, int height);

  // Creates an image of the specified width and height, initialized with the
  // specified color.
  Image(int width, int height, Pixel pixel);

  // Creates an image using an already allocated array of pixels.
  Image(int width, int height, void* pixels, Callback<void(void*)> free_pixels);

  Image(const Image&) = delete;
  Image(Image&&) = delete;
  Image& operator=(const Image&) = delete;
  Image& operator=(Image&&) = delete;
  ~Image();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the current image attributes.
  int GetWidth() const { return view_.GetWidth(); }
  int GetHeight() const { return view_.GetHeight(); }
  int GetCount() const { return view_.GetCount(); }
  int GetSizeInBytes() const { return view_.GetSizeInBytes(); }

  // The following methods return direct read-only access to the entire pixel
  // buffer as either RGBA pixels (Pixel*), packed pixels (uint32_t*), or raw
  // memory (void*).
  absl::Span<const Pixel> GetPixels() const { return view_.GetPixels(); }
  absl::Span<const uint32_t> GetPackedPixels() const {
    return view_.GetPackedPixels();
  }
  const void* GetRawPixels() const { return view_.GetRawPixels(); }

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Returns an editable view onto the image.
  //
  // The underlying Image must live longer than any view created from it.
  ImageView Edit() {
    return ImageView(view_.GetWidth(), view_.GetHeight(),
                     const_cast<void*>(view_.GetRawPixels()));
  }

  // Returns a read-only view onto the image.
  const ImageView& View() { return view_; }

 private:
  ImageView view_;
  Callback<void(void*)> free_pixels_;
};

}  // namespace gb

#endif  // GB_IMAGE_IMAGE_H_
