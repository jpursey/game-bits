// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_test.h"

#include "absl/log/log.h"
#include "gb/base/context_builder.h"
#include "gb/file/memory_file_protocol.h"

namespace gb {

void RenderTest::CreateSystem(bool edit_mode) {
  CHECK(resource_system_ = ResourceSystem::Create());
  CHECK(file_system_ = std::make_unique<FileSystem>());
  CHECK(file_system_->Register(std::make_unique<MemoryFileProtocol>()));
  CHECK(render_system_ = RenderSystem::Create(
            ContextBuilder()
                .SetOwned<RenderBackend>(
                    std::make_unique<TestRenderBackend>(&state_))
                .SetPtr(resource_system_.get())
                .SetPtr(file_system_.get())
                .SetValue<bool>(RenderSystem::kKeyEnableEdit, edit_mode)
                .Build()));
}

std::unique_ptr<RenderPipeline> RenderTest::CreatePipeline(
    absl::Span<const Binding> bindings, const MaterialConfig& config) {
  auto* scene_type = render_system_->RegisterSceneType("scene", {});
  EXPECT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  EXPECT_NE(vertex_type, nullptr);
  auto* vertex_shader = render_system_->CreateShader(
      &temp_resource_set_, ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  EXPECT_NE(vertex_shader, nullptr);
  auto* fragment_shader = render_system_->CreateShader(
      &temp_resource_set_, ShaderType::kFragment,
      render_system_->CreateShaderCode(kFragmentShaderCode.data(),
                                       kFragmentShaderCode.size()),
      {}, {}, {});
  EXPECT_NE(fragment_shader, nullptr);
  if (scene_type == nullptr || vertex_type == nullptr ||
      vertex_shader == nullptr || fragment_shader == nullptr) {
    return nullptr;
  }
  return state_.backend->CreatePipeline(
      GetAccessToken(), scene_type, vertex_type, bindings,
      vertex_shader->GetCode(), fragment_shader->GetCode(), config);
}

MaterialType* RenderTest::CreateMaterialType(absl::Span<const Binding> bindings,
                                             const MaterialConfig& config) {
  auto* scene_type = render_system_->RegisterSceneType("scene", bindings);
  EXPECT_NE(scene_type, nullptr);
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  EXPECT_NE(vertex_type, nullptr);
  auto vertex_shader_code = render_system_->CreateShaderCode(
      kVertexShaderCode.data(), kVertexShaderCode.size());
  auto* vertex_shader =
      render_system_->CreateShader(&temp_resource_set_, ShaderType::kVertex,
                                   std::move(vertex_shader_code), {}, {}, {});
  EXPECT_NE(vertex_shader, nullptr);
  auto fragment_shader_code = render_system_->CreateShaderCode(
      kFragmentShaderCode.data(), kFragmentShaderCode.size());
  auto* fragment_shader =
      render_system_->CreateShader(&temp_resource_set_, ShaderType::kFragment,
                                   std::move(fragment_shader_code), {}, {}, {});
  EXPECT_NE(fragment_shader, nullptr);
  return render_system_->CreateMaterialType(&temp_resource_set_, scene_type,
                                            vertex_type, vertex_shader,
                                            fragment_shader, config);
}

Material* RenderTest::CreateMaterial(absl::Span<const Binding> bindings) {
  auto* material_type = CreateMaterialType(bindings);
  EXPECT_NE(material_type, nullptr);
  return render_system_->CreateMaterial(&temp_resource_set_, material_type);
}

}  // namespace gb
