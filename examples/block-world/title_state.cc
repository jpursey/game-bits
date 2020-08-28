#include "title_state.h"

#include <algorithm>

#include "gb/render/render_system.h"

void TitleState::OnEnter() { BaseState::OnEnter(); }

void TitleState::OnUpdate(absl::Duration delta_time) {
  BaseState::OnUpdate(delta_time);

  auto* render_system = GetRenderSystem();
  if (!render_system->BeginFrame()) {
    return;
  }

  // TODO: Draw title screen

  render_system->EndFrame();
}

bool TitleState::OnSdlEvent(const SDL_Event& event) {
  if (event.type == SDL_KEYDOWN) {
    ChangeState<PlayState>();
    return true;
  }
  return false;
}
