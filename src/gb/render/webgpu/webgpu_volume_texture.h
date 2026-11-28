// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_VOLUME_TEXTURE_H_
#define GB_RENDER_WEBGPU_WEBGPU_VOLUME_TEXTURE_H_

#include <memory>

#include "gb/render/volume_texture.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "gb/render/webgpu/webgpu_texels.h"

namespace gb {

// WebGPU implementation of a volume texture: a 3D texture.
//
// This class is thread-compatible.
class WebGpuVolumeTexture final : public VolumeTexture {
 public:
  // Creates a volume texture, or returns null having logged why. `backend`
  // must outlive it.
  static WebGpuVolumeTexture* Create(ResourceEntry entry,
                                     WebGpuBackend* backend,
                                     DataVolatility volatility, int width,
                                     int height, int depth,
                                     const SamplerOptions& options,
                                     TextureFormat format);

  const WebGpuTexels& GetTexels() const { return *texels_; }

 protected:
  bool DoClear(int x, int y, int z, int width, int height, int depth,
               const void* texel) override;
  bool DoSet(int x, int y, int z, int width, int height, int depth,
             const void* texels, int stride, int slice_height) override;
  bool DoGet(void* out_texels) const override;

 private:
  WebGpuVolumeTexture(ResourceEntry entry, DataVolatility volatility, int width,
                      int height, int depth, const SamplerOptions& options,
                      TextureFormat format,
                      std::unique_ptr<WebGpuTexels> texels);
  ~WebGpuVolumeTexture() override = default;

  const std::unique_ptr<WebGpuTexels> texels_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_VOLUME_TEXTURE_H_
