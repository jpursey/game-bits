// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_TEXTURE_H_
#define GB_RENDER_TEST_TEXTURE_H_

#include <vector>

#include "absl/types/span.h"
#include "gb/render/texture.h"

namespace gb {

class TestTexture final : public Texture {
 public:
  struct Config {
    Config() = default;

    bool fail_clear = false;
    bool fail_set = false;
    bool fail_edit_begin = false;
  };

  TestTexture(Config* config, ResourceEntry entry, DataVolatility volatility,
              int width, int height, const SamplerOptions& options);
  ~TestTexture() override;

  Pixel* GetPixelData() const { return pixels_; }
  Pixel GetPixel(int x, int y) const { return pixels_[y * GetWidth() + x]; }
  absl::Span<const Pixel> GetPixels() const {
    return absl::MakeConstSpan(pixels_, GetWidth() * GetHeight());
  }
  absl::Span<const uint32_t> GetPackedPixels() const {
    return absl::MakeConstSpan(reinterpret_cast<uint32_t*>(pixels_),
                               GetWidth() * GetHeight());
  }
  std::vector<Pixel> GetPixelRegion(int x, int y, int width, int height) const;
  std::vector<uint32_t> GetPackedPixelRegion(int x, int y, int width,
                                             int height) const;

  int GetModifyCount() const { return modify_count_; }
  int GetInvalidCallCount() const { return invalid_call_count_; }

 protected:
  bool DoClear(int x, int y, int width, int height, Pixel pixel) override;
  bool DoSet(int x, int y, int width, int height, const void* pixels,
             int stride) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;

 private:
  Config* const config_;
  Pixel* pixels_ = nullptr;
  bool editing_ = false;
  int modify_count_ = 0;
  int invalid_call_count_ = 0;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_TEXTURE_H_
