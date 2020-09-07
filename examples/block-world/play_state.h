// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef DEMO_STATE_H_
#define DEMO_STATE_H_

#include <memory>

#include "base_state.h"
#include "gb/base/callback_scope.h"
#include "gb/base/validated_context.h"
#include "gb/file/file_types.h"
#include "gb/render/binding_data.h"
#include "gb/render/mesh.h"
#include "gb/render/render_scene.h"
#include "gb/resource/resource_set.h"
#include "gb/resource/resource_types.h"
#include "glm/glm.hpp"

class PlayState final : public BaseState {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: ResourceSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSystem, kInRequired,
                               gb::ResourceSystem);

  using Contract = gb::DerivedContextContract<BaseState::Contract,
                                              kConstraintResourceSystem>;

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
  void OnExit() override;
  void OnUpdate(absl::Duration delta_time) override;

 private:
  struct SceneData {
    glm::mat4 view_projection;
  };

  struct SceneLightData {
    glm::vec4 ambient;    // w is intensity
    glm::vec4 sun_color;  // w is intensity
    glm::vec3 sun_direction;
  };

  struct InstanceData {
    glm::mat4 model;
  };

  void DrawGui();

  gb::ResourceSet resources_;
  gb::Mesh* instance_mesh_ = nullptr;
  std::unique_ptr<gb::BindingData> instance_data_;
  std::unique_ptr<gb::RenderScene> scene_;
  glm::mat4 view_;
  SceneLightData lights_;
};

#endif  // DEMO_STATE_H_
