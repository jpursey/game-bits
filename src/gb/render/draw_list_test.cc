// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/draw_list.h"

#include <memory>

#include "gb/render/binding_data.h"
#include "gb/render/material.h"
#include "gb/render/material_type.h"
#include "gb/render/mesh.h"
#include "gb/render/render_assert.h"
#include "gb/render/render_test.h"
#include "gb/render/test_binding_data.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

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
  }

  Material* material_ = nullptr;
  Mesh* mesh_ = nullptr;
  std::unique_ptr<BindingData> instance_data_;
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
  draw_list_.DrawPartial(0, 1, 65535);
  draw_list_.Reset();
  draw_list_.SetMaterial(material_);
  draw_list_.SetMesh(mesh_, instance_data_.get());
  draw_list_.Draw();
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
  EXPECT_DEATH(draw_list_.DrawPartial(0, 1, -1), "FitsUint16.first_vertex.");
  EXPECT_DEATH(draw_list_.DrawPartial(0, 1, 65536), "FitsUint16.first_vertex.");
}

TEST_F(DrawListDeathTest, MeshWithOtherVertexType) {
  auto* vertex_type = render_system_->RegisterVertexType<Vector2>(
      "other_vertex", {ShaderValue::kVec2});
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
