// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/draw_list.h"

#include <memory>
#include <vector>

#include "absl/types/span.h"
#include "gb/render/binding_data.h"
#include "gb/render/material.h"
#include "gb/render/material_type.h"
#include "gb/render/mesh.h"
#include "gb/render/mesh_pool.h"
#include "gb/render/render_assert.h"
#include "gb/render/render_test.h"
#include "gb/render/test_binding_data.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using Type = DrawCommand::Type;

class DrawListTest : public RenderTest {
 protected:
  void SetUp() override {
    CreateSystem();
    SetRenderAssertEnabled(true);
    material_ = CreateMaterial({});
    ASSERT_NE(material_, nullptr);
    mesh_ = render_system_->CreateMesh(&temp_resource_set_,
                                       material_->GetType()->GetVertexType(),
                                       DataVolatility::kPerFrame, 3, 1);
    ASSERT_NE(mesh_, nullptr);
    instance_data_ = material_->CreateInstanceBindingData();
    ASSERT_NE(instance_data_, nullptr);

    pool_ = render_system_->CreateMeshPool(
        &temp_resource_set_, material_->GetType()->GetVertexType(),
        IndexType::kUint16, 16, 16);
    ASSERT_NE(pool_, nullptr);
    const std::vector<Vector3> vertices = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    const std::vector<Triangle> triangles = {{0, 1, 2}, {2, 3, 0}};
    first_pooled_mesh_ = pool_->CreateMesh<Vector3>(
        absl::MakeConstSpan(vertices).subspan(0, 3),
        absl::MakeConstSpan(triangles).subspan(0, 1));
    ASSERT_NE(first_pooled_mesh_, nullptr);
    pooled_mesh_ = pool_->CreateMesh<Vector3>(vertices, triangles);
    ASSERT_NE(pooled_mesh_, nullptr);
  }

  const std::vector<DrawCommand>& GetCommands() {
    return draw_list_.GetCommands(GetAccessToken());
  }

  // Returns the types of the commands recorded so far.
  std::vector<Type> GetTypes() {
    std::vector<Type> types;
    for (const DrawCommand& command : GetCommands()) {
      types.push_back(command.type);
    }
    return types;
  }

  // Returns a vertex type no material in the test uses.
  const VertexType* RegisterOtherVertexType() {
    return render_system_->RegisterVertexType<Vector2>("other_vertex",
                                                       {ShaderValue::kVec2});
  }

  Material* material_ = nullptr;
  Mesh* mesh_ = nullptr;
  std::unique_ptr<BindingData> instance_data_;
  MeshPool* pool_ = nullptr;

  // The first mesh in the pool, so `pooled_mesh_`'s offsets aren't zero.
  std::unique_ptr<PooledMesh> first_pooled_mesh_;

  // A mesh of 4 vertices and 2 triangles.
  std::unique_ptr<PooledMesh> pooled_mesh_;

  DrawList draw_list_;
};

class DrawListDeathTest : public DrawListTest {
 protected:
  void SetUp() override {
    if (!kRenderAssertsCompiled) {
      GTEST_SKIP() << "Render asserts are compiled out";
    }
    DrawListTest::SetUp();
  }
};

// Render asserts are enabled, so this fails if any valid call asserts.
TEST_F(DrawListTest, ValidCallsPassAsserts) {
  draw_list_.SetMesh(mesh_, material_, instance_data_.get());
  draw_list_.SetMaterialData(material_->GetMaterialBindingData());
  draw_list_.SetInstanceData(instance_data_.get());
  draw_list_.SetScissor(0, 0, 65535, 65535);
  draw_list_.Draw();
  draw_list_.DrawPartial(0, 1, 2);
  draw_list_.Reset();
  draw_list_.SetMaterial(material_);
  draw_list_.SetMesh(mesh_, instance_data_.get());
  draw_list_.Draw();
  draw_list_.SetMeshPool(pool_);
  draw_list_.SetMaterial(material_);
  draw_list_.Draw(pooled_mesh_->GetDraw());
  draw_list_.Reset();
  draw_list_.SetMeshPool(pool_, material_, instance_data_.get());
  draw_list_.Draw(pooled_mesh_->GetDraw(1, 1, 5));
  draw_list_.SetMaterial(material_);
  draw_list_.SetMeshPool(pool_, instance_data_.get());
  draw_list_.Draw(pooled_mesh_->GetDraw());
}

TEST_F(DrawListTest, MeshPoolCommands) {
  draw_list_.SetMeshPool(pool_, material_, instance_data_.get());
  draw_list_.Draw(pooled_mesh_->GetDraw(7));
  draw_list_.Draw(pooled_mesh_->GetDraw(1, 1, 9));
  ASSERT_EQ(GetTypes(), (std::vector<Type>{Type::kPipeline, Type::kMaterialData,
                                           Type::kInstanceData, Type::kMeshPool,
                                           Type::kDraw, Type::kDraw}));
  const std::vector<DrawCommand>& commands = GetCommands();
  EXPECT_EQ(commands[3].mesh_pool_buffers, pool_->GetBuffers(GetAccessToken()));

  // The pool's first mesh has 3 vertices and 1 triangle before this one.
  const DrawCommand::Draw& whole = commands[4].draw;
  EXPECT_EQ(whole.index_offset, 3);
  EXPECT_EQ(whole.index_count, 6);
  EXPECT_EQ(whole.vertex_offset, 3);
  EXPECT_EQ(whole.first_instance, 7);
  const DrawCommand::Draw& part = commands[5].draw;
  EXPECT_EQ(part.index_offset, 6);
  EXPECT_EQ(part.index_count, 3);
  EXPECT_EQ(part.vertex_offset, 3);
  EXPECT_EQ(part.first_instance, 9);
}

TEST_F(DrawListTest, MeshDrawsHaveFirstInstanceZero) {
  draw_list_.SetMesh(mesh_, material_, instance_data_.get());
  draw_list_.Draw();
  draw_list_.DrawPartial(0, 1, 2);
  ASSERT_EQ(GetTypes(),
            (std::vector<Type>{Type::kPipeline, Type::kMaterialData,
                               Type::kInstanceData, Type::kVertices,
                               Type::kIndices, Type::kDraw, Type::kDraw}));
  const std::vector<DrawCommand>& commands = GetCommands();
  const DrawCommand::Draw& whole = commands[5].draw;
  EXPECT_EQ(whole.index_offset, 0);
  EXPECT_EQ(whole.vertex_offset, 0);
  EXPECT_EQ(whole.first_instance, 0);
  const DrawCommand::Draw& part = commands[6].draw;
  EXPECT_EQ(part.index_offset, 0);
  EXPECT_EQ(part.index_count, 3);
  EXPECT_EQ(part.vertex_offset, 2);
  EXPECT_EQ(part.first_instance, 0);
}

TEST_F(DrawListTest, SwitchBetweenMeshAndPool) {
  draw_list_.SetMesh(mesh_, material_, instance_data_.get());
  draw_list_.Draw();
  draw_list_.SetMeshPool(pool_);
  draw_list_.Draw(pooled_mesh_->GetDraw());
  draw_list_.SetMesh(mesh_);
  draw_list_.Draw();
  EXPECT_EQ(GetTypes(),
            (std::vector<Type>{
                Type::kPipeline, Type::kMaterialData, Type::kInstanceData,
                Type::kVertices, Type::kIndices, Type::kDraw, Type::kMeshPool,
                Type::kDraw, Type::kVertices, Type::kIndices, Type::kDraw}));
}

TEST_F(DrawListDeathTest, MeshDrawsNeedAMesh) {
  draw_list_.SetMeshPool(pool_, material_, instance_data_.get());
  EXPECT_DEATH(draw_list_.Draw(), "current_mesh_ != nullptr");
  EXPECT_DEATH(draw_list_.DrawPartial(0, 1), "current_mesh_ != nullptr");
}

TEST_F(DrawListDeathTest, PoolDrawsNeedAPool) {
  const MeshDraw draw = pooled_mesh_->GetDraw();
  EXPECT_DEATH(draw_list_.Draw(draw), "current_mesh_pool_ != nullptr");
  draw_list_.SetMesh(mesh_, material_, instance_data_.get());
  EXPECT_DEATH(draw_list_.Draw(draw), "current_mesh_pool_ != nullptr");

  // A mesh replaces the pool.
  draw_list_.SetMeshPool(pool_);
  draw_list_.SetMesh(mesh_);
  EXPECT_DEATH(draw_list_.Draw(draw), "current_mesh_pool_ != nullptr");
}

TEST_F(DrawListDeathTest, PoolDrawWithoutInstanceData) {
  draw_list_.SetMeshPool(pool_, material_);
  EXPECT_DEATH(draw_list_.Draw(pooled_mesh_->GetDraw()),
               "current_instance_data_ != nullptr");
}

TEST_F(DrawListDeathTest, PoolDrawWithManyInstances) {
  draw_list_.SetMeshPool(pool_, material_, instance_data_.get());
  MeshDraw draw = pooled_mesh_->GetDraw();
  draw.instance_count = 2;
  EXPECT_DEATH(draw_list_.Draw(draw), "draw.instance_count == 1");
}

TEST_F(DrawListDeathTest, NullPool) {
  EXPECT_DEATH(draw_list_.SetMeshPool(nullptr, material_), "pool != nullptr");
}

TEST_F(DrawListDeathTest, PoolWithoutMaterial) {
  EXPECT_DEATH(draw_list_.SetMeshPool(pool_),
               "current_material_type_ != nullptr");
}

TEST_F(DrawListDeathTest, PoolWithOtherVertexType) {
  const VertexType* vertex_type = RegisterOtherVertexType();
  ASSERT_NE(vertex_type, nullptr);
  MeshPool* other_pool = render_system_->CreateMeshPool(
      &temp_resource_set_, vertex_type, IndexType::kUint16, 16, 16);
  ASSERT_NE(other_pool, nullptr);

  EXPECT_DEATH(draw_list_.SetMeshPool(other_pool, material_), "GetVertexType");
}

TEST_F(DrawListDeathTest, MaterialWithOtherVertexTypeThanPool) {
  const VertexType* vertex_type = RegisterOtherVertexType();
  ASSERT_NE(vertex_type, nullptr);
  MaterialType* material_type = material_->GetType();
  MaterialType* other_type = render_system_->CreateMaterialType(
      &temp_resource_set_, material_type->GetSceneType(), vertex_type,
      material_type->GetVertexShader(), material_type->GetFragmentShader());
  ASSERT_NE(other_type, nullptr);
  Material* other_material =
      render_system_->CreateMaterial(&temp_resource_set_, other_type);
  ASSERT_NE(other_material, nullptr);

  draw_list_.SetMeshPool(pool_, material_);
  EXPECT_DEATH(draw_list_.SetMaterial(other_material), "GetVertexType");

  // Once the pool is cleared, any material can be set.
  draw_list_.ClearBindings();
  draw_list_.SetMaterial(other_material);
}

TEST_F(DrawListDeathTest, NullArguments) {
  EXPECT_DEATH(draw_list_.SetMesh(nullptr, material_), "mesh != nullptr");
  EXPECT_DEATH(draw_list_.SetMaterial(nullptr), "material != nullptr");
  EXPECT_DEATH(draw_list_.SetMaterialData(nullptr), "material_data != nullptr");
  EXPECT_DEATH(draw_list_.SetInstanceData(nullptr), "instance_data != nullptr");
}

TEST_F(DrawListDeathTest, MeshWithoutMaterial) {
  EXPECT_DEATH(draw_list_.SetMesh(mesh_), "current_material_type_ != nullptr");
}

TEST_F(DrawListDeathTest, DrawWithoutMesh) {
  draw_list_.SetMaterial(material_);
  draw_list_.SetInstanceData(instance_data_.get());
  EXPECT_DEATH(draw_list_.Draw(), "current_mesh_ != nullptr");
  EXPECT_DEATH(draw_list_.DrawPartial(0, 1), "current_mesh_ != nullptr");
}

TEST_F(DrawListDeathTest, DrawWithoutInstanceData) {
  draw_list_.SetMesh(mesh_, material_);
  EXPECT_DEATH(draw_list_.Draw(), "current_instance_data_ != nullptr");
  EXPECT_DEATH(draw_list_.DrawPartial(0, 1),
               "current_instance_data_ != nullptr");
}

TEST_F(DrawListDeathTest, DrawPartialOutOfRange) {
  draw_list_.SetMesh(mesh_, material_, instance_data_.get());
  EXPECT_DEATH(draw_list_.DrawPartial(-1, 1), "first_triangle >= 0");
  EXPECT_DEATH(draw_list_.DrawPartial(0, -1), "triangle_count >= 0");
  EXPECT_DEATH(draw_list_.DrawPartial(0, 1, -1), "first_vertex >= 0");
  EXPECT_DEATH(draw_list_.DrawPartial(0, 1, 3), "GetVertexCapacity");
}

TEST_F(DrawListDeathTest, MeshWithOtherVertexType) {
  const VertexType* vertex_type = RegisterOtherVertexType();
  ASSERT_NE(vertex_type, nullptr);
  Mesh* other_mesh = render_system_->CreateMesh(
      &temp_resource_set_, vertex_type, DataVolatility::kPerFrame, 3, 1);
  ASSERT_NE(other_mesh, nullptr);

  EXPECT_DEATH(draw_list_.SetMesh(other_mesh, material_, instance_data_.get()),
               "GetVertexType");
}

TEST_F(DrawListDeathTest, InstanceDataWithoutMaterial) {
  EXPECT_DEATH(draw_list_.SetInstanceData(instance_data_.get()),
               "current_material_type_ != nullptr");
}

TEST_F(DrawListDeathTest, InstanceDataFromMaterialSet) {
  draw_list_.SetMaterial(material_);
  EXPECT_DEATH(draw_list_.SetInstanceData(material_->GetMaterialBindingData()),
               "ValidateInstanceBindingData");
}

TEST_F(DrawListDeathTest, MaterialDataFromOtherPipeline) {
  TestBindingData other_data(nullptr, BindingSet::kMaterial, {});
  EXPECT_DEATH(draw_list_.SetMaterialData(&other_data),
               "current_material_type_ != nullptr");
  draw_list_.SetMaterial(material_);
  EXPECT_DEATH(draw_list_.SetMaterialData(&other_data),
               "material_data->GetPipeline");
}

TEST_F(DrawListDeathTest, ScissorOutOfRange) {
  EXPECT_DEATH(draw_list_.SetScissor(-1, 0, 1, 1), "FitsUint16.x.");
  EXPECT_DEATH(draw_list_.SetScissor(0, 0, 65536, 1), "FitsUint16.width.");
}

}  // namespace
}  // namespace gb
