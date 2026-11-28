// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_TEXTURE_H_
#define GB_RENDER_WEBGPU_WEBGPU_TEXTURE_H_

#include <memory>

#include "gb/render/texture.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "gb/render/webgpu/webgpu_texels.h"

namespace gb {

// WebGPU implementation of a 2D texture.
//
// This class is thread-compatible.
class WebGpuTexture final : public Texture {
 public:
  // Creates a texture, or returns null having logged why. `backend` must
  // outlive it.
  static WebGpuTexture* Create(ResourceEntry entry, WebGpuBackend* backend,
                               DataVolatility volatility, int width, int height,
                               const SamplerOptions& options,
                               TextureFormat format);

 protected:
  bool DoClear(int x, int y, int width, int height, const void* texel) override;
  bool DoSet(int x, int y, int width, int height, const void* texels,
             int stride) override;
  bool DoGet(void* out_texels) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;

 private:
  WebGpuTexture(ResourceEntry entry, DataVolatility volatility, int width,
                int height, const SamplerOptions& options, TextureFormat format,
                std::unique_ptr<WebGpuTexels> texels);
  ~WebGpuTexture() override = default;

  const std::unique_ptr<WebGpuTexels> texels_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_TEXTURE_H_
