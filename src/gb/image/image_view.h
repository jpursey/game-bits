// Copyright (c) 2021 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_IMAGE_IMAGE_VIEW_H_
#define GB_IMAGE_IMAGE_VIEW_H_

#include <vector>

#include "absl/types/span.h"
#include "gb/base/callback.h"
#include "gb/image/image_types.h"
#include "gb/image/pixel.h"

namespace gb {

// An image view provides an editable window onto image data.
//
// ImageView does not take ownership of the data, and requires that it remains
// valid for the life of the ImageView (much like std::string_view or
// absl::Span). An optional callback may be referenced which will be called when
// the ImageView is destructed (indicating it no longer is using the pixel
// data).
//
// An image view can also be used in a read-only fashion. If no Set, Remove, or
// Modify functions are called, then this will notify the image data owner that
// no modifications were made.
//
// This class is thread-compatible.
class ImageView {
 public:
  class ConstRegion;
  class Region;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  ImageView(int width, int height, void* pixels);
  ImageView(int width, int height, void* pixels,
            Callback<void(bool modified)> on_delete);
  ImageView(const ImageView& other) = delete;
  ImageView(ImageView&& other) = delete;
  ImageView& operator=(const ImageView& other) = delete;
  ImageView& operator=(ImageView&& other) = delete;
  ~ImageView();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the width and height in pixels.
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  // Returns the total number of pixels in the image.
  int GetCount() const { return width_ * height_; }

  // Returns the total number of bytes required to store the image
  // uncompressed.
  int GetSizeInBytes() const { return width_ * height_ * sizeof(Pixel); }

  // Returns true if the view was modified.
  bool IsModified() const { return modified_; }

  //----------------------------------------------------------------------------
  // Pixel access
  //----------------------------------------------------------------------------

  // The following methods return direct read-only access to the entire pixel
  // buffer as either RGBA pixels (Pixel*), packed pixels (uint32_t*), or raw
  // memory (void*).
  absl::Span<const Pixel> GetPixels() const {
    return absl::Span(static_cast<const Pixel*>(pixels_), width_ * height_);
  }
  absl::Span<const uint32_t> GetPackedPixels() const {
    return absl::Span(static_cast<const uint32_t*>(pixels_), width_ * height_);
  }
  const void* GetRawPixels() const { return pixels_; }

  // The following methods return direct writable access to the entire pixel
  // buffer as either RGBA pixels (Pixel*), packed pixels (uint32_t*), or raw
  // memory (void*).
  Pixel* ModifyPixels() {
    modified_ = true;
    return static_cast<Pixel*>(pixels_);
  }
  uint32_t* ModifyPackedPixels() {
    modified_ = true;
    return static_cast<uint32_t*>(pixels_);
  }
  void* ModifyRawPixels() {
    modified_ = true;
    return pixels_;
  }

  // Returns a read-only reference to the specified pixel.
  //
  // It is undefined behavior to specify coordinates that lie outside the
  // image width and height.
  const Pixel& Get(int x, int y) const { return GetPixels()[y * width_ + x]; }

  // Returns a writable reference to the specified pixel.
  //
  // It is undefined behavior to specify coordinates that lie outside the
  // image width and height.
  Pixel& Modify(int x, int y) {
    modified_ = true;
    return ModifyPixels()[y * width_ + x];
  }

  // Returns a read-only view onto a rectangular region of the image.
  //
  // If no region coordinates are specified, this returns a region for the
  // entire image. Specifying a sub region is primarily useful for operating on
  // images that are logically an atlas or tile map of multiple images.
  //
  // It is undefined behavior to specify a region that does not lie entirely
  // within the image.
  ConstRegion GetRegion() const;
  ConstRegion GetRegion(int x, int y, int width, int height) const;

  // Returns a modifiable view onto a rectangular region of the image.
  //
  // If no region coordinates are specified, this returns a region for the
  // entire image. Specifying a sub region is primarily useful for operating on
  // images that are logically an atlas or tile map of multiple images.
  //
  // It is undefined behavior to specify a region that does not lie entirely
  // within the image.
  Region ModifyRegion();
  Region ModifyRegion(int x, int y, int width, int height);

 private:
  friend class ConstRegion;
  friend class Region;

  const int width_;
  const int height_;
  void* const pixels_;
  const Callback<void(bool modified)> on_delete_;
  bool modified_ = false;
};

// This class provides a read-only view onto a region of a Image.
//
// This class is thread-compatible.
class ImageView::ConstRegion {
 public:
  // Constructs the region from the specified image view.
  //
  // Alternatively, you can construct a region by calling GetRegion() rather
  // than explicitly constructing it.
  ConstRegion(const ImageView* view, int x, int y, int width, int height);
  ConstRegion(const ConstRegion&) = delete;
  ConstRegion(ConstRegion&&) = delete;
  ConstRegion& operator=(const ConstRegion&) = delete;
  ConstRegion& operator=(ConstRegion&&) = delete;
  ~ConstRegion() = default;  // Not virtual, because all members are trivial (no
                             // destructors).

  // Returns the position of the region within the underlying image
  // (upper-left corner).
  int GetX() const { return x_; }
  int GetY() const { return y_; }

  // Returns the width and height of the region.
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  // Returns a read-only reference to the specified pixel relative to the region
  // position.
  //
  // It is undefined behavior to specify coordinates that lie outside the
  // region's width and height.
  const Pixel& Get(int x, int y) const;

  // Copies all pixels in the region to a contiguous array of region
  // width*height pixels.
  void GetAll(std::vector<Pixel>* pixels) const;
  void GetAll(std::vector<uint32_t>* pixels) const;
  void GetAll(void* pixels, size_t size_in_bytes) const;

  template <typename Type>
  std::vector<Type> GetAll() const {
    std::vector<Type> pixels;
    GetAll(&pixels);
    return pixels;
  }

 protected:
  Pixel* GetPixels() const { return static_cast<Pixel*>(pixels_); }

  void* const pixels_;
  const int stride_;
  const int x_;
  const int y_;
  const int width_;
  const int height_;
};

// This class provides a writable view onto a region of a Image.
//
// This is an extension of the ConstRegion, providing the corresponding
// modification functions.
//
// This class is thread-compatible.
class ImageView::Region : public ImageView::ConstRegion {
 public:
  // Constructs the region from the specified image view.
  //
  // Alternatively, you can construct a region by calling GetRegion() rather
  // than explicitly constructing it.
  Region(ImageView* view, int x, int y, int width, int height);
  ~Region() = default;

  // Returns a writable reference to the specified pixel relative to the region
  // position.
  //
  // It is undefined behavior to specify coordinates that lie outside the
  // region's width and height.
  Pixel& Modify(int x, int y) {
    *modified_ = true;
    return GetPixels()[y * stride_ + x];
  }

  // Copies the contiguous array of pixels to the image region.
  //
  // The provided array are expected to contain region width*height pixels. If
  // there are less pixels provided, then the remainder of the region is left
  // untouched.
  void SetAll(absl::Span<const Pixel> pixels);
  void SetAll(absl::Span<const uint32_t> pixels);
  void SetAll(const void* pixels, size_t size_in_bytes);

  // Clears the region with the specified color (transparent black, by default).
  void Clear(Pixel pixel = {0, 0, 0, 0});
  void Clear(uint32_t pixel) { Clear(Pixel(pixel)); }

 private:
  bool* const modified_;
};

inline ImageView::ConstRegion::ConstRegion(const ImageView* view, int x, int y,
                                           int width, int height)
    : pixels_(const_cast<Pixel*>(&view->Get(x, y))),
      stride_(view->GetWidth()),
      x_(x),
      y_(y),
      width_(width),
      height_(height) {}

inline const Pixel& ImageView::ConstRegion::Get(int x, int y) const {
  return GetPixels()[y * stride_ + x];
}

inline void ImageView::ConstRegion::GetAll(std::vector<Pixel>* pixels) const {
  pixels->resize(width_ * height_);
  GetAll(pixels->data(), pixels->size() * sizeof(Pixel));
}

inline void ImageView::ConstRegion::GetAll(
    std::vector<uint32_t>* pixels) const {
  pixels->resize(width_ * height_);
  GetAll(pixels->data(), pixels->size() * sizeof(uint32_t));
}

inline ImageView::Region::Region(ImageView* view, int x, int y, int width,
                                 int height)
    : ConstRegion(view, x, y, width, height), modified_(&view->modified_) {}

inline void ImageView::Region::SetAll(absl::Span<const Pixel> pixels) {
  SetAll(pixels.data(), pixels.size() * sizeof(Pixel));
}

inline void ImageView::Region::SetAll(absl::Span<const uint32_t> pixels) {
  SetAll(pixels.data(), pixels.size() * sizeof(uint32_t));
}

inline ImageView::Region ImageView::ModifyRegion() {
  return Region(this, 0, 0, width_, height_);
}

inline ImageView::ConstRegion ImageView::GetRegion() const {
  return ConstRegion(this, 0, 0, width_, height_);
}

inline ImageView::Region ImageView::ModifyRegion(int x, int y, int width,
                                                 int height) {
  return Region(this, x, y, width, height);
}

inline ImageView::ConstRegion ImageView::GetRegion(int x, int y, int width,
                                                   int height) const {
  return ConstRegion(this, x, y, width, height);
}

}  // namespace gb

#endif  // GB_IMAGE_IMAGE_VIEW_H_
