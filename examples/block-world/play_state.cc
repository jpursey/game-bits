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
#include "gui_fonts.h"
#include "world.h"

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

  camera_.SetPosition({3, 15, 3});
  camera_.SetDirection({0.4f, -0.7f, 0.4f});
  camera_.SetViewDistance(100.0f);
}

void PlayState::OnUpdate(absl::Duration delta_time) {
  BaseState::OnUpdate(delta_time);

  auto* render_system = GetRenderSystem();
  if (!render_system->BeginFrame()) {
    return;
  }

  if (camera_speed_mod_ != 0.0f || camera_strafe_mod_ != 0.0f) {
    glm::vec3 camera_position = camera_.GetPosition();
    camera_position +=
        camera_.GetDirection() * camera_speed_ * camera_speed_mod_;
    camera_position += camera_.GetStrafe() * camera_speed_ * camera_strafe_mod_;
    camera_.SetPosition(camera_position);
  }

  world_->Draw(camera_);

  DrawGui();

  render_system->EndFrame();
}

void PlayState::DrawGui() {
  auto* fonts = GetGuiFonts();
  ImGui::PushFont(fonts->console);

  world_->DrawLightingGui();
  camera_.DrawGui();

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
    const float pitch = glm::radians(static_cast<float>(event.motion.yrel)) *
                        camera_sensitivity_;
    camera_.SetDirection(glm::rotate(
        glm::rotate(camera_.GetDirection(), -pitch, camera_.GetStrafe()),
        -rotation, kUpAxis));
    return true;
  }
  return false;
}