// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_SCENE_H_
#define GB_RENDER_RENDER_SCENE_H_

#include "gb/render/binding_data.h"
#include "gb/render/render_scene_type.h"
#include "gb/render/render_types.h"

namespace gb {

// A RenderScene defines context for render resources that are drawn together as
// part of a single scene.
//
// All RenderSystem drawing methods require a scene. Every scene is defined by a
// scene type, and there may be multiple scenes all of the same type.
//
// This class and all derived classes must be thread-compatible.
class RenderScene {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  RenderScene(const RenderScene&) = delete;
  RenderScene(RenderScene&&) = delete;
  RenderScene& operator=(const RenderScene&) = delete;
  RenderScene& operator=(RenderScene&&) = delete;
  virtual ~RenderScene() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the scene type for this scene.
  //
  // The scene type is registered with the RenderSystem. It represents the how the
  // scene is handled by the RenderSystem, and what common bindings are defined for
  // all binding sets.
  RenderSceneType* GetType() const { return type_; }

  // Returns the scene order for this scene.
  //
  // The scene order is used to define a global processing order across scenes.
  // Scenes in an earlier order will be processed before scenes in a later
  // order. Scenes that have the same scene order will still be ordered relative
  // to each other, but in an indeterminate way. Scene order does not imply any
  // sort of memory dependency, so for instance it is not possible for a later
  // ordered scene to read the the results of a earlier ordered scene, but it
  // can be used to (for instance) ensure a UI scene is rendered on top of a 3D
  // scene.
  int GetOrder() const { return order_; }

  // Returns the scene binding data for this scene.
  //
  // This data is applied when rendering anything that uses this scene.
  const BindingData* GetSceneBindingData() const { return scene_data_.get(); }
  BindingData* GetSceneBindingData() { return scene_data_.get(); }

 protected:
  RenderScene(RenderSceneType* scene_type, int scene_order,
              std::unique_ptr<BindingData> scene_data)
      : type_(scene_type),
        order_(scene_order),
        scene_data_(std::move(scene_data)) {}

 private:
  RenderSceneType* const type_;
  const int order_;
  std::unique_ptr<BindingData> scene_data_;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_SCENE_H_
