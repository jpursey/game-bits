// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef BLOCK_WORLD_H_
#define BLOCK_WORLD_H_

#include "SDL.h"
#include "gb/file/file_system.h"
#include "gb/game/game.h"
#include "gb/game/game_state_machine.h"
#include "gb/imgui/imgui_instance.h"
#include "gb/message/message_dispatcher.h"
#include "gb/message/message_stack_endpoint.h"
#include "gb/message/message_system.h"
#include "gb/render/render_types.h"
#include "gb/resource/resource_system.h"
#include "gui_fonts.h"

class BlockWorld final : public gb::Game {
 public:
  // Context constraints for the game.
  static GB_CONTEXT_CONSTRAINT(kConstraintBlockWorld, kScoped, BlockWorld);
  static GB_CONTEXT_CONSTRAINT(kConstraintWindow, kScoped, SDL_Window);
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kScoped, gb::FileSystem);
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSystem, kScoped,
                               gb::ResourceSystem);
  static GB_CONTEXT_CONSTRAINT(kConstraintMessageSystem, kScoped,
                               gb::MessageSystem);
  static GB_CONTEXT_CONSTRAINT(kConstraintStateEndpoint, kScoped,
                               gb::MessageStackEndpoint);
  static inline constexpr char* kKeySdlEndpointId = "SdlEndpointId";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintSdlEndpointId, kScoped,
                                     gb::MessageEndpointId, kKeySdlEndpointId);
  static GB_CONTEXT_CONSTRAINT(kConstraintStateMachine, kScoped,
                               gb::GameStateMachine);
  static GB_CONTEXT_CONSTRAINT(kConstraintRenderSystem, kScoped,
                               gb::RenderSystem);
  static GB_CONTEXT_CONSTRAINT(kConstraintGuiInstance, kScoped,
                               gb::ImGuiInstance);
  static GB_CONTEXT_CONSTRAINT(kConstraintGuiFonts, kScoped, GuiFonts);
  using Contract = gb::DerivedContextContract<
      GameContract, kConstraintBlockWorld, kConstraintWindow,
      kConstraintFileSystem, kConstraintResourceSystem,
      kConstraintMessageSystem, kConstraintStateEndpoint,
      kConstraintSdlEndpointId, kConstraintStateMachine,
      kConstraintRenderSystem, kConstraintGuiInstance, kConstraintGuiFonts>;

  BlockWorld() = default;
  ~BlockWorld() override = default;

  gb::ValidatedContext& Context() override { return context_; }

 protected:
  // Game callbacks
  bool Init(const std::vector<std::string_view>& args) override;
  bool Update(absl::Duration delta_time) override;
  void CleanUp() override;

 private:
  bool InitFileSystem();
  bool InitResourceSystem();
  bool InitMessages();
  bool InitRenderSystem();
  bool InitGui();
  bool InitStates();

  void UpdateStateMachine(absl::Duration delta_time);

  gb::PollingMessageDispatcher dispatcher_;
  gb::ValidatedContext context_;
  gb::MessageSystem* message_system_ = nullptr;
  gb::MessageEndpointId sdl_channel_ = gb::kNoMessageEndpointId;
  gb::GameStateMachine* state_machine_ = nullptr;
  SDL_Window* window_ = nullptr;
  gb::RenderSystem* render_system_ = nullptr;
  gb::RenderBackend* render_backend_ = nullptr;
  gb::ImGuiInstance* gui_instance_ = nullptr;
  std::string init_state_name_;
};

#endif  // BLOCK_WORLD_H_
