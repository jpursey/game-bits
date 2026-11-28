// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_texture.h"

#include <utility>

namespace gb {

WebGpuTexture* WebGpuTexture::Create(ResourceEntry entry,
                                     WebGpuBackend* backend,
                                     DataVolatility volatility, int width,
                                     int height, const SamplerOptions& options,
                                     TextureFormat format) {
  auto texels = WebGpuTexels::Create(
      backend, volatility,
      {.format = format, .width = width, .height = height});
  if (texels == nullptr) {
    return nullptr;
  }
  return new WebGpuTexture(std::move(entry), volatility, width, height, options,
                           format, std::move(texels));
}

WebGpuTexture::WebGpuTexture(ResourceEntry entry, DataVolatility volatility,
                             int width, int height,
                             const SamplerOptions& options,
                             TextureFormat format,
                             std::unique_ptr<WebGpuTexels> texels)
    : Texture(std::move(entry), volatility, width, height, options, format),
      texels_(std::move(texels)) {}

bool WebGpuTexture::DoClear(int x, int y, int width, int height,
                            const void* texel) {
  return texels_->Clear({x, y, 0, width, height, 1}, texel);
}

bool WebGpuTexture::DoSet(int x, int y, int width, int height,
                          const void* texels, int stride) {
  return texels_->Set({x, y, 0, width, height, 1}, texels, stride, height);
}

bool WebGpuTexture::DoGet(void* out_texels) { return texels_->Get(out_texels); }

void* WebGpuTexture::DoEditBegin() { return texels_->EditBegin(); }

void WebGpuTexture::OnEditEnd(bool modified) { texels_->EditEnd(modified); }

}  // namespace gb
