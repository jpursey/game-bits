// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_render_target.h"

#include <utility>

namespace gb {

TestRenderTarget::TestRenderTarget(Config* config, ResourceEntry entry,
                                   TextureFormat format,
                                   const RenderTargetSize& size, int width,
                                   int height, const SamplerOptions& options)
    : RenderTarget(std::move(entry), format, size, width, height, options),
      config_(config) {}

bool TestRenderTarget::DoResize(int width, int height) {
  if (config_->fail_resize) {
    return false;
  }
  resizes_.emplace_back(width, height);
  return true;
}

}  // namespace gb
