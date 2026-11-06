// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/mesh_pool.h"

#include <cstdint>
#include <memory>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gb/render/render_test.h"
#include "gb/render/test_mesh_pool_buffers.h"
#include "gb/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {

inline void PrintTo(const MeshPoolStats::Space& value, std::ostream* out) {
  *out << "{capacity=" << value.capacity << ", used=" << value.used
       << ", waiting=" << value.waiting << ", free_ranges=" << value.free_ranges
       << ", largest_free_range=" << value.largest_free_range << "}";
}

namespace {

class MeshPoolTest : public RenderTest {
 protected:
  // A mesh, as created, and the data it was created from.
  struct LiveMesh {
    std::vector<Vector3> vertices;
    std::vector<uint16_t> indices;
    std::unique_ptr<PooledMesh> mesh;
  };

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
  const std::vector<uint32_t> kCubeIndices32{kCubeIndices.begin(),
                                             kCubeIndices.end()};

  void SetUp() override {
    CreateSystem();
    vertex_type_ = render_system_->RegisterVertexType<Vector3>(
        "vertex", {ShaderValue::kVec3});
    ASSERT_NE(vertex_type_, nullptr);
  }

  // Returns the cube's vertices moved by `x` along the x axis.
  std::vector<Vector3> GetCubeVertices(float x) const {
    std::vector<Vector3> vertices = kCubeVertices;
    for (Vector3& vertex : vertices) {
      vertex.x += x;
    }
    return vertices;
  }

  // Returns the vertices and indices of a fan of triangles, whose vertices
  // are unique to `thread` and `index`.
  static LiveMesh GetFan(int thread, int index) {
    LiveMesh fan;
    const int vertex_count = 3 + index % 6;
    for (int i = 0; i < vertex_count; ++i) {
      fan.vertices.push_back({static_cast<float>(thread),
                              static_cast<float>(index),
                              static_cast<float>(i)});
    }
    for (int i = 1; i + 1 < vertex_count; ++i) {
      fan.indices.push_back(0);
      fan.indices.push_back(static_cast<uint16_t>(i));
      fan.indices.push_back(static_cast<uint16_t>(i + 1));
    }
    return fan;
  }

  TestMeshPoolBuffers* GetBuffers(MeshPool* pool) {
    return static_cast<TestMeshPoolBuffers*>(
        pool->GetBuffers(GetAccessToken()));
  }

  int GetFrame() { return state_.backend->GetFrame(); }

  // Begins and ends a frame, returning false if it couldn't begin.
  bool EndFrame() {
    if (!render_system_->BeginFrame()) {
      return false;
    }
    render_system_->EndFrame();
    return true;
  }

  // Returns the vertices or indices the pool's buffers hold for a mesh.
  std::vector<Vector3> GetVertices(const PooledMesh& mesh) {
    return GetBuffers(mesh.GetPool())
        ->GetVertices<Vector3>(mesh.GetDraw().vertex_offset,
                               mesh.GetVertexCount());
  }
  template <typename Index>
  std::vector<Index> GetIndices(const PooledMesh& mesh) {
    const MeshDraw draw = mesh.GetDraw();
    return GetBuffers(mesh.GetPool())
        ->GetIndices<Index>(draw.first_index, draw.index_count);
  }

  // Creates `count` meshes in the pool, keeping the last `live_count` of them
  // in `meshes` and destroying the rest. Returns false if any fails.
  bool CreateAndDestroyMeshes(MeshPool* pool, int thread, int count,
                              int live_count, std::vector<LiveMesh>* meshes) {
    for (int i = 0; i < count; ++i) {
      LiveMesh live_mesh = GetFan(thread, i);
      live_mesh.mesh =
          pool->CreateMesh<Vector3>(live_mesh.vertices, live_mesh.indices);
      if (live_mesh.mesh == nullptr) {
        return false;
      }
      meshes->push_back(std::move(live_mesh));
      if (static_cast<int>(meshes->size()) > live_count) {
        meshes->erase(meshes->begin());
      }
    }
    return true;
  }

  const VertexType* vertex_type_ = nullptr;
};

TEST_F(MeshPoolTest, CreateAsResourcePtr) {
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 100, 50);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(pool->GetVertexType(), vertex_type_);
  EXPECT_EQ(pool->GetIndexType(), IndexType::kUint16);

  const MeshPoolStats stats = pool->GetStats();
  EXPECT_EQ(stats.mesh_count, 0);
  EXPECT_EQ(stats.vertices,
            (MeshPoolStats::Space{
                .capacity = 100, .free_ranges = 1, .largest_free_range = 100}));
  EXPECT_EQ(stats.triangles,
            (MeshPoolStats::Space{
                .capacity = 50, .free_ranges = 1, .largest_free_range = 50}));

  TestMeshPoolBuffers* buffers = GetBuffers(pool.Get());
  EXPECT_EQ(buffers->GetVertexSize(), static_cast<int>(sizeof(Vector3)));
  EXPECT_EQ(buffers->GetIndexType(), IndexType::kUint16);
  EXPECT_EQ(buffers->GetVertexCapacity(), 100);
  EXPECT_EQ(buffers->GetIndexCapacity(), 150);
}

TEST_F(MeshPoolTest, CreateInResourceSet) {
  ResourceSet resource_set;
  MeshPool* pool = render_system_->CreateMeshPool(&resource_set, vertex_type_,
                                                  IndexType::kUint32, 10, 20);
  ASSERT_NE(pool, nullptr);
  EXPECT_EQ(resource_set.Get<MeshPool>(pool->GetResourceId()), pool);
  EXPECT_EQ(pool->GetIndexType(), IndexType::kUint32);
  EXPECT_EQ(GetBuffers(pool)->GetIndexType(), IndexType::kUint32);
}

TEST_F(MeshPoolTest, FailCreate) {
  EXPECT_EQ(render_system_->CreateMeshPool(nullptr, IndexType::kUint16, 10, 10),
            nullptr);
  EXPECT_EQ(
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 0, 10),
      nullptr);
  EXPECT_EQ(
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 10, 0),
      nullptr);

  state_.fail_create_mesh_pool_buffers = true;
  EXPECT_EQ(
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 10, 10),
      nullptr);
}

TEST_F(MeshPoolTest, MeshReachesBuffersWhenFrameEnds) {
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool, nullptr);
  auto mesh = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh, nullptr);
  EXPECT_EQ(mesh->GetPool(), pool.Get());
  EXPECT_EQ(mesh->GetVertexCount(), 8);
  EXPECT_EQ(mesh->GetTriangleCount(), 12);
  EXPECT_NE(GetVertices(*mesh), kCubeVertices);
  EXPECT_EQ(GetIndices<uint16_t>(*mesh), std::vector<uint16_t>(36, 0xFFFF));

  ASSERT_TRUE(EndFrame());
  EXPECT_EQ(GetVertices(*mesh), kCubeVertices);
  EXPECT_EQ(GetIndices<uint16_t>(*mesh), kCubeIndices);
  EXPECT_EQ(GetBuffers(pool.Get())->GetInvalidCallCount(), 0);
}

TEST_F(MeshPoolTest, CreateMeshWithIndices) {
  ResourcePtr<MeshPool> pool16 =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool16, nullptr);
  ResourcePtr<MeshPool> pool32 =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint32, 8, 12);
  ASSERT_NE(pool32, nullptr);
  auto mesh16 = pool16->CreateMesh<Vector3>(kCubeVertices, kCubeIndices);
  ASSERT_NE(mesh16, nullptr);
  auto mesh32 = pool32->CreateMesh<Vector3>(kCubeVertices, kCubeIndices32);
  ASSERT_NE(mesh32, nullptr);
  EXPECT_EQ(mesh32->GetTriangleCount(), 12);

  ASSERT_TRUE(EndFrame());
  EXPECT_EQ(GetVertices(*mesh16), kCubeVertices);
  EXPECT_EQ(GetIndices<uint16_t>(*mesh16), kCubeIndices);
  EXPECT_EQ(GetVertices(*mesh32), kCubeVertices);
  EXPECT_EQ(GetIndices<uint32_t>(*mesh32), kCubeIndices32);
}

TEST_F(MeshPoolTest, WrongTypesFail) {
  ResourcePtr<MeshPool> pool16 =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool16, nullptr);
  ResourcePtr<MeshPool> pool32 =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint32, 8, 12);
  ASSERT_NE(pool32, nullptr);

  const std::vector<Vector2> vertices2 = {{0, 0}, {1, 0}, {1, 1}};
  EXPECT_EQ(pool16->CreateMesh<Vector2>(vertices2, kCubeTriangles), nullptr);
  EXPECT_EQ(pool16->CreateMesh<Vector3>(kCubeVertices, kCubeIndices32),
            nullptr);
  EXPECT_EQ(pool32->CreateMesh<Vector3>(kCubeVertices, kCubeIndices), nullptr);
  EXPECT_EQ(pool32->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles),
            nullptr);

  EXPECT_EQ(pool16->GetStats().vertices.used, 0);
  EXPECT_EQ(pool32->GetStats().vertices.used, 0);
}

TEST_F(MeshPoolTest, MeshWithoutTrianglesFails) {
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool, nullptr);
  const std::vector<uint16_t> no_indices;
  const std::vector<uint16_t> partial_triangle = {0, 1, 2, 3};
  EXPECT_EQ(pool->CreateMesh<Vector3>({}, kCubeIndices), nullptr);
  EXPECT_EQ(pool->CreateMesh<Vector3>(kCubeVertices, no_indices), nullptr);
  EXPECT_EQ(pool->CreateMesh<Vector3>(kCubeVertices, partial_triangle),
            nullptr);
  EXPECT_EQ(pool->GetStats().mesh_count, 0);
}

TEST_F(MeshPoolTest, VertexLimitIsOnlyFor16BitIndices) {
  ResourcePtr<MeshPool> pool16 =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool16, nullptr);
  ResourcePtr<MeshPool> pool32 =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint32, 8, 12);
  ASSERT_NE(pool32, nullptr);

  const std::vector<Vector3> max_vertices(kMaxMeshVertices, {0, 0, 0});
  const std::vector<Vector3> too_many_vertices(kMaxMeshVertices + 1, {0, 0, 0});
  const std::vector<uint16_t> indices16 = {0, 1, 2};
  const std::vector<uint32_t> indices32 = {0, 1, 2};
  auto mesh16 = pool16->CreateMesh<Vector3>(max_vertices, indices16);
  EXPECT_NE(mesh16, nullptr);
  EXPECT_EQ(pool16->CreateMesh<Vector3>(too_many_vertices, indices16), nullptr);
  auto mesh32 = pool32->CreateMesh<Vector3>(too_many_vertices, indices32);
  ASSERT_NE(mesh32, nullptr);
  EXPECT_EQ(mesh32->GetVertexCount(), kMaxMeshVertices + 1);
}

TEST_F(MeshPoolTest, GetDraw) {
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 16, 24);
  ASSERT_NE(pool, nullptr);
  auto mesh1 = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh1, nullptr);
  auto mesh2 = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh2, nullptr);

  EXPECT_EQ(mesh1->GetDraw(), (MeshDraw{.index_count = 36,
                                        .instance_count = 1,
                                        .first_index = 0,
                                        .vertex_offset = 0,
                                        .first_instance = 0}));
  EXPECT_EQ(mesh2->GetDraw(), (MeshDraw{.index_count = 36,
                                        .instance_count = 1,
                                        .first_index = 36,
                                        .vertex_offset = 8,
                                        .first_instance = 0}));
  EXPECT_EQ(mesh2->GetDraw(5).first_instance, 5);
  EXPECT_EQ(mesh2->GetDraw(2, 4, 7), (MeshDraw{.index_count = 12,
                                               .instance_count = 1,
                                               .first_index = 42,
                                               .vertex_offset = 8,
                                               .first_instance = 7}));
  EXPECT_EQ(mesh2->GetDraw(12, 0).index_count, 0);
}

TEST_F(MeshPoolTest, ReusesSpaceOnceBuffersCanReuseIt) {
  state_.mesh_pool_buffers_config.frames_before_reuse = 2;
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool, nullptr);

  // Destroyed in frame 0, so its space waits until frames 0, 1, and 2 have
  // ended.
  auto mesh = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh, nullptr);
  mesh.reset();
  EXPECT_EQ(pool->GetStats().vertices,
            (MeshPoolStats::Space{.capacity = 8, .used = 8, .waiting = 8}));
  EXPECT_EQ(pool->GetStats().triangles,
            (MeshPoolStats::Space{.capacity = 12, .used = 12, .waiting = 12}));

  ASSERT_TRUE(EndFrame());
  ASSERT_TRUE(EndFrame());
  ASSERT_EQ(GetFrame(), 2);
  auto mesh_in_frame_2 =
      pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh_in_frame_2, nullptr);
  EXPECT_EQ(mesh_in_frame_2->GetDraw().vertex_offset, 8);
  EXPECT_EQ(mesh_in_frame_2->GetDraw().first_index, 36);

  ASSERT_TRUE(EndFrame());
  auto mesh_in_frame_3 =
      pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh_in_frame_3, nullptr);
  EXPECT_EQ(mesh_in_frame_3->GetDraw().vertex_offset, 0);
  EXPECT_EQ(mesh_in_frame_3->GetDraw().first_index, 0);
  EXPECT_EQ(pool->GetStats().vertices.waiting, 0);
  EXPECT_EQ(pool->GetStats().triangles.waiting, 0);
}

TEST_F(MeshPoolTest, GrowsKeepingEveryMesh) {
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool, nullptr);

  // Half the meshes are created after the first half have been copied, and
  // each growth keeps what is already in the buffers.
  std::vector<std::unique_ptr<PooledMesh>> meshes;
  for (int i = 0; i < 10; ++i) {
    if (i == 5) {
      ASSERT_TRUE(EndFrame());
    }
    auto mesh = pool->CreateMesh<Vector3>(GetCubeVertices(i), kCubeTriangles);
    ASSERT_NE(mesh, nullptr) << "Mesh " << i;
    meshes.push_back(std::move(mesh));
  }
  ASSERT_TRUE(EndFrame());

  for (int i = 0; i < 10; ++i) {
    const PooledMesh& mesh = *meshes[i];
    EXPECT_EQ(mesh.GetDraw().vertex_offset, i * 8) << "Mesh " << i;
    EXPECT_EQ(mesh.GetDraw().first_index, i * 36) << "Mesh " << i;
    EXPECT_EQ(GetVertices(mesh), GetCubeVertices(i)) << "Mesh " << i;
    EXPECT_EQ(GetIndices<uint16_t>(mesh), kCubeIndices) << "Mesh " << i;
  }

  const MeshPoolStats stats = pool->GetStats();
  EXPECT_EQ(stats.mesh_count, 10);
  EXPECT_EQ(stats.vertices, (MeshPoolStats::Space{.capacity = 128,
                                                  .used = 80,
                                                  .free_ranges = 1,
                                                  .largest_free_range = 48}));
  EXPECT_EQ(stats.triangles, (MeshPoolStats::Space{.capacity = 192,
                                                   .used = 120,
                                                   .free_ranges = 1,
                                                   .largest_free_range = 72}));
  TestMeshPoolBuffers* buffers = GetBuffers(pool.Get());
  EXPECT_EQ(buffers->GetVertexCapacity(), 128);
  EXPECT_EQ(buffers->GetIndexCapacity(), 192 * 3);
  EXPECT_EQ(buffers->GetInvalidCallCount(), 0);
}

TEST_F(MeshPoolTest, GrowsToFitLargeMesh) {
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 1, 1);
  ASSERT_NE(pool, nullptr);
  auto mesh = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh, nullptr);
  EXPECT_EQ(pool->GetStats().vertices.capacity, 9);
  EXPECT_EQ(pool->GetStats().triangles.capacity, 13);
}

TEST_F(MeshPoolTest, StatsFollowEachChange) {
  state_.mesh_pool_buffers_config.frames_before_reuse = 0;
  ResourcePtr<MeshPool> pool = render_system_->CreateMeshPool(
      vertex_type_, IndexType::kUint16, 100, 100);
  ASSERT_NE(pool, nullptr);
  auto mesh1 = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  auto mesh2 = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  auto mesh3 = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_TRUE(mesh1 != nullptr && mesh2 != nullptr && mesh3 != nullptr);
  MeshPoolStats stats = pool->GetStats();
  EXPECT_EQ(stats.mesh_count, 3);
  EXPECT_EQ(stats.vertices, (MeshPoolStats::Space{.capacity = 100,
                                                  .used = 24,
                                                  .free_ranges = 1,
                                                  .largest_free_range = 76}));
  EXPECT_EQ(stats.triangles, (MeshPoolStats::Space{.capacity = 100,
                                                   .used = 36,
                                                   .free_ranges = 1,
                                                   .largest_free_range = 64}));

  mesh2.reset();
  stats = pool->GetStats();
  EXPECT_EQ(stats.mesh_count, 2);
  EXPECT_EQ(stats.vertices, (MeshPoolStats::Space{.capacity = 100,
                                                  .used = 24,
                                                  .waiting = 8,
                                                  .free_ranges = 1,
                                                  .largest_free_range = 76}));

  // Once mesh2's space is free, a mesh with one more vertex doesn't fit its
  // vertices, but its triangles fit best there.
  ASSERT_TRUE(EndFrame());
  std::vector<Vector3> vertices = kCubeVertices;
  vertices.push_back({2, 2, 2});
  auto mesh4 = pool->CreateMesh<Vector3>(vertices, kCubeTriangles);
  ASSERT_NE(mesh4, nullptr);
  EXPECT_EQ(mesh4->GetDraw().vertex_offset, 24);
  EXPECT_EQ(mesh4->GetDraw().first_index, 36);
  stats = pool->GetStats();
  EXPECT_EQ(stats.mesh_count, 3);
  EXPECT_EQ(stats.vertices, (MeshPoolStats::Space{.capacity = 100,
                                                  .used = 25,
                                                  .free_ranges = 2,
                                                  .largest_free_range = 67}));
  EXPECT_EQ(stats.triangles, (MeshPoolStats::Space{.capacity = 100,
                                                   .used = 36,
                                                   .free_ranges = 1,
                                                   .largest_free_range = 64}));
}

TEST_F(MeshPoolTest, FailedWriteReleasesSpace) {
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool, nullptr);
  state_.mesh_pool_buffers_config.fail_write = true;
  EXPECT_EQ(pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles), nullptr);
  const MeshPoolStats stats = pool->GetStats();
  EXPECT_EQ(stats.mesh_count, 0);
  EXPECT_EQ(stats.vertices.waiting, 8);
  EXPECT_EQ(stats.triangles.waiting, 12);
}

TEST_F(MeshPoolTest, FailedGrowFails) {
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool, nullptr);
  auto mesh = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh, nullptr);
  state_.mesh_pool_buffers_config.fail_grow = true;
  EXPECT_EQ(pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles), nullptr);
  const MeshPoolStats stats = pool->GetStats();
  EXPECT_EQ(stats.mesh_count, 1);
  EXPECT_EQ(stats.vertices.capacity, 8);
  EXPECT_EQ(stats.triangles.capacity, 12);
}

TEST_F(MeshPoolTest, DestroyPoolBeforeMeshFails) {
  EXPECT_DEATH(
      {
        ResourcePtr<MeshPool> pool = render_system_->CreateMeshPool(
            vertex_type_, IndexType::kUint16, 8, 12);
        auto mesh = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
        pool.Reset();
      },
      "pooled meshes remain");
}

TEST_F(MeshPoolTest, GetDrawOutOfRangeFails) {
  if (!kRenderAssertsCompiled) {
    GTEST_SKIP() << "Render asserts are only in debug builds";
  }
  SetRenderAssertEnabled(true);
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 8, 12);
  ASSERT_NE(pool, nullptr);
  auto mesh = pool->CreateMesh<Vector3>(kCubeVertices, kCubeTriangles);
  ASSERT_NE(mesh, nullptr);
  EXPECT_DEATH(mesh->GetDraw(-1, 1), "first_triangle >= 0");
  EXPECT_DEATH(mesh->GetDraw(10, 3), "triangle_count_");
  EXPECT_DEATH(mesh->GetDraw(-1), "draw_index >= 0");
}

TEST_F(MeshPoolTest, CreateAndDestroyFromManyThreads) {
  constexpr int kThreadCount = 4;
  constexpr int kMeshesPerThread = 500;
  constexpr int kLiveMeshesPerThread = 10;
  state_.mesh_pool_buffers_config.frames_before_reuse = 2;
  ResourcePtr<MeshPool> pool =
      render_system_->CreateMeshPool(vertex_type_, IndexType::kUint16, 16, 16);
  ASSERT_NE(pool, nullptr);

  std::vector<std::vector<LiveMesh>> live_meshes(kThreadCount);
  {
    ThreadTester tester;
    tester.RunLoop(0, "frames", [this] { return EndFrame(); });
    for (int thread = 0; thread < kThreadCount; ++thread) {
      tester.RunThenSignal(thread + 1, absl::StrCat("meshes", thread),
                           [this, &pool, &live_meshes, thread] {
                             return CreateAndDestroyMeshes(
                                 pool.Get(), thread, kMeshesPerThread,
                                 kLiveMeshesPerThread, &live_meshes[thread]);
                           });
    }
    for (int thread = 0; thread < kThreadCount; ++thread) {
      EXPECT_TRUE(tester.Wait(thread + 1));
    }
    EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  }

  // The last frame copies any writes the frames thread missed.
  ASSERT_TRUE(EndFrame());
  int vertex_count = 0;
  int triangle_count = 0;
  for (int thread = 0; thread < kThreadCount; ++thread) {
    ASSERT_EQ(static_cast<int>(live_meshes[thread].size()),
              kLiveMeshesPerThread);
    for (const LiveMesh& live_mesh : live_meshes[thread]) {
      EXPECT_EQ(GetVertices(*live_mesh.mesh), live_mesh.vertices);
      EXPECT_EQ(GetIndices<uint16_t>(*live_mesh.mesh), live_mesh.indices);
      vertex_count += live_mesh.mesh->GetVertexCount();
      triangle_count += live_mesh.mesh->GetTriangleCount();
    }
  }

  const MeshPoolStats stats = pool->GetStats();
  EXPECT_EQ(stats.mesh_count, kThreadCount * kLiveMeshesPerThread);
  EXPECT_EQ(stats.vertices.used - stats.vertices.waiting, vertex_count);
  EXPECT_EQ(stats.triangles.used - stats.triangles.waiting, triangle_count);
  EXPECT_EQ(GetBuffers(pool.Get())->GetInvalidCallCount(), 0);
}

}  // namespace
}  // namespace gb
