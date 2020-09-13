// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef DEMO_STATE_H_
#define DEMO_STATE_H_

#include <memory>

#include "gb/base/callback_scope.h"
#include "gb/base/validated_context.h"
#include "gb/file/file_types.h"
#include "gb/render/binding_data.h"
#include "gb/render/mesh.h"
#include "gb/render/render_scene.h"
#include "gb/resource/resource_set.h"
#include "gb/resource/resource_types.h"
#include "glm/glm.hpp"

// Game includes
#include "base_state.h"
#include "block.h"
#include "camera.h"
#include "game_types.h"

class PlayState final : public BaseState {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: ResourceSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSystem, kInRequired,
                               gb::ResourceSystem);

  // REQUIRED: WorldResources.
  static GB_CONTEXT_CONSTRAINT(kConstraintWorldResources, kInRequired,
                               WorldResources);

  // SCOPED: World.
  static GB_CONTEXT_CONSTRAINT(kConstraintWorld, kScoped, World);

  using Contract =
      gb::DerivedContextContract<BaseState::Contract, kConstraintResourceSystem,
                                 kConstraintWorldResources, kConstraintWorld>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  PlayState();
  ~PlayState() override;

 protected:
  //----------------------------------------------------------------------------
  // BaseState overrides
  //----------------------------------------------------------------------------

  void OnEnter() override;
  void OnUpdate(absl::Duration delta_time) override;
  bool OnSdlEvent(const SDL_Event& event) override;

 private:
  void DrawGui();

  void AddBlock(int screen_x, int screen_y);
  void RemoveBlock(int screen_x, int screen_y);

  World* world_ = nullptr;
  WorldResources* world_resources_ = nullptr;

  Camera camera_;
  glm::ivec2 mouse_pos_ = {0, 0};
  float camera_speed_ = 20.0f;
  float camera_speed_mod_ = 0.0f;
  float camera_strafe_mod_ = 0.0f;
  bool camera_rotating_ = false;
  float camera_sensitivity_ = 0.25f;
  BlockId selected_block_ = kFirstSolidBlock;
  absl::Duration right_click_down_time_;
};

#endif  // DEMO_STATE_H_
