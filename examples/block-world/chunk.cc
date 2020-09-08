#include "chunk.h"

#include "gb/render/pixel_colors.h"
#include "gb/render/render_system.h"
#include "glm/ext/matrix_transform.hpp"

// Game includes
#include "block.h"
#include "cube.h"
#include "world.h"
#include "world_resources.h"

namespace {

constexpr int kMaxVerticesPerMesh = 65536;
constexpr int kMaxTrianglesPerMesh = 32768;

}  // namespace

void Chunk::Update() {
  if (!modified_) {
    return;
  }
  UpdateEmpty();
  if (has_mesh_) {
    UpdateMesh();
    ClearMesh();
    BuildMesh();
  }
}

void Chunk::UpdateEmpty() {
  empty_ = true;
  for (int x = 0; x < kSize.x; ++x) {
    for (int y = 0; y < kSize.y; ++y) {
      for (int z = 0; z < kSize.z; ++z) {
        if (blocks_[x][y][z] != kBlockAir) {
          empty_ = false;
          return;
        }
      }
    }
  }
}

bool Chunk::BuildMesh() {
  if (!has_mesh_) {
    has_mesh_ = UpdateMesh();
  }
  return has_mesh_;
}

bool Chunk::UpdateMesh() {
  MeshContext context;
  context.neighbor_chunks[kCubePx] =
      world_->GetChunk({index_.x + 1, index_.y, index_.z});
  context.neighbor_chunks[kCubeNx] =
      world_->GetChunk({index_.x - 1, index_.y, index_.z});
  context.neighbor_chunks[kCubePy] =
      world_->GetChunk({index_.x, index_.y + 1, index_.z});
  context.neighbor_chunks[kCubeNy] =
      world_->GetChunk({index_.x, index_.y - 1, index_.z});
  context.neighbor_chunks[kCubePz] =
      world_->GetChunk({index_.x, index_.y, index_.z + 1});
  context.neighbor_chunks[kCubeNz] =
      world_->GetChunk({index_.x, index_.y, index_.z - 1});

  for (int x = 0; x < kSize.x; ++x) {
    for (int y = 0; y < kSize.y; ++y) {
      for (int z = 0; z < kSize.z; ++z) {
        AddMesh(&context, x, y, z);
      }
    }
  }

  auto* render_system = world_->GetContext().GetPtr<gb::RenderSystem>();
  auto* world_resources = world_->GetContext().GetPtr<WorldResources>();

  const int num_vertices = static_cast<int>(context.vertices.size());
  const int num_triangles = static_cast<int>(context.triangles.size());
  const int num_meshes =
      (static_cast<int>(context.vertices.size()) + kMaxVerticesPerMesh - 1) /
      kMaxVerticesPerMesh;
  for (int i = num_meshes; i < static_cast<int>(mesh_.size()); ++i) {
    resources_.Remove(mesh_[i], false);
  }
  mesh_.resize(num_meshes);
  if (num_meshes == 0) {
    return true;
  }

  int vertex_index = 0;
  int triangle_index = 0;
  for (int i = 0; i < num_meshes; ++i) {
    const int vertex_count = std::min(num_vertices, kMaxVerticesPerMesh);
    const int triangle_count = std::min(num_triangles, kMaxTrianglesPerMesh);
    if (mesh_[i] == nullptr) {
      mesh_[i] = render_system->CreateMesh(
          &resources_, world_resources->GetChunkMaterial(),
          gb::DataVolatility::kStaticWrite, vertex_count, triangle_count);
      if (mesh_[i] == nullptr) {
        LOG(ERROR) << "Failed to create mesh for chunk";
        ClearMesh();
        return false;
      }
    }
    if (!mesh_[i]->Set(
            absl::MakeConstSpan(context.vertices.data() + vertex_index,
                                vertex_count),
            absl::MakeConstSpan(context.triangles.data() + triangle_index,
                                triangle_count))) {
      LOG(ERROR) << "Failed to initialize mesh for chunk";
      ClearMesh();
      return false;
    }
    vertex_index += vertex_count;
    triangle_index += triangle_count;
  }

  if (instance_data_ == nullptr) {
    instance_data_ =
        world_resources->GetChunkMaterial()->CreateInstanceBindingData();
    if (instance_data_ == nullptr) {
      LOG(ERROR) << "Failed to create instance data for chunk";
      ClearMesh();
      return false;
    }
    InstanceData data;
    data.model = glm::translate(
        glm::mat4(1.0f),
        glm::vec3(index_.x * kSize.x, index_.y * kSize.y, index_.z * kSize.z));
    instance_data_->SetConstants<InstanceData>(0, data);
  }

  return true;
}

void Chunk::AddMesh(MeshContext* context, int x, int y, int z) {
  BlockId block_id = blocks_[x][y][z];
  if (block_id == kBlockAir) {
    return;
  }

  BlockId neighbors[6];
  neighbors[kCubePx] =
      (x < kSize.x - 1 ? blocks_[x + 1][y][z]
                       : context->neighbor_chunks[kCubePx]->blocks_[0][y][z]);
  neighbors[kCubeNx] =
      (x > 0 ? blocks_[x - 1][y][z]
             : context->neighbor_chunks[kCubeNx]->blocks_[kSize.x - 1][y][z]);
  neighbors[kCubePy] =
      (y < kSize.y - 1 ? blocks_[x][y + 1][z]
                       : context->neighbor_chunks[kCubePy]->blocks_[x][0][z]);
  neighbors[kCubeNy] =
      (y > 0 ? blocks_[x][y - 1][z]
             : context->neighbor_chunks[kCubeNy]->blocks_[x][kSize.y - 1][z]);
  neighbors[kCubePz] =
      (z < kSize.z - 1 ? blocks_[x][y][z + 1]
                       : context->neighbor_chunks[kCubePz]->blocks_[x][y][0]);
  neighbors[kCubeNz] =
      (z > 0 ? blocks_[x][y][z - 1]
             : context->neighbor_chunks[kCubeNz]->blocks_[x][y][kSize.z - 1]);

  Vertex vertex;
  vertex.color = gb::Colors::kWhite;

  // If we have more than 2^16 indexes, then this will start wrapping around --
  // which is exactly what we want. We break the mesh into multiple meshes.
  uint16_t index = static_cast<uint16_t>(context->vertices.size());
  glm::vec3 position = {x, y, z};

  for (int side = 0; side < 6; ++side) {
    if (neighbors[side] != kBlockAir) {
      continue;
    }
    vertex.normal = kCubeSideNormal[side];
    for (int i = 0; i < 4; ++i) {
      vertex.pos = position + kCubePosition[kCubeSideVertex[side][i]];
      vertex.uv = kBlockUvOffset[block_id] + kCubeSideUv[i] * kBlockUvEndScale;
      context->vertices.push_back(vertex);
    }
    for (int face = 0; face < 2; ++face) {
      gb::Triangle triangle = kCubeSideTriangle[face];
      triangle.a += index;
      triangle.b += index;
      triangle.c += index;
      context->triangles.push_back(triangle);
    }
    index += 4;
  }
}

void Chunk::ClearMesh() {
  mesh_.clear();
  resources_.RemoveAll();
  has_mesh_ = false;
}
