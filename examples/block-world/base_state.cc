// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "base_state.h"

#include "SDL.h"
#include "absl/log/log.h"
#include "imgui.h"
#include "imgui_impl_sdl.h"

bool BaseState::ProcessGuiEvent(const SDL_Event& event) {
  ImGui_ImplSDL2_ProcessEvent(&event);
  if (ImGui::GetIO().WantCaptureKeyboard) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
        event.type == SDL_TEXTEDITING || event.type == SDL_TEXTINPUT) {
      return true;
    }
  }
  if (ImGui::GetIO().WantCaptureMouse) {
    if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
        event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEWHEEL) {
      return true;
    }
  }
  if (ImGui::GetIO().WantTextInput) {
    if (event.type == SDL_TEXTEDITING || event.type == SDL_TEXTINPUT) {
      return true;
    }
  }
  return false;
}

void BaseState::OnInit() {
  handlers_.SetHandler<SDL_Event>(
      [this](gb::MessageEndpointId, const SDL_Event& event) {
        return ProcessGuiEvent(event) || OnSdlEvent(event);
      });
}

void BaseState::OnEnter() {
  window_ = Context().GetPtr<SDL_Window>();
  render_system_ = Context().GetPtr<gb::RenderSystem>();
  gui_instance_ = Context().GetPtr<gb::ImGuiInstance>();
  gui_fonts_ = Context().GetPtr<GuiFonts>();
  endpoint_ = Context().GetPtr<gb::MessageStackEndpoint>();
  if (window_ == nullptr || render_system_ == nullptr || endpoint_ == nullptr ||
      gui_instance_ == nullptr || gui_fonts_ == nullptr) {
    LOG(ERROR)
        << "Derived state did not include BaseState::Contract in its contract.";
    return;
  }

  endpoint_->Push(&handlers_);
}

void BaseState::OnExit() {
  if (endpoint_ != nullptr) {
    endpoint_->Remove(&handlers_);
    endpoint_ = nullptr;
  }
  render_system_ = nullptr;
  window_ = nullptr;
}
