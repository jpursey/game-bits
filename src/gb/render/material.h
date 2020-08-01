// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_MATERIAL_H_
#define GB_RENDER_MATERIAL_H_

#include <memory>

#include "gb/resource/resource.h"
#include "gb/render/binding_data.h"
#include "gb/render/local_binding_data.h"
#include "gb/render/material_type.h"
#include "gb/render/render_types.h"

namespace gb {

// A material is an instantiation of a material type.
//
// Materials may be applied to mesh and can contain overrides for any material
// binding data defined by its material type. Materials can also be used to
// generate instance binding data which is required to render mesh.
//
// This class is thread-compatible.
class Material final : public Resource {
 public:
  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the material type that defines the behavior of this material.
  MaterialType* GetType() const { return material_type_; }

  //----------------------------------------------------------------------------
  // Binding data
  //----------------------------------------------------------------------------

  // Creates binding data for kInstance binding set.
  //
  // Instance binding data is required to render mesh.
  std::unique_ptr<BindingData> CreateInstanceBindingData();

  // Returns the material binding data for this material.
  //
  // This data is applied when rendering all mesh that uses this material.
  const BindingData* GetMaterialBindingData() const {
    return material_data_.get();
  }
  BindingData* GetMaterialBindingData() { return material_data_.get(); }

  // Returns the default instance binding data for the material.
  //
  // This is local cached data, and cannot be passed as binding data to
  // RenderSystem::Draw.
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

  Material(RenderInternal, ResourceEntry entry,
           MaterialType* material_type);

 private:
  ~Material() override;

  MaterialType* material_type_;
  std::unique_ptr<BindingData> material_data_;
  std::unique_ptr<LocalBindingData> instance_defaults_;
};

}  // namespace gb

#endif  // GB_RENDER_MATERIAL_H_
