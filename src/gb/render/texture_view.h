// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEXTURE_VIEW_H_
#define GB_RENDER_TEXTURE_VIEW_H_

#include "gb/render/render_assert.h"
#include "gb/render/render_types.h"

namespace gb {

// A texture view provides an editable window onto a texture.
//
// Only one texture view may be active on a texture at a time. While a texture
// view is active, it can be edited freely although the texture dimensions are
// fixed. Edits are not applied to the underlying texture on the GPU
// until the texture view is deleted.
//
// A texture view can also be used in a read-only fashion. If no Set, Remove, or
// Modify functions are called, then this will not incur any update overhead for
// the texture.
//
// This class is thread-compatible.
class TextureView final {
 public:
  class ConstRegion;
  class Region;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  TextureView(const TextureView& other) = delete;
  TextureView(TextureView&& other) = default;
  TextureView& operator=(const TextureView& other) = delete;
  TextureView& operator=(TextureView&& other) = default;
  ~TextureView();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the width and height in pixels.
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  // Returns the total number of pixels in the texture.
  int GetCount() const { return width_ * height_; }

  // Returns the total number of bytes required to store the texture
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
  const Pixel* GetPixels() const { return static_cast<const Pixel*>(pixels_); }
  const uint32_t* GetPackedPixels() const {
    return static_cast<const uint32_t*>(pixels_);
  }
  const void* GetRawPixels() const { return pixels_; }

  // The following methods return direct writable access to the entire pixel
  // buffer as either RGBA pixels (Pixel*), packed pixels (uint32_t*), or raw
  // memory (void*).
  //
  // Calling these methods will result in the texture getting re-uploaded to the
  // GPU, regardless of whether the pixels are actually changed or not. Prefer
  // calling Get* methods, if the pixel data will not actually be modified.
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
  // texture width and height.
  const Pixel& Get(int x, int y) const { return GetPixels()[y * width_ + x]; }

  // Returns a writable reference to the specified pixel.
  //
  // Calling this function will result in the texture getting re-uploaded to the
  // GPU, regardless of whether the pixel is actually changed or not. Prefer
  // calling Get, if the pixel will not actually be modified.
  //
  // It is undefined behavior to specify coordinates that lie outside the
  // texture width and height.
  Pixel& Modify(int x, int y) {
    modified_ = true;
    return ModifyPixels()[y * width_ + x];
  }

  // Returns a read-only view onto a rectangular region of the image.
  //
  // If no region coordinates are specified, this returns a region for the
  // entire image. This is primarily useful for operating on images that are
  // logically an atlas or tile map of multiple images.
  //
  // It is undefined behavior to specify a region that does not lie entirely
  // within the texture.
  ConstRegion GetRegion() const;
  ConstRegion GetRegion(int x, int y, int width, int height) const;

  // Returns a modifiable view onto a rectangular region of the image.
  //
  // If no region coordinates are specified, this returns a region for the
  // entire image. This is primarily useful for operating on images that are
  // logically an atlas or tile map of multiple images.
  //
  // It is undefined behavior to specify a region that does not lie entirely
  // within the texture.
  Region ModifyRegion();
  Region ModifyRegion(int x, int y, int width, int height);

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  TextureView(RenderInternal, Texture* texture, void* pixels);

 private:
  friend class ConstRegion;
  friend class Region;

  Texture* const texture_;
  const int width_;
  const int height_;
  void* const pixels_;
  bool modified_ = false;
};

// This class provides a read-only view onto a region of a Texture.
//
// This class is thread-compatible.
class TextureView::ConstRegion {
 public:
  // Constructs the region from the specified texture view.
  //
  // Alternatively, you can construct a region by calling GetRegion() rather
  // than explicitly constructing it.
  ConstRegion(const TextureView* view, int x, int y, int width, int height);
  ConstRegion(const ConstRegion&) = delete;
  ConstRegion(ConstRegion&&) = delete;
  ConstRegion& operator=(const ConstRegion&) = delete;
  ConstRegion& operator=(ConstRegion&&) = delete;
  ~ConstRegion() = default;  // Not virtual, because all members are trivial (no
                             // destructors).

  // Returns the position of the region within the underlying texture
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

// This class provides a writable view onto a region of a Texture.
//
// This is an extension of the ConstRegion, providing the corresponding
// modification functions.
//
// This class is thread-compatible.
class TextureView::Region : public TextureView::ConstRegion {
 public:
  // Constructs the region from the specified texture view.
  //
  // Alternatively, you can construct a region by calling GetRegion() rather
  // than explicitly constructing it.
  Region(TextureView* view, int x, int y, int width, int height);
  ~Region() = default;

  // Returns a writable reference to the specified pixel relative to the region
  // position.
  //
  // Calling this function will result in the texture getting re-uploaded to the
  // GPU, regardless of whether the pixel is actually changed or not. Prefer
  // calling Get, if the pixel will not actually be modified.
  //
  // It is undefined behavior to specify coordinates that lie outside the
  // region's width and height.
  Pixel& Modify(int x, int y) {
    *modified_ = true;
    return GetPixels()[y * stride_ + x];
  }

  // Copies the contiguous array of pixels to the texture region.
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

inline TextureView::ConstRegion::ConstRegion(const TextureView* view, int x,
                                             int y, int width, int height)
    : pixels_(const_cast<Pixel*>(&view->Get(x, y))),
      stride_(view->GetWidth()),
      x_(x),
      y_(y),
      width_(width),
      height_(height) {
  RENDER_ASSERT(x >= 0 && x < view->GetWidth());
  RENDER_ASSERT(y >= 0 && y < view->GetHeight());
  RENDER_ASSERT(width >= 0 && x + width <= view->GetWidth());
  RENDER_ASSERT(height >= 0 && y + height <= view->GetHeight());
}

inline const Pixel& TextureView::ConstRegion::Get(int x, int y) const {
  return GetPixels()[y * stride_ + x];
}

inline void TextureView::ConstRegion::GetAll(std::vector<Pixel>* pixels) const {
  pixels->resize(width_ * height_);
  GetAll(pixels->data(), pixels->size() * sizeof(Pixel));
}

inline void TextureView::ConstRegion::GetAll(
    std::vector<uint32_t>* pixels) const {
  pixels->resize(width_ * height_);
  GetAll(pixels->data(), pixels->size() * sizeof(uint32_t));
}

inline TextureView::Region::Region(TextureView* view, int x, int y, int width,
                                   int height)
    : ConstRegion(view, x, y, width, height), modified_(&view->modified_) {}

inline void TextureView::Region::SetAll(absl::Span<const Pixel> pixels) {
  SetAll(pixels.data(), pixels.size() * sizeof(Pixel));
}

inline void TextureView::Region::SetAll(absl::Span<const uint32_t> pixels) {
  SetAll(pixels.data(), pixels.size() * sizeof(uint32_t));
}

inline TextureView::Region TextureView::ModifyRegion() {
  return Region(this, 0, 0, width_, height_);
}

inline TextureView::ConstRegion TextureView::GetRegion() const {
  return ConstRegion(this, 0, 0, width_, height_);
}

inline TextureView::Region TextureView::ModifyRegion(int x, int y, int width,
                                                     int height) {
  return Region(this, x, y, width, height);
}

inline TextureView::ConstRegion TextureView::GetRegion(int x, int y, int width,
                                                       int height) const {
  return ConstRegion(this, x, y, width, height);
}

}  // namespace gb

#endif  // GB_RENDER_TEXTURE_VIEW_H_
