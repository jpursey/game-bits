// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_SCENE_TYPE_H_
#define GB_RENDER_TEST_RENDER_SCENE_TYPE_H_

#include "gb/render/render_scene_type.h"

namespace gb {

class TestRenderSceneType final : public RenderSceneType {
 public:
  TestRenderSceneType(absl::Span<const Binding> bindings)
      : RenderSceneType(bindings) {}
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_SCENE_TYPE_H_
