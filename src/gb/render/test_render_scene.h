// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_SCENE_H_
#define GB_RENDER_TEST_RENDER_SCENE_H_

#include "gb/render/render_scene.h"
#include "gb/render/test_binding_data.h"

namespace gb {

class TestRenderScene final : public RenderScene {
 public:
  TestRenderScene(RenderSceneType* scene_type, int scene_order)
      : RenderScene(
            scene_type, scene_order,
            std::make_unique<TestBindingData>(nullptr, BindingSet::kScene,
                                              scene_type->GetBindings())) {}
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_SCENE_H_
