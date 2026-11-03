// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_TEXTURE_H_
#define GB_RENDER_TEST_TEXTURE_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "gb/render/render_types.h"
#include "gb/render/texture.h"

namespace gb {

// A texture whose texels are kept in memory.
//
// The Pixel accessors are only valid for textures in a Pixel format (see
// IsPixelFormat).
class TestTexture final : public Texture {
 public:
  struct Config {
    Config() = default;

    bool fail_clear = false;
    bool fail_set = false;
    bool fail_get = false;
    bool fail_edit_begin = false;
  };

  TestTexture(Config* config, ResourceEntry entry, DataVolatility volatility,
              int width, int height, const SamplerOptions& options,
              TextureFormat format);

  absl::Span<const uint8_t> GetTexels() const { return texels_; }
  Pixel GetPixel(int x, int y) const { return GetPixels()[y * GetWidth() + x]; }
  absl::Span<const Pixel> GetPixels() const {
    return absl::MakeConstSpan(reinterpret_cast<const Pixel*>(texels_.data()),
                               GetWidth() * GetHeight());
  }
  absl::Span<const uint32_t> GetPackedPixels() const {
    return absl::MakeConstSpan(
        reinterpret_cast<const uint32_t*>(texels_.data()),
        GetWidth() * GetHeight());
  }
  std::vector<Pixel> GetPixelRegion(int x, int y, int width, int height) const;
  std::vector<uint32_t> GetPackedPixelRegion(int x, int y, int width,
                                             int height) const;

  int GetModifyCount() const { return modify_count_; }
  int GetInvalidCallCount() const { return invalid_call_count_; }

 protected:
  bool DoClear(int x, int y, int width, int height, const void* texel) override;
  bool DoSet(int x, int y, int width, int height, const void* texels,
             int stride) override;
  bool DoGet(void* out_texels) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;

 private:
  Config* const config_;
  std::vector<uint8_t> texels_;
  bool editing_ = false;
  int modify_count_ = 0;
  int invalid_call_count_ = 0;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_TEXTURE_H_
