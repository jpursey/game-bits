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
                                       TestShaderCode* fragment_shader,
                                       const MaterialConfig& material_config)
    : config_(config),
      scene_type_(scene_type),
      vertex_type_(vertex_type),
      bindings_(bindings.begin(), bindings.end()),
      vertex_shader_(vertex_shader),
      fragment_shader_(fragment_shader),
      material_config_(material_config) {}

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

bool TestRenderPipeline::ValidateInstanceBindingData(
    BindingData* binding_data) {
  // This is only used in RENDER_ASSERT calls, and there are not tests for this,
  // so we just return true for now.
  return true;
}

}  // namespace gb
