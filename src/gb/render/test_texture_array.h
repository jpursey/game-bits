// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_TEXTURE_ARRAY_H_
#define GB_RENDER_TEST_TEXTURE_ARRAY_H_

#include <vector>

#include "absl/types/span.h"
#include "gb/render/texture_array.h"

namespace gb {

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
                   const SamplerOptions& options);
  ~TestTextureArray() override;

  Pixel* GetPixelData() const { return pixels_; }
  Pixel GetPixel(int index, int x, int y) const {
    return pixels_[(index * GetHeight() + y) * GetWidth() + x];
  }
  absl::Span<const Pixel> GetPixels(int index) const {
    const int pixel_count = GetWidth() * GetHeight();
    return absl::MakeConstSpan(pixels_ + (index * pixel_count), pixel_count);
  }
  absl::Span<const uint32_t> GetPackedPixels(int index) const {
    const int pixel_count = GetWidth() * GetHeight();
    return absl::MakeConstSpan(
        reinterpret_cast<uint32_t*>(pixels_ + (index * pixel_count)),
        pixel_count);
  }

  int GetModifyCount() const { return modify_count_; }
  int GetInvalidCallCount() const { return invalid_call_count_; }

 protected:
  bool DoClear(int index, Pixel pixel) override;
  bool DoSet(int index, const void* pixels) override;
  bool DoGet(int index, void* out_pixels) override;

 private:
  Config* const config_;
  Pixel* pixels_ = nullptr;
  int modify_count_ = 0;
  int invalid_call_count_ = 0;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_TEXTURE_ARRAY_H_
