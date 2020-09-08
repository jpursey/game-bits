// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "title_state.h"

#include <algorithm>

#include "gb/imgui/imgui_instance.h"
#include "gb/render/render_system.h"
#include "imgui.h"

// Game includes
#include "gui_fonts.h"

namespace {

constexpr const char* kTitleText = "Block World";
constexpr const char* kPromptText = ">>> Press any key to begin <<<";

}  // namespace

void TitleState::OnEnter() { BaseState::OnEnter(); }

void TitleState::OnUpdate(absl::Duration delta_time) {
  BaseState::OnUpdate(delta_time);

  auto* render_system = GetRenderSystem();
  if (!render_system->BeginFrame()) {
    return;
  }

  auto render_size = render_system->GetFrameDimensions();
  ImVec2 window_size = {static_cast<float>(render_size.width),
                        static_cast<float>(render_size.height)};
  ImGui::SetNextWindowPos({0, 0});
  ImGui::SetNextWindowSize(window_size);
  ImGui::Begin("TitleState", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                   ImGuiWindowFlags_NoBringToFrontOnFocus);
  auto* fonts = GetGuiFonts();

  ImGui::PushFont(fonts->title);
  auto title_size = ImGui::CalcTextSize(kTitleText);
  ImGui::SetCursorPos({(window_size.x - title_size.x) / 2.0f, 100.0f});
  ImGui::TextColored(ImVec4(0.5f, 0.1f, 0.1f, 1.0f), kTitleText);
  ImGui::PopFont();

  ImGui::PushFont(fonts->prompt);
  auto prompt_size = ImGui::CalcTextSize(kPromptText);
  ImGui::SetCursorPos({(window_size.x - prompt_size.x) / 2.0f,
                       std::max(150.0f + title_size.y,
                                window_size.y - 100.0f - prompt_size.y)});
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), kPromptText);
  ImGui::PopFont();

  ImGui::End();
  GetGuiInstance()->Draw();

  render_system->EndFrame();
}

bool TitleState::OnSdlEvent(const SDL_Event& event) {
  if (event.type == SDL_KEYDOWN) {
    ChangeState<PlayState>();
    return true;
  }
  return false;
}
