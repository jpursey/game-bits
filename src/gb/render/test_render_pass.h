// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_PASS_H_
#define GB_RENDER_TEST_RENDER_PASS_H_

#include "gb/render/render_pass.h"
#include "gb/render/render_types.h"

namespace gb {

class TestRenderPass final : public RenderPass {
 public:
  TestRenderPass(const RenderPassConfig& config, int pass_order)
      : RenderPass(config, pass_order) {}
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_PASS_H_
