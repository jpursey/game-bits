// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mesh.h"

#include "gb/render/render_test.h"
#include "gb/render/test_render_buffer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class MeshTest : public RenderTest {
 protected:
  // A cube, CCW faces
  const std::vector<Vector3> kCubeVertices = {
      {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
  };
  const std::vector<Triangle> kCubeTriangles = {
      {5, 1, 2}, {2, 6, 5}, {0, 4, 7}, {7, 3, 0}, {3, 7, 6}, {6, 2, 3},
      {0, 1, 5}, {5, 3, 0}, {4, 5, 6}, {6, 7, 4}, {1, 0, 3}, {3, 2, 1},
  };
  const std::vector<uint16_t> kCubeIndices = {
      5, 1, 2, 2, 6, 5, 0, 4, 7, 7, 3, 0, 3, 7, 6, 6, 2, 3,
      0, 1, 5, 5, 3, 0, 4, 5, 6, 6, 7, 4, 1, 0, 3, 3, 2, 1,
  };
};

TEST_F(MeshTest, CreateAsResourcePtr) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  ResourcePtr<Mesh> mesh = render_system_->CreateMesh(
      vertex_type, DataVolatility::kStaticWrite, 3, 1);
  ASSERT_NE(mesh, nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
  auto* test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  auto* test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);
}

TEST_F(MeshTest, CreateInResourceSet) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  ResourceSet resource_set;
  Mesh* mesh = render_system_->CreateMesh(&resource_set, vertex_type,
                                          DataVolatility::kStaticWrite,
                                          64 * 1024 - 1, 128 * 1024);
  ASSERT_NE(mesh, nullptr);
  EXPECT_EQ(resource_set.Get<Mesh>(mesh->GetResourceId()), mesh);

  EXPECT_EQ(state_.invalid_call_count, 0);
  auto* test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  auto* test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);
}

TEST_F(MeshTest, FailCreate) {
  CreateSystem();

  // Invalid material.
  EXPECT_EQ(
      render_system_->CreateMesh(nullptr, DataVolatility::kStaticWrite, 30, 30),
      nullptr);

  // Invalid number of vertices.
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  EXPECT_EQ(render_system_->CreateMesh(vertex_type,
                                       DataVolatility::kStaticWrite, 0, 30),
            nullptr);
  EXPECT_EQ(render_system_->CreateMesh(vertex_type,
                                       DataVolatility::kStaticWrite, 1, 30),
            nullptr);
  EXPECT_EQ(render_system_->CreateMesh(vertex_type,
                                       DataVolatility::kStaticWrite, 2, 30),
            nullptr);
  EXPECT_EQ(render_system_->CreateMesh(
                vertex_type, DataVolatility::kStaticWrite, 64 * 1024, 30),
            nullptr);

  // Invalid number of indices.
  EXPECT_EQ(
      render_system_->CreateMesh(nullptr, DataVolatility::kStaticWrite, 30, 0),
      nullptr);

  // Fail vertex buffer creation.
  state_.fail_create_vertex_buffer = true;
  EXPECT_EQ(render_system_->CreateMesh(vertex_type,
                                       DataVolatility::kStaticWrite, 30, 30),
            nullptr);

  // Fail index buffer creation.
  state_.ResetState();
  state_.fail_create_index_buffer = true;
  EXPECT_EQ(render_system_->CreateMesh(vertex_type,
                                       DataVolatility::kStaticWrite, 30, 30),
            nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MeshTest, Properties) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  auto mesh = render_system_->CreateMesh(
      vertex_type, DataVolatility::kStaticReadWrite, 30, 60);
  ASSERT_NE(mesh, nullptr);

  EXPECT_EQ(mesh->GetVertexType(), vertex_type);
  EXPECT_EQ(mesh->GetVolatility(), DataVolatility::kStaticReadWrite);
  EXPECT_EQ(mesh->GetVertexCount(), 0);
  EXPECT_EQ(mesh->GetVertexCapacity(), 30);
  EXPECT_EQ(mesh->GetTriangleCount(), 0);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 60);

  RenderBuffer* vertex_buffer = mesh->GetVertexBuffer(GetAccessToken());
  ASSERT_NE(vertex_buffer, nullptr);
  EXPECT_EQ(vertex_buffer->GetVolatility(), DataVolatility::kStaticReadWrite);
  EXPECT_EQ(vertex_buffer->GetValueSize(), sizeof(Vector3));
  EXPECT_EQ(vertex_buffer->GetCapacity(), 30);
  EXPECT_EQ(vertex_buffer->GetSize(), 0);

  RenderBuffer* index_buffer = mesh->GetIndexBuffer(GetAccessToken());
  ASSERT_NE(index_buffer, nullptr);
  EXPECT_EQ(index_buffer->GetVolatility(), DataVolatility::kStaticReadWrite);
  EXPECT_EQ(index_buffer->GetValueSize(), sizeof(uint16_t));
  EXPECT_EQ(index_buffer->GetCapacity(), 180);
  EXPECT_EQ(index_buffer->GetSize(), 0);

  EXPECT_EQ(state_.invalid_call_count, 0);
  auto* test_vertex_buffer = static_cast<TestRenderBuffer*>(vertex_buffer);
  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  auto* test_index_buffer = static_cast<TestRenderBuffer*>(index_buffer);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);
}

TEST_F(MeshTest, SetWithTriangles) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  auto mesh = render_system_->CreateMesh(vertex_type,
                                         DataVolatility::kStaticWrite, 10, 14);
  ASSERT_NE(mesh, nullptr);

  TestRenderBuffer* test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  ASSERT_NE(test_vertex_buffer, nullptr);

  TestRenderBuffer* test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  ASSERT_NE(test_index_buffer, nullptr);

  EXPECT_TRUE(mesh->Set<Vector3>(kCubeVertices, kCubeTriangles));
  EXPECT_EQ(mesh->GetVertexCount(), 8);
  EXPECT_EQ(mesh->GetVertexCapacity(), 10);
  EXPECT_EQ(mesh->GetTriangleCount(), 12);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 14);

  ASSERT_EQ(mesh->GetVertexBuffer(GetAccessToken()), test_vertex_buffer);
  EXPECT_EQ(test_vertex_buffer->GetSize(), mesh->GetVertexCount());
  EXPECT_EQ(test_vertex_buffer->GetCapacity(), mesh->GetVertexCapacity());
  EXPECT_EQ(std::memcmp(test_vertex_buffer->GetData(), kCubeVertices.data(),
                        kCubeVertices.size() * sizeof(Vector3)),
            0);

  ASSERT_EQ(mesh->GetIndexBuffer(GetAccessToken()), test_index_buffer);
  EXPECT_EQ(test_index_buffer->GetSize(), mesh->GetTriangleCount() * 3);
  EXPECT_EQ(test_index_buffer->GetCapacity(), mesh->GetTriangleCapacity() * 3);
  EXPECT_EQ(std::memcmp(test_index_buffer->GetData(), kCubeTriangles.data(),
                        kCubeTriangles.size() * sizeof(Triangle)),
            0);

  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);

  // Explicitly grow the capacity with the same data.
  EXPECT_TRUE(mesh->Set<Vector3>(kCubeVertices, kCubeTriangles, 15, 20));
  EXPECT_EQ(mesh->GetVertexCount(), 8);
  EXPECT_EQ(mesh->GetVertexCapacity(), 15);
  EXPECT_EQ(mesh->GetTriangleCount(), 12);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 20);

  EXPECT_NE(mesh->GetVertexBuffer(GetAccessToken()), test_vertex_buffer);
  test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  ASSERT_NE(test_vertex_buffer, nullptr);
  EXPECT_EQ(test_vertex_buffer->GetSize(), mesh->GetVertexCount());
  EXPECT_EQ(test_vertex_buffer->GetCapacity(), mesh->GetVertexCapacity());
  EXPECT_EQ(std::memcmp(test_vertex_buffer->GetData(), kCubeVertices.data(),
                        kCubeVertices.size() * sizeof(Vector3)),
            0);

  EXPECT_NE(mesh->GetIndexBuffer(GetAccessToken()), test_index_buffer);
  test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  ASSERT_NE(test_index_buffer, nullptr);
  EXPECT_EQ(test_index_buffer->GetSize(), mesh->GetTriangleCount() * 3);
  EXPECT_EQ(test_index_buffer->GetCapacity(), mesh->GetTriangleCapacity() * 3);
  EXPECT_EQ(std::memcmp(test_index_buffer->GetData(), kCubeTriangles.data(),
                        kCubeTriangles.size() * sizeof(Triangle)),
            0);

  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);

  // Implicitly grow the capacity based on the actual data.
  std::vector<Vector3> vertices(30, Vector3{100, 100, 100});
  std::vector<Triangle> triangles(40, Triangle{10, 10, 10});
  EXPECT_TRUE(mesh->Set<Vector3>(vertices, triangles));
  EXPECT_EQ(mesh->GetVertexCount(), 30);
  EXPECT_EQ(mesh->GetVertexCapacity(), 30);
  EXPECT_EQ(mesh->GetTriangleCount(), 40);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 40);

  EXPECT_NE(mesh->GetVertexBuffer(GetAccessToken()), test_vertex_buffer);
  test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  ASSERT_NE(test_vertex_buffer, nullptr);
  EXPECT_EQ(test_vertex_buffer->GetSize(), mesh->GetVertexCount());
  EXPECT_EQ(test_vertex_buffer->GetCapacity(), mesh->GetVertexCapacity());
  EXPECT_EQ(std::memcmp(test_vertex_buffer->GetData(), vertices.data(),
                        vertices.size() * sizeof(Vector3)),
            0);

  EXPECT_NE(mesh->GetIndexBuffer(GetAccessToken()), test_index_buffer);
  test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  ASSERT_NE(test_index_buffer, nullptr);
  EXPECT_EQ(test_index_buffer->GetSize(), mesh->GetTriangleCount() * 3);
  EXPECT_EQ(test_index_buffer->GetCapacity(), mesh->GetTriangleCapacity() * 3);
  EXPECT_EQ(std::memcmp(test_index_buffer->GetData(), triangles.data(),
                        triangles.size() * sizeof(Triangle)),
            0);

  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);

  // Shrink back to a cube.
  EXPECT_TRUE(mesh->Set<Vector3>(kCubeVertices, kCubeTriangles));
  EXPECT_EQ(mesh->GetVertexCount(), 8);
  EXPECT_EQ(mesh->GetVertexCapacity(), 30);
  EXPECT_EQ(mesh->GetTriangleCount(), 12);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 40);

  ASSERT_EQ(mesh->GetVertexBuffer(GetAccessToken()), test_vertex_buffer);
  EXPECT_EQ(test_vertex_buffer->GetSize(), mesh->GetVertexCount());
  EXPECT_EQ(test_vertex_buffer->GetCapacity(), mesh->GetVertexCapacity());
  EXPECT_EQ(std::memcmp(test_vertex_buffer->GetData(), kCubeVertices.data(),
                        kCubeVertices.size() * sizeof(Vector3)),
            0);

  ASSERT_EQ(mesh->GetIndexBuffer(GetAccessToken()), test_index_buffer);
  EXPECT_EQ(test_index_buffer->GetSize(), mesh->GetTriangleCount() * 3);
  EXPECT_EQ(test_index_buffer->GetCapacity(), mesh->GetTriangleCapacity() * 3);
  EXPECT_EQ(std::memcmp(test_index_buffer->GetData(), kCubeTriangles.data(),
                        kCubeTriangles.size() * sizeof(Triangle)),
            0);

  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 2);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 2);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MeshTest, SetWithIndices) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  auto mesh = render_system_->CreateMesh(vertex_type,
                                         DataVolatility::kStaticWrite, 10, 14);
  ASSERT_NE(mesh, nullptr);

  TestRenderBuffer* test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  ASSERT_NE(test_vertex_buffer, nullptr);

  TestRenderBuffer* test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  ASSERT_NE(test_index_buffer, nullptr);

  EXPECT_TRUE(mesh->Set<Vector3>(kCubeVertices, kCubeIndices));
  EXPECT_EQ(mesh->GetVertexCount(), 8);
  EXPECT_EQ(mesh->GetVertexCapacity(), 10);
  EXPECT_EQ(mesh->GetTriangleCount(), 12);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 14);

  ASSERT_EQ(mesh->GetVertexBuffer(GetAccessToken()), test_vertex_buffer);
  EXPECT_EQ(test_vertex_buffer->GetSize(), mesh->GetVertexCount());
  EXPECT_EQ(test_vertex_buffer->GetCapacity(), mesh->GetVertexCapacity());
  EXPECT_EQ(std::memcmp(test_vertex_buffer->GetData(), kCubeVertices.data(),
                        kCubeVertices.size() * sizeof(Vector3)),
            0);

  ASSERT_EQ(mesh->GetIndexBuffer(GetAccessToken()), test_index_buffer);
  EXPECT_EQ(test_index_buffer->GetSize(), mesh->GetTriangleCount() * 3);
  EXPECT_EQ(test_index_buffer->GetCapacity(), mesh->GetTriangleCapacity() * 3);
  EXPECT_EQ(std::memcmp(test_index_buffer->GetData(), kCubeIndices.data(),
                        kCubeIndices.size() * sizeof(uint16_t)),
            0);

  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);

  // Explicitly grow the capacity with the same data.
  EXPECT_TRUE(mesh->Set<Vector3>(kCubeVertices, kCubeIndices, 15, 20));
  EXPECT_EQ(mesh->GetVertexCount(), 8);
  EXPECT_EQ(mesh->GetVertexCapacity(), 15);
  EXPECT_EQ(mesh->GetTriangleCount(), 12);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 20);

  EXPECT_NE(mesh->GetVertexBuffer(GetAccessToken()), test_vertex_buffer);
  test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  ASSERT_NE(test_vertex_buffer, nullptr);
  EXPECT_EQ(test_vertex_buffer->GetSize(), mesh->GetVertexCount());
  EXPECT_EQ(test_vertex_buffer->GetCapacity(), mesh->GetVertexCapacity());
  EXPECT_EQ(std::memcmp(test_vertex_buffer->GetData(), kCubeVertices.data(),
                        kCubeVertices.size() * sizeof(Vector3)),
            0);

  EXPECT_NE(mesh->GetIndexBuffer(GetAccessToken()), test_index_buffer);
  test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  ASSERT_NE(test_index_buffer, nullptr);
  EXPECT_EQ(test_index_buffer->GetSize(), mesh->GetTriangleCount() * 3);
  EXPECT_EQ(test_index_buffer->GetCapacity(), mesh->GetTriangleCapacity() * 3);
  EXPECT_EQ(std::memcmp(test_index_buffer->GetData(), kCubeIndices.data(),
                        kCubeIndices.size() * sizeof(uint16_t)),
            0);

  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);

  // Implicitly grow the capacity based on the actual data.
  std::vector<Vector3> vertices(30, Vector3{100, 100, 100});
  std::vector<uint16_t> indices(120, 10);
  EXPECT_TRUE(mesh->Set<Vector3>(vertices, indices));
  EXPECT_EQ(mesh->GetVertexCount(), 30);
  EXPECT_EQ(mesh->GetVertexCapacity(), 30);
  EXPECT_EQ(mesh->GetTriangleCount(), 40);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 40);

  EXPECT_NE(mesh->GetVertexBuffer(GetAccessToken()), test_vertex_buffer);
  test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  ASSERT_NE(test_vertex_buffer, nullptr);
  EXPECT_EQ(test_vertex_buffer->GetSize(), mesh->GetVertexCount());
  EXPECT_EQ(test_vertex_buffer->GetCapacity(), mesh->GetVertexCapacity());
  EXPECT_EQ(std::memcmp(test_vertex_buffer->GetData(), vertices.data(),
                        vertices.size() * sizeof(Vector3)),
            0);

  EXPECT_NE(mesh->GetIndexBuffer(GetAccessToken()), test_index_buffer);
  test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  ASSERT_NE(test_index_buffer, nullptr);
  EXPECT_EQ(test_index_buffer->GetSize(), mesh->GetTriangleCount() * 3);
  EXPECT_EQ(test_index_buffer->GetCapacity(), mesh->GetTriangleCapacity() * 3);
  EXPECT_EQ(std::memcmp(test_index_buffer->GetData(), indices.data(),
                        indices.size() * sizeof(uint16_t)),
            0);

  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);

  // Shrink back to a cube.
  EXPECT_TRUE(mesh->Set<Vector3>(kCubeVertices, kCubeIndices));
  EXPECT_EQ(mesh->GetVertexCount(), 8);
  EXPECT_EQ(mesh->GetVertexCapacity(), 30);
  EXPECT_EQ(mesh->GetTriangleCount(), 12);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 40);

  ASSERT_EQ(mesh->GetVertexBuffer(GetAccessToken()), test_vertex_buffer);
  EXPECT_EQ(test_vertex_buffer->GetSize(), mesh->GetVertexCount());
  EXPECT_EQ(test_vertex_buffer->GetCapacity(), mesh->GetVertexCapacity());
  EXPECT_EQ(std::memcmp(test_vertex_buffer->GetData(), kCubeVertices.data(),
                        kCubeVertices.size() * sizeof(Vector3)),
            0);

  ASSERT_EQ(mesh->GetIndexBuffer(GetAccessToken()), test_index_buffer);
  EXPECT_EQ(test_index_buffer->GetSize(), mesh->GetTriangleCount() * 3);
  EXPECT_EQ(test_index_buffer->GetCapacity(), mesh->GetTriangleCapacity() * 3);
  EXPECT_EQ(std::memcmp(test_index_buffer->GetData(), kCubeIndices.data(),
                        kCubeIndices.size() * sizeof(uint16_t)),
            0);

  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 2);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 2);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);

  EXPECT_EQ(state_.invalid_call_count, 0);
}

TEST_F(MeshTest, FailSet) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  auto mesh = render_system_->CreateMesh(vertex_type,
                                         DataVolatility::kStaticWrite, 30, 60);
  ASSERT_NE(mesh, nullptr);

  ASSERT_TRUE(mesh->Set<Vector3>({kCubeVertices.data(), 3},
                                 {kCubeTriangles.data(), 3}));

  state_.vertex_buffer_config.fail_set = true;
  EXPECT_FALSE(mesh->Set<Vector3>(kCubeVertices, kCubeTriangles));
  EXPECT_FALSE(mesh->Set<Vector3>(kCubeVertices, kCubeIndices));
  EXPECT_EQ(mesh->GetVertexCount(), 0);
  EXPECT_EQ(mesh->GetVertexCapacity(), 30);
  EXPECT_EQ(mesh->GetTriangleCount(), 0);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 60);

  state_.ResetState();
  ASSERT_TRUE(mesh->Set<Vector3>({kCubeVertices.data(), 3},
                                 {kCubeTriangles.data(), 3}));

  state_.index_buffer_config.fail_set = true;
  EXPECT_FALSE(mesh->Set<Vector3>(kCubeVertices, kCubeTriangles));
  EXPECT_FALSE(mesh->Set<Vector3>(kCubeVertices, kCubeIndices));
  EXPECT_EQ(mesh->GetVertexCount(), 0);
  EXPECT_EQ(mesh->GetVertexCapacity(), 30);
  EXPECT_EQ(mesh->GetTriangleCount(), 0);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 60);

  EXPECT_EQ(state_.invalid_call_count, 0);
  auto* test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  EXPECT_EQ(test_vertex_buffer->GetModifyCount(),
            4);  // 4 because failing the index buffer happens second.
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  auto* test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 2);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);
}

TEST_F(MeshTest, EditPreventsModification) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  auto mesh = render_system_->CreateMesh(
      vertex_type, DataVolatility::kStaticReadWrite, 30, 60);
  ASSERT_NE(mesh, nullptr);

  auto view = mesh->Edit();
  EXPECT_NE(view, nullptr);
  EXPECT_FALSE(mesh->Set<Vector3>(kCubeVertices, kCubeTriangles));
  EXPECT_FALSE(mesh->Set<Vector3>(kCubeVertices, kCubeIndices));
  EXPECT_EQ(mesh->GetVertexCount(), 0);
  EXPECT_EQ(mesh->GetVertexCapacity(), 30);
  EXPECT_EQ(mesh->GetTriangleCount(), 0);
  EXPECT_EQ(mesh->GetTriangleCapacity(), 60);
  view.reset();

  EXPECT_EQ(state_.invalid_call_count, 0);
  auto* test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  auto* test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);
}

TEST_F(MeshTest, CannotEditStaticWrite) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  auto mesh = render_system_->CreateMesh(vertex_type,
                                         DataVolatility::kStaticWrite, 30, 60);
  ASSERT_NE(mesh, nullptr);
  EXPECT_EQ(mesh->Edit(), nullptr);

  EXPECT_EQ(state_.invalid_call_count, 0);
  auto* test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  auto* test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);
}

TEST_F(MeshTest, FailEdit) {
  CreateSystem();
  auto material_type = CreateMaterialType({});
  EXPECT_NE(material_type, nullptr);
  const VertexType* vertex_type = material_type->GetVertexType();
  auto mesh = render_system_->CreateMesh(
      vertex_type, DataVolatility::kStaticReadWrite, 30, 60);
  ASSERT_NE(mesh, nullptr);

  state_.vertex_buffer_config.fail_edit_begin = true;
  EXPECT_EQ(mesh->Edit(), nullptr);

  state_.ResetState();
  state_.index_buffer_config.fail_edit_begin = true;
  EXPECT_EQ(mesh->Edit(), nullptr);

  state_.ResetState();
  auto view = mesh->Edit();
  EXPECT_NE(view, nullptr);
  EXPECT_EQ(mesh->Edit(), nullptr);
  view.reset();

  EXPECT_EQ(state_.invalid_call_count, 0);
  auto* test_vertex_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetVertexBuffer(GetAccessToken()));
  EXPECT_EQ(test_vertex_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_vertex_buffer->GetInvalidCallCount(), 0);
  auto* test_index_buffer =
      static_cast<TestRenderBuffer*>(mesh->GetIndexBuffer(GetAccessToken()));
  EXPECT_EQ(test_index_buffer->GetModifyCount(), 0);
  EXPECT_EQ(test_index_buffer->GetInvalidCallCount(), 0);
}

}  // namespace
}  // namespace gb
