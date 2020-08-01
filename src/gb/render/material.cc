// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/material.h"

#include "gb/render/render_pipeline.h"

namespace gb {

Material::Material(RenderInternal internal, ResourceEntry entry,
                   MaterialType* material_type)
    : Resource(std::move(entry)), material_type_(material_type) {
  std::unique_ptr<BindingData> material_binding_data =
      material_type_->GetPipeline({})->CreateMaterialBindingData();
  if (material_binding_data != nullptr) {
    material_type_->GetDefaultMaterialBindingData()->CopyTo(
        material_binding_data.get());
  }
  material_data_ = std::move(material_binding_data);
  instance_defaults_ = std::make_unique<LocalBindingData>(
      internal, *material_type_->GetDefaultInstanceBindingData());
}

Material::~Material() {}

std::unique_ptr<BindingData> Material::CreateInstanceBindingData() {
  std::unique_ptr<BindingData> binding_data =
      material_type_->GetPipeline({})->CreateInstanceBindingData();
  if (binding_data != nullptr) {
    instance_defaults_->CopyTo(binding_data.get());
  }
  return binding_data;
}

void Material::GetResourceDependencies(
    ResourceDependencyList* dependencies) const {
  dependencies->push_back(material_type_);
  material_data_->GetDependencies(dependencies);
  instance_defaults_->GetDependencies(dependencies);
}

}  // namespace gb
