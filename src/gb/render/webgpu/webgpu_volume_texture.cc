// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_volume_texture.h"

#include <utility>

namespace gb {

WebGpuVolumeTexture* WebGpuVolumeTexture::Create(
    ResourceEntry entry, WebGpuBackend* backend, DataVolatility volatility,
    int width, int height, int depth, const SamplerOptions& options,
    TextureFormat format) {
  auto texels = WebGpuTexels::Create(backend, volatility,
                                     {.format = format,
                                      .dimension = wgpu::TextureDimension::e3D,
                                      .width = width,
                                      .height = height,
                                      .depth = depth});
  if (texels == nullptr) {
    return nullptr;
  }
  return new WebGpuVolumeTexture(std::move(entry), volatility, width, height,
                                 depth, options, format, std::move(texels));
}

WebGpuVolumeTexture::WebGpuVolumeTexture(
    ResourceEntry entry, DataVolatility volatility, int width, int height,
    int depth, const SamplerOptions& options, TextureFormat format,
    std::unique_ptr<WebGpuTexels> texels)
    : VolumeTexture(std::move(entry), volatility, width, height, depth,
                    options, format),
      texels_(std::move(texels)) {}

bool WebGpuVolumeTexture::DoClear(int x, int y, int z, int width, int height,
                                  int depth, const void* texel) {
  return texels_->Clear({x, y, z, width, height, depth}, texel);
}

bool WebGpuVolumeTexture::DoSet(int x, int y, int z, int width, int height,
                                int depth, const void* texels, int stride,
                                int slice_height) {
  return texels_->Set({x, y, z, width, height, depth}, texels, stride,
                      slice_height);
}

bool WebGpuVolumeTexture::DoGet(void* out_texels) const {
  return texels_->Get(out_texels);
}

}  // namespace gb
