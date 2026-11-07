// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_TARGET_H_
#define GB_RENDER_TEST_RENDER_TARGET_H_

#include <utility>
#include <vector>

#include "gb/render/render_target.h"
#include "gb/render/render_types.h"

namespace gb {

// A render target that records each size it is resized to.
class TestRenderTarget final : public RenderTarget {
 public:
  struct Config {
    Config() = default;

    bool fail_resize = false;
  };

  TestRenderTarget(Config* config, ResourceEntry entry, TextureFormat format,
                   const RenderTargetSize& size, int width, int height,
                   const SamplerOptions& options);

  // Returns each width and height the target was resized to, in order.
  const std::vector<std::pair<int, int>>& GetResizes() const {
    return resizes_;
  }

 protected:
  bool DoResize(int width, int height) override;

 private:
  Config* const config_;
  std::vector<std::pair<int, int>> resizes_;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_TARGET_H_
