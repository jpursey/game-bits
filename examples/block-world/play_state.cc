#include "play_state.h"

#include "absl/time/time.h"
#include "gb/render/material.h"
#include "gb/render/material_type.h"
#include "gb/render/mesh.h"
#include "gb/render/render_system.h"
#include "gb/render/shader.h"
#include "gb/render/texture.h"
#include "gb/resource/resource_system.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glog/logging.h"

namespace {

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 uv;
};

const Vertex kVertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

const uint16_t kIndices[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

}  // namespace

PlayState::PlayState() {}
PlayState::~PlayState() {}

void PlayState::OnEnter() {
  BaseState::OnEnter();

  auto* render_system = GetRenderSystem();

  auto* resource_system = Context().GetPtr<gb::ResourceSystem>();

  auto vertex_shader_code =
      render_system->LoadShaderCode("asset:/shaders/demo_shader.vert.spv");
  if (vertex_shader_code == nullptr) {
    LOG(ERROR) << "Failed to load vertex shader";
    ExitState();
    return;
  }

  auto fragment_shader_code =
      render_system->LoadShaderCode("asset:/shaders/demo_shader.frag.spv");
  if (fragment_shader_code == nullptr) {
    LOG(ERROR) << "Failed to load fragment shader";
    ExitState();
    return;
  }

  const auto* vertex_type = render_system->RegisterVertexType<Vertex>(
      "demo_vertex",
      {gb::ShaderValue::kVec3, gb::ShaderValue::kVec3, gb::ShaderValue::kVec2});
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Vertex type could not be registered (likely a size or "
                  "alignment issue)";
    ExitState();
    return;
  }

  gb::Shader* vertex_shader = render_system->CreateShader(
      &resources_, gb::ShaderType::kVertex, std::move(vertex_shader_code), {},
      {
          {gb::ShaderValue::kVec3, 0},  // inPosition
          {gb::ShaderValue::kVec3, 1},  // inColor
          {gb::ShaderValue::kVec2, 2},  // inUv
      },
      {
          {gb::ShaderValue::kVec3, 0},  // outColor
          {gb::ShaderValue::kVec2, 1},  // outUv
      });
  if (vertex_shader == nullptr) {
    LOG(ERROR) << "Could not create vertex shader";
    ExitState();
    return;
  }

  gb::Shader* fragment_shader = render_system->CreateShader(
      &resources_, gb::ShaderType::kFragment, std::move(fragment_shader_code),
      {gb::Binding()
           .SetShaders(gb::ShaderType::kFragment)
           .SetLocation(gb::BindingSet::kMaterial, 0)
           .SetTexture()},
      {
          {gb::ShaderValue::kVec3, 0},  // inColor
          {gb::ShaderValue::kVec2, 1},  // inUv
      },
      {});
  if (fragment_shader == nullptr) {
    LOG(ERROR) << "Could not create fragment shader";
    ExitState();
    return;
  }

  gb::RenderSceneType* scene_type = render_system->RegisterSceneType(
      "Scene",
      {gb::Binding()
           .SetLocation(gb::BindingSet::kScene, 0)
           .SetShaders(gb::ShaderType::kVertex)
           .SetConstants(render_system->RegisterConstantsType<ViewMatrices>(
                             "ViewMatrices"),
                         gb::DataVolatility::kPerFrame),
       gb::Binding()
           .SetLocation(gb::BindingSet::kInstance, 0)
           .SetShaders(gb::ShaderType::kVertex)
           .SetConstants(
               render_system->RegisterConstantsType<glm::mat4>("ModelMatrix"),
               gb::DataVolatility::kPerFrame)});
  if (scene_type == nullptr) {
    LOG(ERROR) << "Could not register scene type";
    ExitState();
    return;
  }

  gb::MaterialType* material_type = render_system->CreateMaterialType(
      &resources_, scene_type, vertex_type, vertex_shader, fragment_shader);
  if (material_type == nullptr) {
    LOG(ERROR) << "Could not create material type";
    ExitState();
    return;
  }

  gb::Material* material =
      render_system->CreateMaterial(&resources_, material_type);
  if (material == nullptr) {
    LOG(ERROR) << "Could not create material";
    ExitState();
    return;
  }

  gb::Texture* texture = resource_system->Load<gb::Texture>(
      &resources_, "asset:/textures/statue.jpg");
  if (texture == nullptr) {
    LOG(ERROR) << "Failed to load texture";
    ExitState();
    return;
  }
  material->GetMaterialBindingData()->SetTexture(0, texture);

  gb::Mesh* mesh = render_system->CreateMesh(
      &resources_, material, gb::DataVolatility::kStaticWrite,
      ABSL_ARRAYSIZE(kVertices), ABSL_ARRAYSIZE(kIndices));
  if (mesh == nullptr) {
    LOG(ERROR) << "Could not create mesh";
    ExitState();
    return;
  }
  auto vertex_span = absl::MakeConstSpan(kVertices, ABSL_ARRAYSIZE(kVertices));
  auto index_span = absl::MakeConstSpan(kIndices, ABSL_ARRAYSIZE(kIndices));
  if (!mesh->Set<Vertex>(vertex_span, index_span)) {
    LOG(ERROR) << "Could not initialize mesh";
    ExitState();
    return;
  }
  instance_mesh_ = mesh;
  instance_data_ = material->CreateInstanceBindingData();
  instance_data_->SetConstants(0, glm::mat4(1.0f));

  scene_ = render_system->CreateScene(scene_type);
  if (scene_ == nullptr) {
    LOG(ERROR) << "Could not create scene";
    ExitState();
    return;
  }

  view_matrices_.view =
      glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                  glm::vec3(0.0f, 0.0f, 1.0f));
}

void PlayState::OnExit() {
  instance_data_.reset();
  scene_.reset();
  resources_.RemoveAll();

  BaseState::OnExit();
}

void PlayState::OnUpdate(absl::Duration delta_time) {
  BaseState::OnUpdate(delta_time);

  static absl::Duration total_time;
  total_time += delta_time;

  auto* render_system = GetRenderSystem();
  if (!render_system->BeginFrame()) {
    return;
  }

  auto size = render_system->GetFrameDimensions();
  if (size.width == 0) {
    size.width = 1;
  }
  if (size.height == 0) {
    size.height = 1;
  }
  view_matrices_.projection = glm::perspective(
      glm::radians(45.0f), size.width / static_cast<float>(size.height), 0.1f,
      10.0f);
  view_matrices_.projection[1][1] *= -1.0f;
  scene_->GetSceneBindingData()->SetConstants(0, view_matrices_);

  const float seconds = static_cast<float>(absl::ToDoubleSeconds(total_time));
  instance_data_->SetConstants(
      0, glm::rotate(glm::mat4(1.0f), seconds * glm::radians(90.0f),
                     glm::vec3(0.0f, 0.0f, 1.0f)));
  render_system->Draw(scene_.get(), instance_mesh_, instance_data_.get());
  render_system->EndFrame();
}
