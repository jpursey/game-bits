// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MATERIAL_TYPE_H_
#define GB_RENDER_MATERIAL_TYPE_H_

#include <memory>

#include "absl/types/span.h"
#include "gb/render/binding_data.h"
#include "gb/render/local_binding_data.h"
#include "gb/render/render_types.h"
#include "gb/render/shader.h"
#include "gb/resource/resource.h"

namespace gb {

// This class represents a complete rendering pipeline, including shaders,
// vertex descriptions, and any other parameters that affect how rendering is
// peformed.
//
// Material types are required to create materials which are applied to mesh.
// Theses materials and mesh conform to the properties defined by the material
// type.
//
// A material type is also explicitly compatible with a specific scene type
// which defines common bindings and settings that all material types used in
// the scene must conform to.
//
// This class is thread-compatible.
class MaterialType final : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the shaders used in this material type.
  Shader* GetVertexShader() const { return vertex_shader_; }
  Shader* GetFragmentShader() const { return fragment_shader_; }

  // Returns the vertex type expected by the shaders and required by mesh
  // associated with materials of this type.
  const VertexType* GetVertexType() const { return vertex_type_; }

  //----------------------------------------------------------------------------
  // Binding data
  //----------------------------------------------------------------------------

  absl::Span<const Binding> GetBindings() const { return bindings_; }

  // Returns the default material and instance binding data for the material
  // type.
  //
  // Changing these defaults has no effect on existing Materials, or those
  // loaded via the resource system. They only affect newly created Material
  // instances.
  //
  // This is local cached data, and cannot be passed as binding data to
  // RenderSystem::Draw.
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
  // Resource overrides
  //----------------------------------------------------------------------------

  void GetResourceDependencies(
      ResourceDependencyList* dependencies) const override;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  MaterialType(RenderInternal, ResourceEntry entry,
               absl::Span<const Binding> bindings,
               std::unique_ptr<RenderPipeline> pipeline,
               const VertexType* vertex_type, Shader* vertex_shader,
               Shader* fragment_shader);
  RenderPipeline* GetPipeline(RenderInternal) const { return pipeline_.get(); }

 private:
  ~MaterialType() override;

  std::vector<Binding> bindings_;
  std::unique_ptr<RenderPipeline> pipeline_;
  const VertexType* const vertex_type_;
  Shader* const vertex_shader_;
  Shader* const fragment_shader_;
  std::unique_ptr<LocalBindingData> material_defaults_;
  std::unique_ptr<LocalBindingData> instance_defaults_;
};

}  // namespace gb

#endif  // GB_RENDER_MATERIAL_TYPE_H_
