// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_PIPELINE_H_
#define GB_RENDER_TEST_RENDER_PIPELINE_H_

#include "gb/render/material_config.h"
#include "gb/render/render_pipeline.h"
#include "gb/render/render_types.h"
#include "gb/render/test_binding_layout.h"
#include "gb/render/test_shader_code.h"

namespace gb {

class TestRenderPipeline final : public RenderPipeline {
 public:
  struct Config {
    Config() = default;
    bool fail_create_material_binding_data = false;
    bool fail_create_instance_binding_data = false;
  };

  TestRenderPipeline(Config* config, TestBindingLayout* binding_layout,
                     const VertexType* vertex_type,
                     absl::Span<const Binding> bindings,
                     TestShaderCode* vertex_shader,
                     TestShaderCode* fragment_shader,
                     const MaterialConfig& material_config);
  ~TestRenderPipeline() override;

  const MaterialConfig& GetConfig() const { return material_config_; }
  TestBindingLayout* GetBindingLayout() const { return binding_layout_; }
  const VertexType* GetVertexType() const { return vertex_type_; }
  absl::Span<const Binding> GetBindings() const { return bindings_; }
  TestShaderCode* GetVertexShader() const { return vertex_shader_; }
  TestShaderCode* GetFragmentShader() const { return fragment_shader_; }

  std::unique_ptr<BindingData> CreateMaterialBindingData() override;
  std::unique_ptr<BindingData> CreateInstanceBindingData() override;
  bool ValidateInstanceBindingData(BindingData* binding_data) override;

 private:
  Config* const config_;
  TestBindingLayout* const binding_layout_;
  const VertexType* const vertex_type_;
  const std::vector<Binding> bindings_;
  TestShaderCode* const vertex_shader_;
  TestShaderCode* const fragment_shader_;
  const MaterialConfig material_config_;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_PIPELINE_H_
