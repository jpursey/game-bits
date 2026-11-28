// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_texture_array.h"

#include <utility>

namespace gb {

WebGpuTextureArray* WebGpuTextureArray::Create(
    ResourceEntry entry, WebGpuBackend* backend, DataVolatility volatility,
    int count, int width, int height, const SamplerOptions& options,
    TextureFormat format) {
  auto texels = WebGpuTexels::Create(
      backend, volatility,
      {.format = format, .width = width, .height = height, .depth = count});
  if (texels == nullptr) {
    return nullptr;
  }
  return new WebGpuTextureArray(std::move(entry), volatility, count, width,
                                height, options, format, std::move(texels));
}

WebGpuTextureArray::WebGpuTextureArray(ResourceEntry entry,
                                       DataVolatility volatility, int count,
                                       int width, int height,
                                       const SamplerOptions& options,
                                       TextureFormat format,
                                       std::unique_ptr<WebGpuTexels> texels)
    : TextureArray(std::move(entry), volatility, count, width, height, options,
                   format),
      texels_(std::move(texels)) {}

bool WebGpuTextureArray::DoClear(int index, const void* texel) {
  return texels_->Clear({0, 0, index, GetWidth(), GetHeight(), 1}, texel);
}

bool WebGpuTextureArray::DoSet(int index, const void* texels) {
  return texels_->Set({0, 0, index, GetWidth(), GetHeight(), 1}, texels,
                      GetWidth(), GetHeight());
}

bool WebGpuTextureArray::DoGet(int index, void* out_texels) {
  return texels_->GetLayer(index, out_texels);
}

}  // namespace gb
