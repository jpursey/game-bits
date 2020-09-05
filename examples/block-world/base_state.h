#ifndef BASE_STATE_H_
#define BASE_STATE_H_

#include "SDL.h"
#include "gb/game/game_state.h"
#include "gb/imgui/imgui_types.h"
#include "gb/message/message_stack_endpoint.h"
#include "gb/render/render_types.h"

class BaseState : public gb::GameState {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: SDL_Window
  static GB_CONTEXT_CONSTRAINT(kConstraintWindow, kInRequired, SDL_Window);

  // REQUIRED: MessageStackEndpoint
  static GB_CONTEXT_CONSTRAINT(kConstraintStateEndpoint, kInRequired,
                               gb::MessageStackEndpoint);

  // REQUIRED: RenderSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintRenderSystem, kInRequired,
                               gb::RenderSystem);

  // REQUIRED: ImGuiInstance interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintGuiInstance, kInRequired,
                               gb::ImGuiInstance);

  using Contract =
      gb::ContextContract<kConstraintWindow, kConstraintStateEndpoint,
                          kConstraintRenderSystem, kConstraintGuiInstance>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  ~BaseState() override = default;

 protected:
  BaseState() = default;

  // Overrides of gb::GameState. These must be called by derived states if they
  // are overridden.
  void OnInit() override;
  void OnEnter() override;
  void OnExit() override;

  // Returns the message handlers for this state.
  gb::MessageStackHandlers& GetHandlers() { return handlers_; }
  SDL_Window* GetWindow() const { return window_; }
  gb::RenderSystem* GetRenderSystem() const { return render_system_; }
  gb::ImGuiInstance* GetGuiInstance() const { return gui_instance_; }

  // Processes an SDL event. Derived class should return true if the event is
  // completely handled, and parent states should not receive the event.
  virtual bool OnSdlEvent(const SDL_Event& event) { return false; }

 private:
  bool ProcessGuiEvent(const SDL_Event& event);

  SDL_Window* window_ = nullptr;
  gb::RenderSystem* render_system_ = nullptr;
  gb::ImGuiInstance* gui_instance_ = nullptr;
  gb::MessageStackEndpoint* endpoint_;
  gb::MessageStackHandlers handlers_;
};

#endif  // BASE_STATE_H_
