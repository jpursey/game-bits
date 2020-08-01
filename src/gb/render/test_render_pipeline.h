// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_PIPELINE_H_
#define GB_RENDER_TEST_RENDER_PIPELINE_H_

#include "gb/render/render_pipeline.h"
#include "gb/render/test_render_scene_type.h"
#include "gb/render/test_shader_code.h"

namespace gb {

class TestRenderPipeline final : public RenderPipeline {
 public:
  struct Config {
    Config() = default;
    bool fail_create_material_binding_data = false;
    bool fail_create_instance_binding_data = false;
  };

  TestRenderPipeline(Config* config, TestRenderSceneType* scene_type,
                     const VertexType* vertex_type,
                     absl::Span<const Binding> bindings,
                     TestShaderCode* vertex_shader,
                     TestShaderCode* fragment_shader);
  ~TestRenderPipeline() override;

  TestRenderSceneType* GetSceneType() const { return scene_type_; }
  const VertexType* GetVertexType() const { return vertex_type_; }
  absl::Span<const Binding> GetBindings() const { return bindings_; }
  TestShaderCode* GetVertexShader() const { return vertex_shader_; }
  TestShaderCode* GetFragmentShader() const { return fragment_shader_; }

  std::unique_ptr<BindingData> CreateMaterialBindingData() override;
  std::unique_ptr<BindingData> CreateInstanceBindingData() override;

 private:
  Config* const config_;
  TestRenderSceneType* const scene_type_;
  const VertexType* const vertex_type_;
  const std::vector<Binding> bindings_;
  TestShaderCode* const vertex_shader_;
  TestShaderCode* const fragment_shader_;
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_PIPELINE_H_
