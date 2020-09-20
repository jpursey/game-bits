#include "chunk.h"

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

// Shadow colors based on the number of solid blocks sharing the vertex.
constexpr gb::Pixel kShadowColor[9] = {
    gb::Pixel(255, 255, 255), gb::Pixel(255, 255, 255),
    gb::Pixel(255, 255, 255), gb::Pixel(255, 255, 255),
    gb::Pixel(255, 255, 255), gb::Pixel(190, 190, 190),
    gb::Pixel(120, 120, 120), gb::Pixel(30, 30, 30),
    gb::Pixel(0, 0, 0),
};

}  // namespace

void Chunk::Update() {
  if (!modified_) {
    return;
  }
  if (has_mesh_) {
    UpdateMesh();
    ClearMesh();
    BuildMesh();
  }
  modified_ = false;
}

bool Chunk::BuildMesh() {
  if (!has_mesh_) {
    has_mesh_ = UpdateMesh();
  }
  return has_mesh_;
}

bool Chunk::UpdateMesh() {
  MeshContext context;
  context.chunk_pxpy = world_->GetChunk({index_.x + 1, index_.y + 1});
  context.chunk_py = world_->GetChunk({index_.x, index_.y + 1});
  context.chunk_nxpy = world_->GetChunk({index_.x - 1, index_.y + 1});
  context.chunk_px = world_->GetChunk({index_.x + 1, index_.y});
  context.chunk_nx = world_->GetChunk({index_.x - 1, index_.y});
  context.chunk_pxny = world_->GetChunk({index_.x + 1, index_.y - 1});
  context.chunk_ny = world_->GetChunk({index_.x, index_.y - 1});
  context.chunk_nxny = world_->GetChunk({index_.x - 1, index_.y - 1});

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
        glm::mat4(1.0f), glm::vec3(index_.x * kSize.x, index_.y * kSize.y, 0));
    instance_data_->SetConstants<InstanceData>(0, data);
  }

  return true;
}

BlockId Chunk::GetBlock(MeshContext* context, int x, int y, int z) {
  if (z < 0) {
    return kBlockRock2;
  }
  if (z >= kSize.z) {
    return kBlockAir;
  }
  if (x < 0) {
    if (y < 0) {
      return context->chunk_nxny->blocks_[kSize.x - 1][kSize.y - 1][z];
    }
    if (y >= kSize.y) {
      return context->chunk_nxpy->blocks_[kSize.x - 1][0][z];
    }
    return context->chunk_nx->blocks_[kSize.x - 1][y][z];
  }
  if (x >= kSize.x) {
    if (y < 0) {
      return context->chunk_pxny->blocks_[0][kSize.y - 1][z];
    }
    if (y >= kSize.y) {
      return context->chunk_pxpy->blocks_[0][0][z];
    }
    return context->chunk_px->blocks_[0][y][z];
  }
  if (y < 0) {
    return context->chunk_ny->blocks_[x][kSize.y - 1][z];
  }
  if (y >= kSize.y) {
    return context->chunk_py->blocks_[x][0][z];
  }
  return blocks_[x][y][z];
}

bool Chunk::GetSideBlocks(MeshContext* context, int x, int y, int z) {
  context->side_blocks[kCubeNx] = context->blocks[0][1][1] =
      GetBlock(context, x - 1, y, z);
  context->side_blocks[kCubePx] = context->blocks[2][1][1] =
      GetBlock(context, x + 1, y, z);
  context->side_blocks[kCubeNy] = context->blocks[1][0][1] =
      GetBlock(context, x, y - 1, z);
  context->side_blocks[kCubePy] = context->blocks[1][2][1] =
      GetBlock(context, x, y + 1, z);
  context->side_blocks[kCubeNz] = context->blocks[1][1][0] =
      GetBlock(context, x, y, z - 1);
  context->side_blocks[kCubePz] = context->blocks[1][1][2] =
      GetBlock(context, x, y, z + 1);
  return context->side_blocks[kCubeNx] != kBlockAir ||
         context->side_blocks[kCubePx] != kBlockAir ||
         context->side_blocks[kCubeNy] != kBlockAir ||
         context->side_blocks[kCubePy] != kBlockAir ||
         context->side_blocks[kCubeNz] != kBlockAir ||
         context->side_blocks[kCubePz] != kBlockAir;
}

void Chunk::GetEdgeAndCornerBlocks(MeshContext* context, int x, int y, int z) {
  context->blocks[0][0][0] = GetBlock(context, x - 1, y - 1, z - 1);
  context->blocks[0][0][1] = GetBlock(context, x - 1, y - 1, z);
  context->blocks[0][0][2] = GetBlock(context, x - 1, y - 1, z + 1);
  context->blocks[0][1][0] = GetBlock(context, x - 1, y, z - 1);
  context->blocks[0][1][2] = GetBlock(context, x - 1, y, z + 1);
  context->blocks[0][2][0] = GetBlock(context, x - 1, y + 1, z - 1);
  context->blocks[0][2][1] = GetBlock(context, x - 1, y + 1, z);
  context->blocks[0][2][2] = GetBlock(context, x - 1, y + 1, z + 1);
  context->blocks[1][0][0] = GetBlock(context, x, y - 1, z - 1);
  context->blocks[1][0][2] = GetBlock(context, x, y - 1, z + 1);
  context->blocks[1][2][0] = GetBlock(context, x, y + 1, z - 1);
  context->blocks[1][2][2] = GetBlock(context, x, y + 1, z + 1);
  context->blocks[2][0][0] = GetBlock(context, x + 1, y - 1, z - 1);
  context->blocks[2][0][1] = GetBlock(context, x + 1, y - 1, z);
  context->blocks[2][0][2] = GetBlock(context, x + 1, y - 1, z + 1);
  context->blocks[2][1][0] = GetBlock(context, x + 1, y, z - 1);
  context->blocks[2][1][2] = GetBlock(context, x + 1, y, z + 1);
  context->blocks[2][2][0] = GetBlock(context, x + 1, y + 1, z - 1);
  context->blocks[2][2][1] = GetBlock(context, x + 1, y + 1, z);
  context->blocks[2][2][2] = GetBlock(context, x + 1, y + 1, z + 1);
}

void Chunk::GetVertexBlockCount(MeshContext* context, int x, int y, int z) {
  std::memset(context->vertex_block_count, 0,
              sizeof(context->vertex_block_count));
  if (context->blocks[0][0][0] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyNz] += 1;
  }
  if (context->blocks[1][0][0] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyNz] += 1;
    context->vertex_block_count[kCubePxNyNz] += 1;
  }
  if (context->blocks[2][0][0] != kBlockAir) {
    context->vertex_block_count[kCubePxNyNz] += 1;
  }

  if (context->blocks[0][1][0] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyNz] += 1;
    context->vertex_block_count[kCubeNxPyNz] += 1;
  }
  if (context->blocks[1][1][0] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyNz] += 1;
    context->vertex_block_count[kCubePxNyNz] += 1;
    context->vertex_block_count[kCubeNxPyNz] += 1;
    context->vertex_block_count[kCubePxPyNz] += 1;
  }
  if (context->blocks[2][1][0] != kBlockAir) {
    context->vertex_block_count[kCubePxNyNz] += 1;
    context->vertex_block_count[kCubePxPyNz] += 1;
  }

  if (context->blocks[0][2][0] != kBlockAir) {
    context->vertex_block_count[kCubeNxPyNz] += 1;
  }
  if (context->blocks[1][2][0] != kBlockAir) {
    context->vertex_block_count[kCubeNxPyNz] += 1;
    context->vertex_block_count[kCubePxPyNz] += 1;
  }
  if (context->blocks[2][2][0] != kBlockAir) {
    context->vertex_block_count[kCubePxPyNz] += 1;
  }

  if (context->blocks[0][0][1] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyNz] += 1;
    context->vertex_block_count[kCubeNxNyPz] += 1;
  }
  if (context->blocks[1][0][1] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyNz] += 1;
    context->vertex_block_count[kCubePxNyNz] += 1;
    context->vertex_block_count[kCubeNxNyPz] += 1;
    context->vertex_block_count[kCubePxNyPz] += 1;
  }
  if (context->blocks[2][0][1] != kBlockAir) {
    context->vertex_block_count[kCubePxNyNz] += 1;
    context->vertex_block_count[kCubePxNyPz] += 1;
  }

  if (context->blocks[0][1][1] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyNz] += 1;
    context->vertex_block_count[kCubeNxPyNz] += 1;
    context->vertex_block_count[kCubeNxNyPz] += 1;
    context->vertex_block_count[kCubeNxPyPz] += 1;
  }
  if (context->blocks[2][1][1] != kBlockAir) {
    context->vertex_block_count[kCubePxNyNz] += 1;
    context->vertex_block_count[kCubePxPyNz] += 1;
    context->vertex_block_count[kCubePxNyPz] += 1;
    context->vertex_block_count[kCubePxPyPz] += 1;
  }

  if (context->blocks[0][2][1] != kBlockAir) {
    context->vertex_block_count[kCubeNxPyNz] += 1;
    context->vertex_block_count[kCubeNxPyPz] += 1;
  }
  if (context->blocks[1][2][1] != kBlockAir) {
    context->vertex_block_count[kCubeNxPyNz] += 1;
    context->vertex_block_count[kCubePxPyNz] += 1;
    context->vertex_block_count[kCubeNxPyPz] += 1;
    context->vertex_block_count[kCubePxPyPz] += 1;
  }
  if (context->blocks[2][2][1] != kBlockAir) {
    context->vertex_block_count[kCubePxPyNz] += 1;
    context->vertex_block_count[kCubePxPyPz] += 1;
  }

  if (context->blocks[0][0][2] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyPz] += 1;
  }
  if (context->blocks[1][0][2] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyPz] += 1;
    context->vertex_block_count[kCubePxNyPz] += 1;
  }
  if (context->blocks[2][0][2] != kBlockAir) {
    context->vertex_block_count[kCubePxNyPz] += 1;
  }

  if (context->blocks[0][1][2] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyPz] += 1;
    context->vertex_block_count[kCubeNxPyPz] += 1;
  }
  if (context->blocks[1][1][2] != kBlockAir) {
    context->vertex_block_count[kCubeNxNyPz] += 1;
    context->vertex_block_count[kCubePxNyPz] += 1;
    context->vertex_block_count[kCubeNxPyPz] += 1;
    context->vertex_block_count[kCubePxPyPz] += 1;
  }
  if (context->blocks[2][1][2] != kBlockAir) {
    context->vertex_block_count[kCubePxNyPz] += 1;
    context->vertex_block_count[kCubePxPyPz] += 1;
  }

  if (context->blocks[0][2][2] != kBlockAir) {
    context->vertex_block_count[kCubeNxPyPz] += 1;
  }
  if (context->blocks[1][2][2] != kBlockAir) {
    context->vertex_block_count[kCubeNxPyPz] += 1;
    context->vertex_block_count[kCubePxPyPz] += 1;
  }
  if (context->blocks[2][2][2] != kBlockAir) {
    context->vertex_block_count[kCubePxPyPz] += 1;
  }
}

void Chunk::AddMesh(MeshContext* context, int x, int y, int z) {
  BlockId block_id = context->blocks[1][1][1] = blocks_[x][y][z];
  if (block_id != kBlockAir) {
    return;
  }

  if (!GetSideBlocks(context, x, y, z)) {
    return;
  }
  GetEdgeAndCornerBlocks(context, x, y, z);
  GetVertexBlockCount(context, x, y, z);

  // If we have more than 2^16 indexes, then this will start wrapping around --
  // which is exactly what we want. We break the mesh into multiple meshes.
  uint16_t index = static_cast<uint16_t>(context->vertices.size());
  glm::vec3 position = {x, y, z};

  Vertex vertex;
  for (int side = 0; side < 6; ++side) {
    block_id = context->side_blocks[side];
    if (block_id == kBlockAir) {
      continue;
    }
    vertex.normal = kCubeSideNormal[side] * -1.0f;
    for (int i = 0; i < 4; ++i) {
      const int vertex_index = kCubeSideVertex[side][i];
      vertex.color = kShadowColor[context->vertex_block_count[vertex_index]];
      vertex.pos = position + kCubePosition[vertex_index];
      vertex.uv = kBlockUvOffset[block_id] + kCubeSideUv[i] * kBlockUvEndScale;
      context->vertices.push_back(vertex);
    }
    for (int face = 0; face < 2; ++face) {
      gb::Triangle triangle = kCubeSideTriangle[face];
      triangle.a += index;
      triangle.b += index;
      triangle.c += index;
      std::swap(triangle.a, triangle.b);  // Fix winding order.
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
