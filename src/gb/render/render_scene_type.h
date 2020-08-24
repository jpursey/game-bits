// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_SCENE_TYPE_H_
#define GB_RENDER_RENDER_SCENE_TYPE_H_

#include "absl/types/span.h"
#include "gb/render/local_binding_data.h"
#include "gb/render/render_types.h"

namespace gb {

// This class defines common shader bindings and other settings for a render
// scene.
//
// All scenes and material types and their corresponding shaders, materials, and
// mesh conform to the settings defined by a RenderSceneType.
//
// This class and all derived classes must be thread-compatible.
class RenderSceneType {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  RenderSceneType(const RenderSceneType&) = delete;
  RenderSceneType(RenderSceneType&&) = delete;
  RenderSceneType& operator=(const RenderSceneType&) = delete;
  RenderSceneType& operator=(RenderSceneType&&) = delete;
  virtual ~RenderSceneType() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  const std::string& GetName() const { return name_; }

  //----------------------------------------------------------------------------
  // Binding Data
  //----------------------------------------------------------------------------

  // Returns the common bindings shared by all scenes, materials, and instances
  // that are associated with this scene type.
  absl::Span<const Binding> GetBindings() const { return bindings_; }

  // Returns the default scene, material, and instance binding data for the
  // scene type.
  //
  // Changing these defaults has no effect on existing RenderScenes,
  // MaterialTypes, or those loaded via the resource system. They only affect
  // newly created MaterialType instances.
  //
  // This is local cached data, and cannot be passed as binding data to
  // RenderSystem::Draw.
  const LocalBindingData* GetDefaultSceneBindingData() const {
    return scene_defaults_.get();
  }
  LocalBindingData* GetDefaultSceneBindingData() {
    return scene_defaults_.get();
  }

  const LocalBindingData* GetDefaultMaterialBindingData() const {
    return material_defaults_.get();
  }
  LocalBindingData* GetDefaultMaterialBindingData() {
    return material_defaults_.get();
  }

  const LocalBindingData* GetDefaultInstanceBindingData() const {
    return instance_defaults_.get();
  }
  LocalBindingData* GetDefaultInstanceBindingData() {
    return instance_defaults_.get();
  }

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  void SetName(RenderInternal, std::string_view name) {
    name_.assign(name.data(), name.size());
  }

 protected:
  RenderSceneType(absl::Span<const Binding> bindings);

 private:
  std::string name_;
  std::vector<Binding> bindings_;
  std::unique_ptr<LocalBindingData> scene_defaults_;
  std::unique_ptr<LocalBindingData> material_defaults_;
  std::unique_ptr<LocalBindingData> instance_defaults_;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_SCENE_TYPE_H_
