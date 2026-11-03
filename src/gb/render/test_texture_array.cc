// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_texture_array.h"

#include <cstring>

namespace gb {

TestTextureArray::TestTextureArray(Config* config, ResourceEntry entry,
                                   DataVolatility volatility, int count,
                                   int width, int height,
                                   const SamplerOptions& options,
                                   TextureFormat format)
    : TextureArray(std::move(entry), volatility, count, width, height, options,
                   format),
      config_(config),
      texels_(GetTextureSizeInBytes(format, width, height, count), 0xFF) {}

bool TestTextureArray::DoClear(int index, const void* texel) {
  if (config_->fail_clear) {
    return false;
  }
  modify_count_ += 1;

  if (index < 0 || index >= GetCount()) {
    invalid_call_count_ += 1;
    return false;
  }

  const int texel_size = GetTexelSize(GetFormat());
  const int texel_count = GetWidth() * GetHeight();
  FillTexels(GetFormat(), texel, texel_count,
             texels_.data() + index * texel_count * texel_size);
  return true;
}

bool TestTextureArray::DoSet(int index, const void* texels) {
  if (config_->fail_set) {
    return false;
  }
  modify_count_ += 1;

  if (index < 0 || index >= GetCount()) {
    invalid_call_count_ += 1;
    return false;
  }

  const int64_t texture_size =
      GetTextureSizeInBytes(GetFormat(), GetWidth(), GetHeight());
  std::memcpy(texels_.data() + index * texture_size, texels, texture_size);
  return true;
}

bool TestTextureArray::DoGet(int index, void* out_texels) {
  if (config_->fail_get) {
    return false;
  }

  if (index < 0 || index >= GetCount()) {
    invalid_call_count_ += 1;
    return false;
  }

  const int64_t texture_size =
      GetTextureSizeInBytes(GetFormat(), GetWidth(), GetHeight());
  std::memcpy(out_texels, texels_.data() + index * texture_size, texture_size);
  return true;
}

}  // namespace gb
