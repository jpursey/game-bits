// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_TEXELS_H_
#define GB_RENDER_WEBGPU_WEBGPU_TEXELS_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "gb/render/render_types.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

// Holds the texels of a texture, texture array, or volume texture: its GPU
// texture, and unless it is kStaticWrite, a CPU copy, which Get() and editing
// read. A kStaticWrite texture writes each change to the GPU at once. Any
// other marks the region it changes, and writes it as the next frame ends:
// changes in one frame are written as the one region that holds them all.
//
// Regions are in three dimensions. In a 2D texture, z is zero and depth one; in
// a texture array, z is the layer.
//
// The GPU texture has one mip level, which a sampler with mipmaps reads.
//
// This class is thread-compatible.
class WebGpuTexels final : public WebGpuPendingWrite {
 public:
  // A region of texels, within the texture.
  struct Region {
    int x = 0;
    int y = 0;
    int z = 0;
    int width = 0;
    int height = 0;
    int depth = 0;
  };

  // What the GPU texture is.
  struct Properties {
    TextureFormat format = TextureFormat::kRgba8Srgb;
    wgpu::TextureDimension dimension = wgpu::TextureDimension::e2D;
    int width = 0;
    int height = 0;
    // The depth of a volume, the layers of an array, or 1.
    int depth = 1;
  };

  // Creates the texels. Returns null, having logged why, if the format can't
  // be held yet (kRgba4, and kDepth, which only render targets have).
  //
  // `backend` must outlive this.
  static std::unique_ptr<WebGpuTexels> Create(WebGpuBackend* backend,
                                              DataVolatility volatility,
                                              const Properties& properties);

  ~WebGpuTexels();

  // Sets every texel in a region to the texel.
  bool Clear(const Region& region, const void* texel);

  // Writes a region from texels, whose rows are `stride` texels apart, and
  // whose slices are `slice_height` rows apart.
  bool Set(const Region& region, const void* texels, int stride,
           int slice_height);

  // Reads the whole texture into out_texels. Fails for kStaticWrite.
  bool Get(void* out_texels) const;

  // Reads one layer (or slice) of the texture into out_texels. Fails for
  // kStaticWrite.
  bool GetLayer(int z, void* out_texels) const;

  // Returns the whole texture to change in place, or null for kStaticWrite.
  // EditEnd() must follow, saying whether it changed.
  void* EditBegin();
  void EditEnd(bool modified);

  void WritePending(const wgpu::Queue& queue,
                    const wgpu::CommandEncoder& encoder) override;

 private:
  WebGpuTexels(WebGpuBackend* backend, DataVolatility volatility,
               const Properties& properties, wgpu::Texture texture);

  Region GetWholeRegion() const { return {0, 0, 0, width_, height_, depth_}; }
  int64_t GetLayerSize() const {
    return GetTextureSizeInBytes(format_, width_, height_);
  }

  // Marks a region of the CPU copy to be written as the next frame ends.
  void MarkPending(const Region& region);

  // Writes a region to the GPU from texels, whose rows are `stride` texels
  // apart and slices `slice_height` rows apart.
  void WriteRegion(const wgpu::Queue& queue, const Region& region,
                   const void* texels, int stride, int slice_height);

  const TextureFormat format_;
  const int width_;
  const int height_;
  const int depth_;
  const int texel_size_;
  const int64_t gpu_bytes_;
  wgpu::Texture texture_;

  // The CPU copy, empty for kStaticWrite, and the region of it changed since
  // the GPU was last written.
  std::vector<uint8_t> texels_;
  std::optional<Region> pending_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_TEXELS_H_
