#ifndef CHUNK_H_
#define CHUNK_H_

#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/types/span.h"
#include "gb/render/binding_data.h"
#include "gb/render/mesh.h"
#include "gb/resource/resource_set.h"
#include "glm/glm.hpp"

// Game includes
#include "block.h"
#include "game_types.h"
#include "scene_types.h"

// All chunks are logically zero for Y.
struct ChunkIndex {
  ChunkIndex() = default;
  ChunkIndex(int in_x, int in_y) : x(in_x), y(in_y) {}
  int x = 0;
  int y = 0;
};

// Game representation for a chunk in the world.
//
// Chunks are made up of blocks
class Chunk final {
 public:
  //----------------------------------------------------------------------------
  // Constants and Types
  //----------------------------------------------------------------------------

  static inline constexpr glm::ivec3 kSize = {16, 16, 256};

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  Chunk(World* world, const ChunkIndex& index) : world_(world), index_(index) {}
  Chunk(const Chunk&) = delete;
  Chunk(Chunk&&) = delete;
  Chunk& operator=(const Chunk&) = delete;
  Chunk& operator=(Chunk&&) = delete;
  ~Chunk() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  World* GetWorld() const { return world_; }
  const ChunkIndex& GetIndex() const { return index_; }

  //----------------------------------------------------------------------------
  // Block access
  //----------------------------------------------------------------------------

  // After modifying block data, it will not be reflected properly until Update
  // is called.
  void Set(int x, int y, int z, BlockId new_block) {
    blocks_[x][y][z] = new_block;
    modified_ = true;
  }
  void Set(const glm::ivec3& index, BlockId new_block) {
    Set(index.x, index.y, index.z, new_block);
  }
  const BlockId& Get(int x, int y, int z) const { return blocks_[x][y][z]; }
  const BlockId& Get(const glm::ivec3& index) const {
    return Get(index.x, index.y, index.z);
  }

  // Explicitly invalidate the chunk.
  //
  // This will force Update to rebuild everything if needed.
  void Invalidate() { modified_ = true; }

  // Updated the chunk based on the current block data.
  void Update();

  //----------------------------------------------------------------------------
  // Mesh management
  //----------------------------------------------------------------------------

  // Returns true if this may have mesh to render.
  bool HasMesh() { return has_mesh_; }

  // Returns the mesh to render.
  absl::Span<gb::Mesh* const> GetMesh() {
    if (modified_) {
      Update();
    }
    return mesh_;
  }

  // Returns the instance data to use with all mesh.
  gb::BindingData* GetInstanceData() const { return instance_data_.get(); }

  // Builds mesh for this chunk.
  bool BuildMesh();

  // Clears mesh for this chunk.
  void ClearMesh();

 private:
  struct MeshContext {
    Chunk* chunk_pxpy;
    Chunk* chunk_py;
    Chunk* chunk_nxpy;
    Chunk* chunk_px;
    Chunk* chunk_nx;
    Chunk* chunk_pxny;
    Chunk* chunk_ny;
    Chunk* chunk_nxny;
    BlockId blocks[3][3][3];
    BlockId side_blocks[6];
    int vertex_block_count[8];
    std::vector<Vertex> vertices;
    std::vector<gb::Triangle> triangles;
  };

  BlockId GetBlock(MeshContext* context, int x, int y, int z);
  bool GetSideBlocks(MeshContext* context, int x, int y, int z);
  void GetEdgeAndCornerBlocks(MeshContext* context, int x, int y, int z);
  void GetVertexBlockCount(MeshContext* context, int x, int y, int z);
  void AddMesh(MeshContext* context, int x, int y, int z);

  bool UpdateMesh();

  World* const world_;
  const ChunkIndex index_;
  bool modified_ = false;
  bool has_mesh_ = false;
  BlockId blocks_[kSize.x][kSize.y][kSize.z] = {};
  gb::ResourceSet resources_;
  absl::InlinedVector<gb::Mesh*, 2> mesh_;
  std::unique_ptr<gb::BindingData> instance_data_;
};

#endif  // CHUNK_H_
