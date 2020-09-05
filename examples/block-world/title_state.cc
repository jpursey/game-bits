#include "title_state.h"

#include <algorithm>

#include "gb/imgui/imgui_instance.h"
#include "gb/render/render_system.h"
#include "imgui.h"

void TitleState::OnEnter() { BaseState::OnEnter(); }

void TitleState::OnUpdate(absl::Duration delta_time) {
  BaseState::OnUpdate(delta_time);

  auto* render_system = GetRenderSystem();
  if (!render_system->BeginFrame()) {
    return;
  }

  static float f = 0.0f;
  static int counter = 0;
  static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  ImGui::Begin("Hello, world!");
  ImGui::Text("This is some useful text.");
  ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
  ImGui::ColorEdit3("clear color", (float*)&clear_color);
  if (ImGui::Button("Button")) {
    ++counter;
  }
  ImGui::SameLine();
  ImGui::Text("counter = %d", counter);
  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
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
