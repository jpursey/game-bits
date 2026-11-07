// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_TARGET_H_
#define GB_RENDER_RENDER_TARGET_H_

#include "gb/render/render_types.h"
#include "gb/render/sampler_options.h"
#include "gb/render/texture.h"
#include "gb/resource/resource_entry.h"

namespace gb {

//==============================================================================
// RenderTargetSize
//==============================================================================

// The size of a render target: fixed in pixels, or a scale of the frame's size
// that follows the window as it resizes.
struct RenderTargetSize {
  // Exactly width x height pixels, each from one to kMaxTextureWidth (or
  // kMaxTextureHeight).
  static RenderTargetSize Pixels(int width, int height) {
    RenderTargetSize size;
    size.follows_frame = false;
    size.width = width;
    size.height = height;
    return size;
  }

  // The frame's size times scale (in (0, 1]), rounded, and at least one pixel.
  static RenderTargetSize Frame(float scale = 1.0f) {
    RenderTargetSize size;
    size.scale = scale;
    return size;
  }

  bool operator==(const RenderTargetSize&) const = default;

  bool follows_frame = true;
  int width = 0;       // Pixels() only.
  int height = 0;      // Pixels() only.
  float scale = 1.0f;  // Frame() only.
};

//==============================================================================
// RenderTarget
//==============================================================================

// The sampler options render targets are created with by default: filtered,
// clamped to the edge, and without mipmaps.
inline const SamplerOptions kRenderTargetSamplerOptions =
    SamplerOptions().SetMipmap(false).SetAddressMode(
        SamplerAddressMode::kClampEdge);

// A texture that passes draw into, sampled in later passes and the frame as any
// other texture: bind it with BindingData::SetTexture().
//
// Its format is kDepth for depth, or kRgba8Srgb, kRgba8, kRg8, or kR8 for
// color. Its contents start cleared (transparent black, or the far plane for
// depth), and are cleared again whenever it is resized. They can't be read or
// written by the CPU: it has kStaticWrite volatility, and Clear(), Set(),
// Edit(), Get(), and saving it all fail.
//
// GetWidth() and GetHeight() are its current size. A target that follows the
// frame changes size only in RenderSystem::BeginFrame().
//
// This class and all derived classes must be thread-compatible.
class RenderTarget : public Texture {
 public:
  // Returns the size it was created with.
  const RenderTargetSize& GetSize() const { return size_; }

  bool IsRenderTarget() const final { return true; }

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  // Resizes the target to width x height texels, clearing it, if that isn't
  // its size already. If the backend can't, it logs an error and leaves the
  // target as it was.
  void Resize(RenderInternal, int width, int height);

 protected:
  RenderTarget(ResourceEntry entry, TextureFormat format,
               const RenderTargetSize& size, int width, int height,
               const SamplerOptions& options);
  ~RenderTarget() override = default;

  // Resizes the backend's image to width x height texels, cleared as a new
  // target's is. Returns false if it can't, leaving the image as it was.
  //
  // This is only called with a size other than the target's current one.
  virtual bool DoResize(int width, int height) = 0;

  // A render target's texels can't be read or written by the CPU, so these all
  // fail. Texture already fails Get() and Edit() for kStaticWrite volatility,
  // so only DoClear() and DoSet() are ever called.
  bool DoClear(int x, int y, int width, int height, const void* texel) final;
  bool DoSet(int x, int y, int width, int height, const void* texels,
             int stride) final;
  bool DoGet(void* out_texels) final;
  void* DoEditBegin() final;
  void OnEditEnd(bool modified) final;

 private:
  const RenderTargetSize size_;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_TARGET_H_
