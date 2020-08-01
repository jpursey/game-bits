// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mesh.h"
#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

class MeshViewTest : public RenderTest {
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
  static constexpr int kCubeVertexCount = 8;
  static constexpr int kCubeTriangleCount = 12;
  static constexpr int kCubeIndexCount = 36;
  static constexpr int kVertexCapacity = kCubeVertexCount + 4;
  static constexpr int kTriangleCapacity = kCubeTriangleCount + 4;

  void SetUp() override {
    CreateSystem();
    auto material = CreateMaterial({});
    EXPECT_NE(material, nullptr);
    mesh_ = render_system_->CreateMesh(&temp_resource_set_, material,
                                       DataVolatility::kPerFrame,
                                       kVertexCapacity, kTriangleCapacity);
    ASSERT_NE(mesh_, nullptr);
    ASSERT_TRUE(mesh_->Set<Vector3>(kCubeVertices, kCubeTriangles));
    test_vertex_buffer_ = static_cast<TestRenderBuffer*>(
        mesh_->GetVertexBuffer(GetAccessToken()));
    test_index_buffer_ =
        static_cast<TestRenderBuffer*>(mesh_->GetIndexBuffer(GetAccessToken()));
  }

  Mesh* mesh_ = nullptr;
  TestRenderBuffer* test_vertex_buffer_ = nullptr;
  TestRenderBuffer* test_index_buffer_ = nullptr;
};

TEST_F(MeshViewTest, Properties) {
  auto view = mesh_->Edit();
  ASSERT_NE(view, nullptr);

  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);
  EXPECT_FALSE(view->IsModified());

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_vertex_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer_->GetInvalidCallCount(), 0);
}

TEST_F(MeshViewTest, IndividualVertexAccess) {
  auto view = mesh_->Edit();
  ASSERT_NE(view, nullptr);

  for (int i = 0; i < kCubeVertexCount; ++i) {
    EXPECT_EQ(view->GetVertex<Vector3>(i), kCubeVertices[i])
        << "  Where i=" << i;
  }
  EXPECT_FALSE(view->IsModified());

  for (int i = 0; i < kCubeVertexCount; ++i) {
    view->ModifyVertex<Vector3>(kCubeVertexCount - i - 1) = kCubeVertices[i];
  }
  EXPECT_TRUE(view->IsModified());

  for (int i = 0; i < kCubeVertexCount; ++i) {
    EXPECT_EQ(view->GetVertex<Vector3>(i),
              kCubeVertices[kCubeVertexCount - i - 1])
        << "  Where i=" << i;
  }

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_vertex_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer_->GetInvalidCallCount(), 0);
}

TEST_F(MeshViewTest, SetVertices) {
  auto view = mesh_->Edit();
  ASSERT_NE(view, nullptr);

  view->AddVertices<Vector3>({kCubeVertices[0], kCubeVertices[1]});
  EXPECT_TRUE(view->IsModified());
  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount + 2);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);
  EXPECT_EQ(view->GetVertex<Vector3>(kCubeVertexCount), kCubeVertices[0]);
  EXPECT_EQ(view->GetVertex<Vector3>(kCubeVertexCount + 1), kCubeVertices[1]);

  view->SetVertices<Vector3>(
      kCubeVertexCount,
      {kCubeVertices[2], kCubeVertices[3], kCubeVertices[4], kCubeVertices[5]});
  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount + 4);
  EXPECT_EQ(view->GetVertex<Vector3>(kCubeVertexCount), kCubeVertices[2]);
  EXPECT_EQ(view->GetVertex<Vector3>(kCubeVertexCount + 1), kCubeVertices[3]);
  EXPECT_EQ(view->GetVertex<Vector3>(kCubeVertexCount + 2), kCubeVertices[4]);
  EXPECT_EQ(view->GetVertex<Vector3>(kCubeVertexCount + 3), kCubeVertices[5]);
  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount + 4);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);

  view->SetVertices<Vector3>(kCubeVertexCount - 2, kCubeVertices);
  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount + 4);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);
  for (int k = 0, i = kCubeVertexCount - 2; i < kVertexCapacity; ++i, ++k) {
    EXPECT_EQ(view->GetVertex<Vector3>(i), kCubeVertices[k])
        << "  Where i=" << i << ", k=" << k;
  }

  view->SetVertices<Vector3>(0, {kCubeVertices[6], kCubeVertices[7]});
  EXPECT_EQ(view->GetVertex<Vector3>(0), kCubeVertices[6]);
  EXPECT_EQ(view->GetVertex<Vector3>(1), kCubeVertices[7]);
  EXPECT_EQ(view->GetVertex<Vector3>(2), kCubeVertices[2]);
  EXPECT_EQ(view->GetVertex<Vector3>(3), kCubeVertices[3]);
  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount + 4);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);

  view->SetVertices<Vector3>(0, {});
  EXPECT_EQ(view->GetVertex<Vector3>(0), kCubeVertices[6]);
  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount + 4);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);

  view->SetVertices<Vector3>(view->GetVertexCapacity(), {});
  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount + 4);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_vertex_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer_->GetInvalidCallCount(), 0);
}

TEST_F(MeshViewTest, RemoveVertices) {
  auto view = mesh_->Edit();
  ASSERT_NE(view, nullptr);

  view->RemoveVertices(4, 2);
  EXPECT_TRUE(view->IsModified());
  EXPECT_EQ(view->GetVertexCount(), kCubeVertexCount - 2);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(view->GetVertex<Vector3>(i), kCubeVertices[i])
        << "  Where i=" << i;
  }
  for (int i = 4; i < kCubeVertexCount - 2; ++i) {
    EXPECT_EQ(view->GetVertex<Vector3>(i), kCubeVertices[i + 2])
        << "  Where i=" << i;
  }

  view->RemoveVertices(4, 100);
  EXPECT_EQ(view->GetVertexCount(), 4);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(view->GetVertex<Vector3>(i), kCubeVertices[i])
        << "  Where i=" << i;
  }

  view->RemoveVertices(2, 0);
  EXPECT_EQ(view->GetVertexCount(), 4);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(view->GetVertex<Vector3>(i), kCubeVertices[i])
        << "  Where i=" << i;
  }

  view->RemoveVertices(view->GetVertexCount(), 0);
  EXPECT_EQ(view->GetVertexCount(), 4);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);

  view->RemoveVertices(0, view->GetVertexCount());
  EXPECT_EQ(view->GetVertexCount(), 0);
  EXPECT_EQ(view->GetVertexCapacity(), kVertexCapacity);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_vertex_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer_->GetInvalidCallCount(), 0);
}

TEST_F(MeshViewTest, IndividualTriangleAccess) {
  auto view = mesh_->Edit();
  ASSERT_NE(view, nullptr);

  for (int i = 0; i < kCubeTriangleCount; ++i) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[i]) << "  Where i=" << i;
  }
  EXPECT_FALSE(view->IsModified());

  for (int i = 0; i < kCubeTriangleCount; ++i) {
    view->ModifyTriangle(kCubeTriangleCount - i - 1) = kCubeTriangles[i];
  }
  EXPECT_TRUE(view->IsModified());

  for (int i = 0; i < kCubeTriangleCount; ++i) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[kCubeTriangleCount - i - 1])
        << "  Where i=" << i;
  }

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_vertex_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer_->GetInvalidCallCount(), 0);
}

TEST_F(MeshViewTest, SetTriangles) {
  auto view = mesh_->Edit();
  ASSERT_NE(view, nullptr);

  view->AddTriangles({kCubeTriangles[0], kCubeTriangles[1]});
  EXPECT_TRUE(view->IsModified());
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 2);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount), kCubeTriangles[0]);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount + 1), kCubeTriangles[1]);

  view->SetTriangles(kCubeTriangleCount,
                     {kCubeTriangles[2], kCubeTriangles[3], kCubeTriangles[4],
                      kCubeTriangles[5]});
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount), kCubeTriangles[2]);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount + 1), kCubeTriangles[3]);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount + 2), kCubeTriangles[4]);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount + 3), kCubeTriangles[5]);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  view->SetTriangles(kCubeTriangleCount - 2, kCubeTriangles);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);
  for (int k = 0, i = kCubeTriangleCount - 2; i < kTriangleCapacity; ++i, ++k) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[k])
        << "  Where i=" << i << ", k=" << k;
  }

  view->SetTriangles(0, {kCubeTriangles[6], kCubeTriangles[7]});
  EXPECT_EQ(view->GetTriangle(0), kCubeTriangles[6]);
  EXPECT_EQ(view->GetTriangle(1), kCubeTriangles[7]);
  EXPECT_EQ(view->GetTriangle(2), kCubeTriangles[2]);
  EXPECT_EQ(view->GetTriangle(3), kCubeTriangles[3]);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  std::vector<Triangle> empty;
  view->SetTriangles(0, empty);
  EXPECT_EQ(view->GetTriangle(0), kCubeTriangles[6]);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  view->SetTriangles(view->GetTriangleCapacity(), empty);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_vertex_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer_->GetInvalidCallCount(), 0);
}

TEST_F(MeshViewTest, SetTrianglesWithIndices) {
  auto view = mesh_->Edit();
  ASSERT_NE(view, nullptr);

  view->AddTriangles({kCubeIndices[0], kCubeIndices[1], kCubeIndices[2],
                      kCubeIndices[3], kCubeIndices[4], kCubeIndices[5]});
  EXPECT_TRUE(view->IsModified());
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 2);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount), kCubeTriangles[0]);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount + 1), kCubeTriangles[1]);

  view->SetTriangles(
      kCubeTriangleCount,
      {kCubeIndices[6], kCubeIndices[7], kCubeIndices[8], kCubeIndices[9],
       kCubeIndices[10], kCubeIndices[11], kCubeIndices[12], kCubeIndices[13],
       kCubeIndices[14], kCubeIndices[15], kCubeIndices[16], kCubeIndices[17]});
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount), kCubeTriangles[2]);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount + 1), kCubeTriangles[3]);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount + 2), kCubeTriangles[4]);
  EXPECT_EQ(view->GetTriangle(kCubeTriangleCount + 3), kCubeTriangles[5]);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  view->SetTriangles(kCubeTriangleCount - 2, kCubeIndices);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);
  for (int k = 0, i = kCubeTriangleCount - 2; i < kTriangleCapacity; ++i, ++k) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[k])
        << "  Where i=" << i << ", k=" << k;
  }

  view->SetTriangles(0, {kCubeIndices[18], kCubeIndices[19], kCubeIndices[20],
                         kCubeIndices[21], kCubeIndices[22], kCubeIndices[23]});
  EXPECT_EQ(view->GetTriangle(0), kCubeTriangles[6]);
  EXPECT_EQ(view->GetTriangle(1), kCubeTriangles[7]);
  EXPECT_EQ(view->GetTriangle(2), kCubeTriangles[2]);
  EXPECT_EQ(view->GetTriangle(3), kCubeTriangles[3]);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  std::vector<uint16_t> empty;
  view->SetTriangles(0, empty);
  EXPECT_EQ(view->GetTriangle(0), kCubeTriangles[6]);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  view->SetTriangles(view->GetTriangleCapacity(), empty);
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount + 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_vertex_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer_->GetInvalidCallCount(), 0);
}

TEST_F(MeshViewTest, RemoveTriangles) {
  auto view = mesh_->Edit();
  ASSERT_NE(view, nullptr);

  view->RemoveTriangles(4, 2);
  EXPECT_TRUE(view->IsModified());
  EXPECT_EQ(view->GetTriangleCount(), kCubeTriangleCount - 2);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[i]) << "  Where i=" << i;
  }
  for (int i = 4; i < kCubeTriangleCount - 2; ++i) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[i + 2]) << "  Where i=" << i;
  }

  view->RemoveTriangles(4, 100);
  EXPECT_EQ(view->GetTriangleCount(), 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[i]) << "  Where i=" << i;
  }

  view->RemoveTriangles(2, 0);
  EXPECT_EQ(view->GetTriangleCount(), 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);
  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[i]) << "  Where i=" << i;
  }

  view->RemoveTriangles(view->GetTriangleCount(), 0);
  EXPECT_EQ(view->GetTriangleCount(), 4);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  view->RemoveTriangles(0, view->GetTriangleCount());
  EXPECT_EQ(view->GetTriangleCount(), 0);
  EXPECT_EQ(view->GetTriangleCapacity(), kTriangleCapacity);

  EXPECT_EQ(state_.invalid_call_count, 0);
  EXPECT_EQ(test_vertex_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_vertex_buffer_->GetInvalidCallCount(), 0);
  EXPECT_EQ(test_index_buffer_->GetModifyCount(), 1);
  EXPECT_EQ(test_index_buffer_->GetInvalidCallCount(), 0);
}

}  // namespace
}  // namespace gb
