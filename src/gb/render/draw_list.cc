// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/draw_list.h"

#include "gb/render/mesh.h"

namespace gb {

void DrawList::SetMesh(Mesh* mesh, BindingData* instance_data) {
  RENDER_ASSERT(mesh != nullptr);
  RENDER_ASSERT(instance_data != nullptr);
  RENDER_ASSERT(current_mesh_ == nullptr ||
                mesh->GetMaterial()->GetType()->GetSceneType() ==
                    current_mesh_->GetMaterial()->GetType()->GetSceneType());
  if (mesh != current_mesh_) {
    commands_.emplace_back(DrawCommand::Type::kPipeline,
                           mesh->GetMaterial()->GetType()->GetPipeline({}));
    commands_.emplace_back(DrawCommand::Type::kVertices,
                           mesh->GetVertexBuffer({}));
    commands_.emplace_back(DrawCommand::Type::kIndices,
                           mesh->GetIndexBuffer({}));
  }
  commands_.emplace_back(DrawCommand::Type::kMaterialData,
                         mesh->GetMaterial()->GetMaterialBindingData());
  commands_.emplace_back(DrawCommand::Type::kInstanceData, instance_data);
  current_mesh_ = mesh;
}

void DrawList::SetMaterialData(BindingData* material_data) {
  RENDER_ASSERT(material_data != nullptr);
  RENDER_ASSERT(current_mesh_ != nullptr &&
                current_mesh_->GetMaterial()->GetType()->GetPipeline({}) ==
                    material_data->GetPipeline({}));
  commands_.emplace_back(DrawCommand::Type::kMaterialData, material_data);
}

void DrawList::SetInstanceData(BindingData* instance_data) {
  RENDER_ASSERT(instance_data != nullptr);
  RENDER_ASSERT(current_mesh_ != nullptr &&
                current_mesh_->GetMaterial()->GetType()->GetPipeline({}) ==
                    instance_data->GetPipeline({}));
  commands_.emplace_back(DrawCommand::Type::kInstanceData, instance_data);
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
  RENDER_ASSERT(current_mesh_ != nullptr);
  commands_.emplace_back(
      DrawCommand::Type::kDraw,
      DrawCommand::Draw{
          0,
          static_cast<uint32_t>(current_mesh_->GetIndexBuffer({})->GetSize()),
          0});
}

void DrawList::DrawPartial(int first_triangle, int triangle_count,
                           int first_vertex) {
  RENDER_ASSERT(current_mesh_ != nullptr && first_triangle >= 0 &&
                triangle_count >= 0);
  commands_.emplace_back(
      DrawCommand::Type::kDraw,
      DrawCommand::Draw{static_cast<uint32_t>(first_triangle * 3),
                        static_cast<uint32_t>(triangle_count * 3),
                        static_cast<uint16_t>(first_vertex)});
}

void DrawList::Reset() {
  commands_.emplace_back(DrawCommand::Type::kReset);
  current_mesh_ = nullptr;
}

}  // namespace gb
