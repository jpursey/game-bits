// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_VOLUME_TEXTURE_H_
#define GB_RENDER_TEST_VOLUME_TEXTURE_H_

#include <cstdint>
#include <vector>

#include "absl/types/span.h"
#include "gb/render/render_types.h"
#include "gb/render/volume_texture.h"

namespace gb {

// A volume texture whose texels are kept in memory.
class TestVolumeTexture final : public VolumeTexture {
 public:
  struct Config {
    Config() = default;

    bool fail_clear = false;
    bool fail_set = false;
    bool fail_get = false;
  };

  TestVolumeTexture(Config* config, ResourceEntry entry,
                    DataVolatility volatility, int width, int height, int depth,
                    const SamplerOptions& options, TextureFormat format);

  // Returns the texels, ordered x first, then y, then z.
  absl::Span<const uint8_t> GetTexels() const { return texels_; }

  int GetModifyCount() const { return modify_count_; }
  int GetInvalidCallCount() const { return invalid_call_count_; }

 protected:
  bool DoClear(int x, int y, int z, int width, int height, int depth,
               const void* texel) override;
  bool DoSet(int x, int y, int z, int width, int height, int depth,
             const void* texels, int stride, int slice_height) override;
  bool DoGet(void* out_texels) const override;

 private:
  // Returns true if the region is within the volume.
  bool IsInVolume(int x, int y, int z, int width, int height, int depth) const;

  // Returns the texels of the row at y and z, starting at x.
  uint8_t* GetRow(int x, int y, int z);

  Config* const config_;
  std::vector<uint8_t> texels_;
  int modify_count_ = 0;
  int invalid_call_count_ = 0;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_VOLUME_TEXTURE_H_
