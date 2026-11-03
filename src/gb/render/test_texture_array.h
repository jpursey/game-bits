// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_TEXTURE_ARRAY_H_
#define GB_RENDER_TEST_TEXTURE_ARRAY_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "gb/render/render_types.h"
#include "gb/render/texture_array.h"

namespace gb {

// A texture array whose texels are kept in memory.
//
// The Pixel accessors are only valid for texture arrays in a Pixel format (see
// IsPixelFormat).
class TestTextureArray final : public TextureArray {
 public:
  struct Config {
    Config() = default;

    bool fail_clear = false;
    bool fail_set = false;
    bool fail_get = false;
  };

  TestTextureArray(Config* config, ResourceEntry entry,
                   DataVolatility volatility, int count, int width, int height,
                   const SamplerOptions& options, TextureFormat format);

  absl::Span<const uint8_t> GetTexels(int index) const {
    const int64_t texture_size =
        GetTextureSizeInBytes(GetFormat(), GetWidth(), GetHeight());
    return absl::MakeConstSpan(texels_.data() + index * texture_size,
                               texture_size);
  }
  Pixel GetPixel(int index, int x, int y) const {
    return GetPixels(index)[y * GetWidth() + x];
  }
  absl::Span<const Pixel> GetPixels(int index) const {
    return absl::MakeConstSpan(
        reinterpret_cast<const Pixel*>(GetTexels(index).data()),
        GetWidth() * GetHeight());
  }
  absl::Span<const uint32_t> GetPackedPixels(int index) const {
    return absl::MakeConstSpan(
        reinterpret_cast<const uint32_t*>(GetTexels(index).data()),
        GetWidth() * GetHeight());
  }

  int GetModifyCount() const { return modify_count_; }
  int GetInvalidCallCount() const { return invalid_call_count_; }

 protected:
  bool DoClear(int index, const void* texel) override;
  bool DoSet(int index, const void* texels) override;
  bool DoGet(int index, void* out_texels) override;

 private:
  Config* const config_;
  std::vector<uint8_t> texels_;
  int modify_count_ = 0;
  int invalid_call_count_ = 0;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_TEXTURE_ARRAY_H_
