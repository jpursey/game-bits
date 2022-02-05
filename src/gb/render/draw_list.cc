// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/draw_list.h"

#include "gb/render/mesh.h"
#include "gb/render/render_pipeline.h"

namespace gb {

void DrawList::ClearBindings() {
  current_material_type_ = nullptr;
  current_instance_data_ = nullptr;
  current_mesh_ = nullptr;
}

void DrawList::SetMesh(Mesh* mesh, Material* material,
                       BindingData* instance_data) {
  RENDER_ASSERT(mesh != nullptr);
  RENDER_ASSERT(material != nullptr || current_material_type_ != nullptr);
  current_mesh_ = nullptr;
  if (instance_data != nullptr) {
    current_instance_data_ = nullptr;
  }
  if (material != nullptr) {
    SetMaterial(material);
  }
  if (instance_data != nullptr) {
    SetInstanceData(instance_data);
  }
  RENDER_ASSERT(current_material_type_->GetVertexType() ==
                mesh->GetVertexType());
  commands_.emplace_back(DrawCommand::Type::kVertices,
                         mesh->GetVertexBuffer({}));
  commands_.emplace_back(DrawCommand::Type::kIndices, mesh->GetIndexBuffer({}));
  current_mesh_ = mesh;
}

void DrawList::SetMaterial(Material* material, BindingData* material_data) {
  RENDER_ASSERT(material != nullptr);
  if (material->GetType() != current_material_type_) {
    RENDER_ASSERT(current_mesh_ == nullptr ||
                  current_mesh_->GetVertexType() ==
                      material->GetType()->GetVertexType());
    RENDER_ASSERT(
        current_instance_data_ == nullptr ||
        material->GetType()->GetPipeline({})->ValidateInstanceBindingData(
            current_instance_data_));
    commands_.emplace_back(DrawCommand::Type::kPipeline,
                           material->GetType()->GetPipeline({}));
    current_material_type_ = material->GetType();
  }
  if (material_data == nullptr) {
    material_data = material->GetMaterialBindingData();
  }
  commands_.emplace_back(DrawCommand::Type::kMaterialData, material_data);
}

void DrawList::SetMaterialData(BindingData* material_data) {
  RENDER_ASSERT(material_data != nullptr);
  RENDER_ASSERT(current_material_type_ != nullptr ||
                current_material_type_->GetPipeline({}) ==
                    material_data->GetPipeline({}));
  commands_.emplace_back(DrawCommand::Type::kMaterialData, material_data);
}

void DrawList::SetInstanceData(BindingData* instance_data) {
  RENDER_ASSERT(instance_data != nullptr);
  RENDER_ASSERT(
      current_material_type_ != nullptr ||
      current_material_type_->GetPipeline({})->ValidateInstanceBindingData(
          instance_data));
  commands_.emplace_back(DrawCommand::Type::kInstanceData, instance_data);
  current_instance_data_ = instance_data;
}

void DrawList::SetScissor(int x, int y, int width, int height) {
  RENDER_ASSERT(x >= 0 && y >= 0 && width >= 0 && height >= 0);
  commands_.emplace_back(
      DrawCommand::Type::kScissor,
      DrawCommand::Rect{static_cast<uint16_t>(x), static_cast<uint16_t>(y),
                        static_cast<uint16_t>(width),
                        static_cast<uint16_t>(height)});
}

void DrawList::Draw() {
  RENDER_ASSERT(current_mesh_ != nullptr && current_instance_data_ != nullptr);
  commands_.emplace_back(
      DrawCommand::Type::kDraw,
      DrawCommand::Draw{
          0,
          static_cast<uint32_t>(current_mesh_->GetIndexBuffer({})->GetSize()),
          0});
}

void DrawList::DrawPartial(int first_triangle, int triangle_count,
                           int first_vertex) {
  RENDER_ASSERT(current_mesh_ != nullptr && current_instance_data_ != nullptr &&
                first_triangle >= 0 && triangle_count >= 0);
  commands_.emplace_back(
      DrawCommand::Type::kDraw,
      DrawCommand::Draw{static_cast<uint32_t>(first_triangle * 3),
                        static_cast<uint32_t>(triangle_count * 3),
                        static_cast<uint16_t>(first_vertex)});
}

void DrawList::Reset() {
  commands_.emplace_back(DrawCommand::Type::kReset);
  ClearBindings();
}

}  // namespace gb
