// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_render_pipeline.h"

#include "gb/render/test_binding_data.h"

namespace gb {

TestRenderPipeline::TestRenderPipeline(Config* config,
                                       TestRenderSceneType* scene_type,
                                       const VertexType* vertex_type,
                                       absl::Span<const Binding> bindings,
                                       TestShaderCode* vertex_shader,
                                       TestShaderCode* fragment_shader)
    : config_(config),
      scene_type_(scene_type),
      vertex_type_(vertex_type),
      bindings_(bindings.begin(), bindings.end()),
      vertex_shader_(vertex_shader),
      fragment_shader_(fragment_shader) {}

TestRenderPipeline::~TestRenderPipeline() {}

std::unique_ptr<BindingData> TestRenderPipeline::CreateMaterialBindingData() {
  if (config_->fail_create_material_binding_data) {
    return nullptr;
  }
  return std::make_unique<TestBindingData>(this, BindingSet::kMaterial,
                                           bindings_);
}

std::unique_ptr<BindingData> TestRenderPipeline::CreateInstanceBindingData() {
  if (config_->fail_create_instance_binding_data) {
    return nullptr;
  }
  return std::make_unique<TestBindingData>(this, BindingSet::kInstance,
                                           bindings_);
}

}  // namespace gb
