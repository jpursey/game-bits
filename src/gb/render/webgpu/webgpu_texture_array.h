// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_TEXTURE_ARRAY_H_
#define GB_RENDER_WEBGPU_WEBGPU_TEXTURE_ARRAY_H_

#include <memory>

#include "gb/render/texture_array.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "gb/render/webgpu/webgpu_texels.h"

namespace gb {

// WebGPU implementation of a texture array: a 2D texture with a layer for each
// texture.
//
// This class is thread-compatible.
class WebGpuTextureArray final : public TextureArray {
 public:
  // Creates a texture array, or returns null having logged why. `backend` must
  // outlive it.
  static WebGpuTextureArray* Create(ResourceEntry entry, WebGpuBackend* backend,
                                    DataVolatility volatility, int count,
                                    int width, int height,
                                    const SamplerOptions& options,
                                    TextureFormat format);

  const WebGpuTexels& GetTexels() const { return *texels_; }

 protected:
  bool DoClear(int index, const void* texel) override;
  bool DoSet(int index, const void* texels) override;
  bool DoGet(int index, void* out_texels) override;

 private:
  WebGpuTextureArray(ResourceEntry entry, DataVolatility volatility, int count,
                     int width, int height, const SamplerOptions& options,
                     TextureFormat format,
                     std::unique_ptr<WebGpuTexels> texels);
  ~WebGpuTextureArray() override = default;

  const std::unique_ptr<WebGpuTexels> texels_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_TEXTURE_ARRAY_H_
