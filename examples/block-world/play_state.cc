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
#include "imgui.h"

// Game includes
#include "cube.h"
#include "gui_fonts.h"
#include "world.h"
#include "world_resources.h"

PlayState::PlayState() {}
PlayState::~PlayState() {}

void PlayState::OnEnter() {
  BaseState::OnEnter();

  auto world = World::Create(Context());
  if (world == nullptr) {
    ExitState();
    return;
  }
  world_ = world.get();
  Context().SetOwned(std::move(world));

  world_resources_ = Context().GetPtr<WorldResources>();

  int z = 0;
  while (z < Chunk::kSize.z - 1 && world_->GetBlock(10, 10, z) != kBlockAir) {
    ++z;
  }
  z = std::min(z + 10, Chunk::kSize.z - 1);

  camera_.SetPosition({10, 10, z});
  camera_.SetDirection({0, 1, 0});
#ifdef NDEBUG
  camera_.SetViewDistance(640.0f);
#else
  camera_.SetViewDistance(96.0f);
#endif
}

void PlayState::OnUpdate(absl::Duration delta_time) {
  BaseState::OnUpdate(delta_time);

  if (camera_rotating_) {
    right_click_down_time_ += delta_time;
  }

  auto* render_system = GetRenderSystem();
  if (!render_system->BeginFrame()) {
    return;
  }

  if (camera_speed_mod_ != 0.0f || camera_strafe_mod_ != 0.0f) {
    glm::vec3 camera_position = camera_.GetPosition();
    float delta_seconds = static_cast<float>(absl::ToDoubleSeconds(delta_time));
    camera_position += camera_.GetDirection() * camera_speed_ *
                       camera_speed_mod_ * delta_seconds;
    camera_position += camera_.GetStrafe() * camera_speed_ *
                       camera_strafe_mod_ * delta_seconds;
    camera_position.z =
        std::clamp(camera_position.z, 0.0f, static_cast<float>(Chunk::kSize.z));
    camera_.SetPosition(camera_position);
  }

  ImGui::PushFont(GetGuiFonts()->console);

  world_->Draw(camera_);

  DrawGui();

  ImGui::PopFont();
  GetGuiInstance()->Draw();

  render_system->EndFrame();
}

void PlayState::DrawGui() {
  world_->DrawLightingGui();
  camera_.DrawGui();

  auto render_size = GetRenderSystem()->GetFrameDimensions();
  ImVec2 window_size = {static_cast<float>(render_size.width),
                        static_cast<float>(render_size.height)};

  static constexpr int kHudBlockSize = 50;
  static constexpr int kHudSpacing = 10;
  static constexpr int kNumBlocks = kLastSoldBlock - kFirstSolidBlock + 1;
  static constexpr int kHudWidth =
      (kHudBlockSize + kHudSpacing) * kNumBlocks - kHudSpacing;

  ImVec2 hud_start = {(window_size.x - kHudWidth) / 2,
                      window_size.y - kHudBlockSize - 20};
  ImGui::SetNextWindowPos({hud_start.x - 10, hud_start.y - 10});
  ImGui::SetNextWindowSize({static_cast<float>(kHudWidth) + 21,
                            static_cast<float>(kHudBlockSize) + 21});
  ImGui::Begin("HUD", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);

  ImGui::PushStyleColor(ImGuiCol_Text, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0, 0, 0, 0});
  ImGui::PushStyleColor(ImGuiCol_Header, {0, 0, 0, 0});
  for (BlockId block = kFirstSolidBlock; block <= kLastSoldBlock; ++block) {
    ImGui::SetCursorScreenPos(hud_start);
    ImGui::Image(
        world_resources_->GetBlockGuiTexture(),
        {static_cast<float>(kHudBlockSize), static_cast<float>(kHudBlockSize)},
        {kBlockUvOffset[block].x, kBlockUvOffset[block].y},
        {kBlockUvOffset[block].x + kBlockUvEndScale,
         kBlockUvOffset[block].y + kBlockUvEndScale},
        {1, 1, 1, 1},
        {1, 1, 1, static_cast<float>(block == selected_block_ ? 1 : 0)});
    ImGui::SetCursorScreenPos(hud_start);
    if (ImGui::Selectable(absl::StrCat("##HUD_", block).c_str(), true, 0,
                          {static_cast<float>(kHudBlockSize),
                           static_cast<float>(kHudBlockSize)})) {
      selected_block_ = block;
    }
    hud_start.x += kHudBlockSize + kHudSpacing;
  }
  ImGui::PopStyleColor(4);
  ImGui::End();
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
      default:
        break;
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
      default:
        break;
    }
    return false;
  }
  if (event.type == SDL_MOUSEBUTTONDOWN) {
    if (event.button.button == SDL_BUTTON_RIGHT) {
      right_click_down_time_ = absl::ZeroDuration();
      camera_rotating_ = true;
      SDL_GetGlobalMouseState(&mouse_pos_.x, &mouse_pos_.y);
      SDL_SetRelativeMouseMode(SDL_TRUE);
      return true;
    }
    if (event.button.button == SDL_BUTTON_LEFT && !camera_rotating_) {
      AddBlock(event.button.x, event.button.y);
    }
    return false;
  }
  if (event.type == SDL_MOUSEBUTTONUP) {
    if (event.button.button == SDL_BUTTON_RIGHT) {
      if (!camera_rotating_) {
        return false;
      }
      camera_rotating_ = false;
      SDL_SetRelativeMouseMode(SDL_FALSE);
      SDL_WarpMouseGlobal(mouse_pos_.x, mouse_pos_.y);
      if (right_click_down_time_ < absl::Seconds(0.25f)) {
        RemoveBlock(event.button.x, event.button.y);
      }
      return true;
    }
    return false;
  }
  if (event.type == SDL_MOUSEMOTION && camera_rotating_) {
    const float rotation = glm::radians(static_cast<float>(event.motion.xrel)) *
                           camera_sensitivity_;
    const float pitch = glm::radians(static_cast<float>(event.motion.yrel)) *
                        camera_sensitivity_;
    camera_.SetDirection(glm::rotate(
        glm::rotate(camera_.GetDirection(), -pitch, camera_.GetStrafe()),
        -rotation, kUpAxis));
    return true;
  }
  return false;
}

void PlayState::AddBlock(int screen_x, int screen_y) {
  const auto& frame_size = GetRenderSystem()->GetFrameDimensions();
  if (frame_size.width == 0 || frame_size.height == 0) {
    return;
  }
  glm::vec3 start = camera_.GetPosition();
  if (world_->GetBlock(start) != kBlockAir) {
    return;
  }
  glm::vec3 ray = camera_.CreateScreenRay(frame_size, screen_x, screen_y);
  HitInfo hit = world_->RayCast(start, ray, 50.0f);
  if (hit.block == kBlockAir) {
    return;
  }

  switch (hit.face) {
    case kCubePx:
      ++hit.index.x;
      break;
    case kCubeNx:
      --hit.index.x;
      break;
    case kCubePy:
      ++hit.index.y;
      break;
    case kCubeNy:
      --hit.index.y;
      break;
    case kCubePz:
      ++hit.index.z;
      break;
    case kCubeNz:
      --hit.index.z;
      break;
  }
  if (hit.index == glm::ivec3(std::floor(start.x), std::floor(start.y),
                              std::floor(start.z))) {
    return;
  }

  world_->SetBlock(hit.index, selected_block_);
}

void PlayState::RemoveBlock(int screen_x, int screen_y) {
  const auto& frame_size = GetRenderSystem()->GetFrameDimensions();
  if (frame_size.width == 0 || frame_size.height == 0) {
    return;
  }
  glm::vec3 start = camera_.GetPosition();
  if (world_->GetBlock(start) != kBlockAir) {
    return;
  }
  glm::vec3 ray = camera_.CreateScreenRay(frame_size, screen_x, screen_y);
  HitInfo hit = world_->RayCast(start, ray, 50.0f);
  if (hit.block == kBlockAir) {
    return;
  }

  world_->SetBlock(hit.index, kBlockAir);
}
