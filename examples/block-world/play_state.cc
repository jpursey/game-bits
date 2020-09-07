// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "play_state.h"

#include <cmath>

#include "absl/time/time.h"
#include "gb/imgui/imgui_instance.h"
#include "gb/render/material.h"
#include "gb/render/material_type.h"
#include "gb/render/mesh.h"
#include "gb/render/pixel_colors.h"
#include "gb/render/render_system.h"
#include "gb/render/shader.h"
#include "gb/render/texture.h"
#include "gb/resource/resource_system.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "glog/logging.h"
#include "gui_fonts.h"
#include "imgui.h"

namespace {

namespace Cube {

const glm::vec3 kPosition[8] = {
    {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
    {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
};

const glm::vec3 kSideNormal[6] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

const uint16_t kSideVertex[6][4] = {
    {5, 1, 2, 6}, {0, 4, 7, 3}, {3, 7, 6, 2},
    {0, 1, 5, 4}, {4, 5, 6, 7}, {1, 0, 3, 2},
};

const glm::vec2 kSideUv[4] = {{0, 0}, {0, 1}, {1, 1}, {1, 0}};

const gb::Triangle kSideTriangle[2] = {{0, 1, 2}, {0, 2, 3}};

}  // namespace Cube

struct Vertex {
  glm::vec3 pos;
  glm::vec3 normal;
  glm::vec2 uv;
  gb::Pixel color;
};

void AddCube(glm::vec3 position, std::vector<Vertex>* vertices,
             std::vector<gb::Triangle>* triangles) {
  Vertex vertex;
  vertex.color = gb::Colors::kWhite;
  uint16_t index = static_cast<uint16_t>(vertices->size());
  for (int s = 0; s < 6; ++s) {
    vertex.normal = Cube::kSideNormal[s];
    for (int v = 0; v < 4; ++v) {
      vertex.pos = position + Cube::kPosition[Cube::kSideVertex[s][v]];
      vertex.uv = Cube::kSideUv[v] * 0.5f;
      vertices->push_back(vertex);
    }
    for (int f = 0; f < 2; ++f) {
      gb::Triangle triangle = Cube::kSideTriangle[f];
      triangle.a += index;
      triangle.b += index;
      triangle.c += index;
      triangles->push_back(triangle);
    }
    index += 4;
  }
}

}  // namespace

PlayState::PlayState() {}
PlayState::~PlayState() {}

void PlayState::OnEnter() {
  BaseState::OnEnter();

  auto* render_system = GetRenderSystem();

  auto* resource_system = Context().GetPtr<gb::ResourceSystem>();

  auto vertex_shader_code =
      render_system->LoadShaderCode("asset:/shaders/block.vert.spv");
  if (vertex_shader_code == nullptr) {
    LOG(ERROR) << "Failed to load vertex shader";
    ExitState();
    return;
  }

  auto fragment_shader_code =
      render_system->LoadShaderCode("asset:/shaders/block.frag.spv");
  if (fragment_shader_code == nullptr) {
    LOG(ERROR) << "Failed to load fragment shader";
    ExitState();
    return;
  }

  const auto* vertex_type = render_system->RegisterVertexType<Vertex>(
      "Vertex", {gb::ShaderValue::kVec3, gb::ShaderValue::kVec3,
                 gb::ShaderValue::kVec2, gb::ShaderValue::kColor});
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Vertex type could not be registered (likely a size or "
                  "alignment issue)";
    ExitState();
    return;
  }

  const auto* scene_constants =
      render_system->RegisterConstantsType<SceneData>("SceneData");
  const auto* scene_light_constants =
      render_system->RegisterConstantsType<SceneLightData>("SceneLightData");
  const auto* instance_constants =
      render_system->RegisterConstantsType<InstanceData>("InstanceData");
  if (scene_constants == nullptr || scene_light_constants == nullptr ||
      instance_constants == nullptr) {
    LOG(ERROR) << "Could not register one or more render constants";
    ExitState();
    return;
  }

  gb::RenderSceneType* scene_type = render_system->RegisterSceneType(
      "Scene",
      {
          gb::Binding()
              .SetShaders(gb::ShaderType::kVertex)
              .SetLocation(gb::BindingSet::kScene, 0)
              .SetConstants(scene_constants, gb::DataVolatility::kPerFrame),
          gb::Binding()
              .SetShaders(gb::ShaderType::kFragment)
              .SetLocation(gb::BindingSet::kScene, 1)
              .SetConstants(scene_light_constants,
                            gb::DataVolatility::kPerFrame),
          gb::Binding()
              .SetShaders(gb::ShaderType::kVertex)
              .SetLocation(gb::BindingSet::kInstance, 0)
              .SetConstants(instance_constants, gb::DataVolatility::kPerFrame),

      });
  if (scene_type == nullptr) {
    LOG(ERROR) << "Could not register scene type";
    ExitState();
    return;
  }

  gb::Shader* vertex_shader = render_system->CreateShader(
      &resources_, gb::ShaderType::kVertex, std::move(vertex_shader_code),
      {
          gb::Binding()
              .SetShaders(gb::ShaderType::kVertex)
              .SetLocation(gb::BindingSet::kScene, 0)
              .SetConstants(scene_constants),
          gb::Binding()
              .SetShaders(gb::ShaderType::kVertex)
              .SetLocation(gb::BindingSet::kInstance, 0)
              .SetConstants(instance_constants),
      },
      {
          {gb::ShaderValue::kVec3, 0},  // in_pos
          {gb::ShaderValue::kVec3, 1},  // in_normal
          {gb::ShaderValue::kVec2, 2},  // in_uv
          {gb::ShaderValue::kVec4, 3},  // in_color
      },
      {
          {gb::ShaderValue::kVec3, 0},  // out_pos
          {gb::ShaderValue::kVec3, 1},  // out_normal
          {gb::ShaderValue::kVec2, 2},  // out_uv
          {gb::ShaderValue::kVec4, 3},  // out_color
      });
  if (vertex_shader == nullptr) {
    LOG(ERROR) << "Could not create vertex shader";
    ExitState();
    return;
  }

  gb::Shader* fragment_shader = render_system->CreateShader(
      &resources_, gb::ShaderType::kFragment, std::move(fragment_shader_code),
      {
          gb::Binding()
              .SetShaders(gb::ShaderType::kFragment)
              .SetLocation(gb::BindingSet::kScene, 1)
              .SetConstants(scene_light_constants),
          gb::Binding()
              .SetShaders(gb::ShaderType::kFragment)
              .SetLocation(gb::BindingSet::kMaterial, 0)
              .SetTexture(),
      },
      {
          {gb::ShaderValue::kVec3, 0},  // in_pos
          {gb::ShaderValue::kVec3, 1},  // in_normal
          {gb::ShaderValue::kVec2, 2},  // in_uv
          {gb::ShaderValue::kVec4, 3},  // in_color
      },
      {
          {gb::ShaderValue::kVec4, 0},  // out_color
      });
  if (fragment_shader == nullptr) {
    LOG(ERROR) << "Could not create fragment shader";
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
      &resources_, "asset:/textures/block.png");
  if (texture == nullptr) {
    LOG(ERROR) << "Failed to load texture";
    ExitState();
    return;
  }
  material->GetMaterialBindingData()->SetTexture(0, texture);

  std::vector<Vertex> vertices;
  std::vector<gb::Triangle> triangles;
  AddCube(glm::vec3(0.0f), &vertices, &triangles);
  gb::Mesh* mesh = render_system->CreateMesh(
      &resources_, material, gb::DataVolatility::kStaticWrite,
      static_cast<int>(vertices.size()), static_cast<int>(triangles.size()));
  if (mesh == nullptr) {
    LOG(ERROR) << "Could not create mesh";
    ExitState();
    return;
  }
  if (!mesh->Set<Vertex>(vertices, triangles)) {
    LOG(ERROR) << "Could not initialize mesh";
    ExitState();
    return;
  }
  instance_mesh_ = mesh;
  instance_data_ = material->CreateInstanceBindingData();
  instance_data_->SetConstants(0, InstanceData{glm::mat4(1.0f)});

  scene_ = render_system->CreateScene(scene_type);
  if (scene_ == nullptr) {
    LOG(ERROR) << "Could not create scene";
    ExitState();
    return;
  }

  lights_ = {
      // Ambient
      {1.0f, 1.0f, 1.0f, 0.1f},  // White light at 10% brightness

      // Sun
      {1.0f, 0.91f, 0.655f, 1.0f},  // Light yellow light, full bright
      glm::normalize(
          glm::vec3(0.15f, -0.4f, -0.8f)),  // Down at a convenient angle
  };
  scene_->GetSceneBindingData()->SetConstants(1, lights_);

  view_ = glm::lookAt(camera_pos_, camera_pos_ + camera_dir_,
                      glm::vec3(0.0f, 1.0f, 0.0f));
  camera_strafe_ = glm::normalize(glm::cross(camera_dir_, glm::vec3(0, 1, 0)));
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

  if (camera_speed_mod_ != 0.0f || camera_strafe_mod_ != 0.0f) {
    camera_pos_ += camera_dir_ * camera_speed_ * camera_speed_mod_;
    camera_pos_ += camera_strafe_ * camera_speed_ * camera_strafe_mod_;
    view_ = glm::lookAt(camera_pos_, camera_pos_ + camera_dir_,
                        glm::vec3(0.0f, 1.0f, 0.0f));
  }

  auto size = render_system->GetFrameDimensions();
  if (size.width == 0) {
    size.width = 1;
  }
  if (size.height == 0) {
    size.height = 1;
  }
  glm::mat4 projection = glm::perspective(
      glm::radians(45.0f), size.width / static_cast<float>(size.height), 0.1f,
      1000.0f);
  projection[1][1] *= -1.0f;
  scene_->GetSceneBindingData()->SetConstants(0, SceneData{projection * view_});

  render_system->Draw(scene_.get(), instance_mesh_, instance_data_.get());

  DrawGui();

  render_system->EndFrame();
}

void PlayState::DrawGui() {
  auto* fonts = GetGuiFonts();
  ImGui::PushFont(fonts->console);

  ImGui::SetNextWindowPos({20, 20}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({450, 160}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Lighting");
  bool modified = false;
  modified = ImGui::ColorEdit3("Ambient Color", &lights_.ambient.r) || modified;
  modified = ImGui::DragFloat("Ambient Brightness", &lights_.ambient.a, 0.001f,
                              0.0f, 1.0f) ||
             modified;
  modified = ImGui::ColorEdit3("Sun Color", &lights_.sun_color.r) || modified;
  modified = ImGui::DragFloat("Sun Brightness", &lights_.sun_color.a, 0.001f,
                              0.0f, 1.0f) ||
             modified;
  modified = ImGui::DragFloat3("Sun Direction", &lights_.sun_direction.x, 0.01f,
                               -1.0f, 1.0f) ||
             modified;
  if (modified) {
    scene_->GetSceneBindingData()->SetConstants(1, lights_);
  }
  ImGui::End();

  ImGui::SetNextWindowPos({20, 200}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({450, 130}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Camera");
  ImGui::DragFloat("Speed", &camera_speed_, 0.01f, 0.1f, 20.0f);
  ImGui::DragFloat("Sensitivity", &camera_sensitivity_, 0.01f, 0.1f, 3.0f);
  ImGui::InputFloat3("Position", &camera_pos_.x, 3);
  ImGui::InputFloat3("Direction", &camera_dir_.x, 3);
  ImGui::End();

  ImGui::PopFont();
  GetGuiInstance()->Draw();
}

bool PlayState::OnSdlEvent(const SDL_Event& event) {
  if (event.type == SDL_KEYDOWN) {
    switch (event.key.keysym.scancode) {
      case SDL_SCANCODE_W:
        camera_speed_mod_ = 1.0f;
        return true;
      case SDL_SCANCODE_S:
        camera_speed_mod_ = -1.0f;
        return true;
      case SDL_SCANCODE_A:
        camera_strafe_mod_ = -1.0f;
        return true;
      case SDL_SCANCODE_D:
        camera_strafe_mod_ = 1.0f;
        return true;
    }
    return false;
  }
  if (event.type == SDL_KEYUP) {
    auto state = SDL_GetKeyboardState(nullptr);
    switch (event.key.keysym.scancode) {
      case SDL_SCANCODE_W:
      case SDL_SCANCODE_S:
        if (state[SDL_SCANCODE_W]) {
          camera_speed_mod_ = 1.0f;
        } else if (state[SDL_SCANCODE_S]) {
          camera_speed_mod_ = -1.0f;
        } else {
          camera_speed_mod_ = 0.0f;
        }
        return true;
      case SDL_SCANCODE_A:
      case SDL_SCANCODE_D: {
        if (state[SDL_SCANCODE_A]) {
          camera_strafe_mod_ = -1.0f;
        } else if (state[SDL_SCANCODE_D]) {
          camera_strafe_mod_ = 1.0f;
        } else {
          camera_strafe_mod_ = 0.0f;
        }
        return true;
      }
    }
    return false;
  }
  if (event.type == SDL_MOUSEBUTTONDOWN) {
    if (event.button.button == SDL_BUTTON_RIGHT) {
      camera_rotating_ = true;
      SDL_GetGlobalMouseState(&mouse_pos_.x, &mouse_pos_.y);
      SDL_SetRelativeMouseMode(SDL_TRUE);
      return true;
    }
    return false;
  }
  if (event.type == SDL_MOUSEBUTTONUP) {
    if (event.button.button == SDL_BUTTON_RIGHT) {
      camera_rotating_ = false;
      SDL_SetRelativeMouseMode(SDL_FALSE);
      SDL_WarpMouseGlobal(mouse_pos_.x, mouse_pos_.y);
      return true;
    }
    return false;
  }
  if (event.type == SDL_MOUSEMOTION && camera_rotating_) {
    const float rotation = glm::radians(static_cast<float>(event.motion.xrel)) *
                           camera_sensitivity_;
    camera_dir_ = glm::rotate(camera_dir_, -rotation, glm::vec3(0, 1, 0));

    const float pitch = glm::radians(static_cast<float>(event.motion.yrel)) *
                        camera_sensitivity_;
    camera_strafe_ =
        glm::normalize(glm::cross(camera_dir_, glm::vec3(0, 1, 0)));
    camera_dir_ = glm::rotate(camera_dir_, -pitch, camera_strafe_);

    view_ = glm::lookAt(camera_pos_, camera_pos_ + camera_dir_,
                        glm::vec3(0.0f, 1.0f, 0.0f));
    return false;
  }
  return false;
}