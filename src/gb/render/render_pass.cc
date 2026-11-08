// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_pass.h"

#include "absl/log/check.h"
#include "gb/render/render_system.h"

namespace gb {

RenderPass::RenderPass(const RenderPassConfig& config, int pass_order)
    : order_(pass_order),
      color_(config.color),
      depth_(config.depth),
      keep_color_(config.keep_color),
      keep_depth_(config.keep_depth),
      multisample_(config.multisample),
      clear_color_(config.clear_color) {}

RenderPass::~RenderPass() {
  CHECK_EQ(view_count_.load(std::memory_order_relaxed), 0)
      << "Render pass destroyed with views left in it";
  if (system_ != nullptr) {
    system_->RemovePass({}, this);
  }
}

}  // namespace gb
