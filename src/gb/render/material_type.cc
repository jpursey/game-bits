// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/material_type.h"

#include "absl/container/inlined_vector.h"
#include "gb/render/local_binding_data.h"
#include "gb/render/render_pipeline.h"

namespace gb {

MaterialType::MaterialType(RenderInternal internal, ResourceEntry entry,
                           RenderSceneType* scene_type,
                           absl::Span<const Binding> bindings,
                           std::unique_ptr<RenderPipeline> pipeline,
                           const VertexType* vertex_type, Shader* vertex_shader,
                           Shader* fragment_shader)
    : Resource(std::move(entry)),
      scene_type_(scene_type),
      bindings_(bindings.begin(), bindings.end()),
      pipeline_(std::move(pipeline)),
      vertex_type_(vertex_type),
      vertex_shader_(vertex_shader),
      fragment_shader_(fragment_shader) {
  absl::InlinedVector<Binding, 16> material_bindings;
  absl::InlinedVector<Binding, 16> instance_bindings;
  for (const auto& binding : bindings) {
    if (binding.set == BindingSet::kMaterial) {
      material_bindings.push_back(binding);
    } else if (binding.set == BindingSet::kInstance) {
      instance_bindings.push_back(binding);
    }
  }
  material_defaults_ = std::make_unique<LocalBindingData>(
      internal, BindingSet::kMaterial, material_bindings);
  instance_defaults_ = std::make_unique<LocalBindingData>(
      internal, BindingSet::kInstance, instance_bindings);
}

MaterialType::~MaterialType() {}

void MaterialType::GetResourceDependencies(
    ResourceDependencyList* dependencies) const {
  dependencies->push_back(vertex_shader_);
  dependencies->push_back(fragment_shader_);
  material_defaults_->GetDependencies(dependencies);
  instance_defaults_->GetDependencies(dependencies);
}

}  // namespace gb
