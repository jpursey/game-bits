// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_scene_type.h"

namespace gb {

RenderSceneType::RenderSceneType(absl::Span<const Binding> bindings)
    : bindings_(bindings.begin(), bindings.end()) {
  absl::InlinedVector<Binding, 16> scene_bindings;
  absl::InlinedVector<Binding, 16> material_bindings;
  absl::InlinedVector<Binding, 16> instance_bindings;
  for (const auto& binding : bindings) {
    switch (binding.set) {
      case BindingSet::kScene:
        scene_bindings.push_back(binding);
        break;
      case BindingSet::kMaterial:
        material_bindings.push_back(binding);
        break;
      case BindingSet::kInstance:
        instance_bindings.push_back(binding);
        break;
    }
  }
  scene_defaults_ = std::make_unique<LocalBindingData>(
      RenderInternal{}, BindingSet::kScene, scene_bindings);
  material_defaults_ = std::make_unique<LocalBindingData>(
      RenderInternal{}, BindingSet::kMaterial, material_bindings);
  instance_defaults_ = std::make_unique<LocalBindingData>(
      RenderInternal{}, BindingSet::kInstance, instance_bindings);
}

}  // namespace gb
