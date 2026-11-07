// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/binding_layout.h"

namespace gb {

BindingLayout::BindingLayout(absl::Span<const Binding> bindings)
    : bindings_(bindings.begin(), bindings.end()) {
  absl::InlinedVector<Binding, 16> view_bindings;
  absl::InlinedVector<Binding, 16> material_bindings;
  absl::InlinedVector<Binding, 16> instance_bindings;
  for (const auto& binding : bindings) {
    switch (binding.set) {
      case BindingSet::kView:
        view_bindings.push_back(binding);
        break;
      case BindingSet::kMaterial:
        material_bindings.push_back(binding);
        break;
      case BindingSet::kInstance:
        instance_bindings.push_back(binding);
        break;
    }
  }
  view_defaults_ = std::make_unique<LocalBindingData>(
      RenderInternal{}, BindingSet::kView, view_bindings);
  material_defaults_ = std::make_unique<LocalBindingData>(
      RenderInternal{}, BindingSet::kMaterial, material_bindings);
  instance_defaults_ = std::make_unique<LocalBindingData>(
      RenderInternal{}, BindingSet::kInstance, instance_bindings);
}

}  // namespace gb
