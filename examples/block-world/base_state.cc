#include "base_state.h"

void BaseState::OnInit() {
  handlers_.SetHandler<SDL_Event>(
      [this](gb::MessageEndpointId, const SDL_Event& event) {
        return OnSdlEvent(event);
      });
}

void BaseState::OnEnter() {
  window_ = Context().GetPtr<SDL_Window>();
  render_system_ = Context().GetPtr<gb::RenderSystem>();
  endpoint_ = Context().GetPtr<gb::MessageStackEndpoint>();
  if (window_ == nullptr || render_system_ == nullptr || endpoint_ == nullptr) {
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
