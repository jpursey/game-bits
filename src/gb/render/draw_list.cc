// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/draw_list.h"

#include <cstdint>

#include "gb/render/binding_data.h"
#include "gb/render/material.h"
#include "gb/render/material_type.h"
#include "gb/render/mesh.h"
#include "gb/render/mesh_pool.h"
#include "gb/render/render_assert.h"
#include "gb/render/render_pipeline.h"

namespace gb {

void DrawList::ClearBindings() {
  current_material_type_ = nullptr;
  current_instance_data_ = nullptr;
  current_mesh_ = nullptr;
  current_mesh_pool_ = nullptr;
}

const VertexType* DrawList::GetVertexType() const {
  if (current_mesh_ != nullptr) {
    return current_mesh_->GetVertexType();
  }
  if (current_mesh_pool_ != nullptr) {
    return current_mesh_pool_->GetVertexType();
  }
  return nullptr;
}

void DrawList::SetMeshBindings(const VertexType* vertex_type,
                               Material* material, BindingData* instance_data) {
  RENDER_ASSERT(material != nullptr || current_material_type_ != nullptr);
  current_mesh_ = nullptr;
  current_mesh_pool_ = nullptr;
  if (instance_data != nullptr) {
    current_instance_data_ = nullptr;
  }
  if (material != nullptr) {
    SetMaterial(material);
  }
  if (instance_data != nullptr) {
    SetInstanceData(instance_data);
  }
  RENDER_ASSERT(current_material_type_->GetVertexType() == vertex_type);
}

void DrawList::SetMesh(Mesh* mesh, Material* material,
                       BindingData* instance_data) {
  RENDER_ASSERT(mesh != nullptr);
  SetMeshBindings(mesh->GetVertexType(), material, instance_data);
  commands_.emplace_back(DrawCommand::Type::kVertices,
                         mesh->GetVertexBuffer({}));
  commands_.emplace_back(DrawCommand::Type::kIndices, mesh->GetIndexBuffer({}));
  current_mesh_ = mesh;
}

void DrawList::SetMeshPool(MeshPool* pool, Material* material,
                           BindingData* instance_data) {
  RENDER_ASSERT(pool != nullptr);
  SetMeshBindings(pool->GetVertexType(), material, instance_data);
  commands_.emplace_back(DrawCommand::Type::kMeshPool, pool->GetBuffers({}));
  current_mesh_pool_ = pool;
}

void DrawList::SetMaterial(Material* material, BindingData* material_data) {
  RENDER_ASSERT(material != nullptr);
  if (material->GetType() != current_material_type_) {
    RENDER_ASSERT(GetVertexType() == nullptr ||
                  GetVertexType() == material->GetType()->GetVertexType());
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
  RENDER_ASSERT(current_material_type_ != nullptr &&
                current_material_type_->GetPipeline({}) ==
                    material_data->GetPipeline({}));
  commands_.emplace_back(DrawCommand::Type::kMaterialData, material_data);
}

void DrawList::SetInstanceData(BindingData* instance_data) {
  RENDER_ASSERT(instance_data != nullptr);
  RENDER_ASSERT(
      current_material_type_ != nullptr &&
      current_material_type_->GetPipeline({})->ValidateInstanceBindingData(
          instance_data));
  commands_.emplace_back(DrawCommand::Type::kInstanceData, instance_data);
  current_instance_data_ = instance_data;
}

void DrawList::SetScissor(int x, int y, int width, int height) {
  RENDER_ASSERT(FitsUint16(x) && FitsUint16(y) && FitsUint16(width) &&
                FitsUint16(height));
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
          0, 0});
}

void DrawList::DrawPartial(int first_triangle, int triangle_count,
                           int first_vertex) {
  RENDER_ASSERT(current_mesh_ != nullptr);
  RENDER_ASSERT(current_instance_data_ != nullptr);
  RENDER_ASSERT(first_triangle >= 0);
  RENDER_ASSERT(triangle_count >= 0);
  RENDER_ASSERT(first_vertex >= 0 &&
                first_vertex < current_mesh_->GetVertexCapacity());
  commands_.emplace_back(
      DrawCommand::Type::kDraw,
      DrawCommand::Draw{static_cast<uint32_t>(first_triangle * 3),
                        static_cast<uint32_t>(triangle_count * 3), first_vertex,
                        0});
}

void DrawList::Draw(const MeshDraw& draw) {
  RENDER_ASSERT(current_mesh_pool_ != nullptr);
  RENDER_ASSERT(current_instance_data_ != nullptr);
  RENDER_ASSERT(draw.instance_count == 1);
  commands_.emplace_back(
      DrawCommand::Type::kDraw,
      DrawCommand::Draw{draw.first_index, draw.index_count, draw.vertex_offset,
                        draw.first_instance});
}

void DrawList::Reset() {
  commands_.emplace_back(DrawCommand::Type::kReset);
  ClearBindings();
}

}  // namespace gb
