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

// Game representation for a chunk in the world.
//
// Chunks are made up of blocks
class Chunk final {
 public:
  //----------------------------------------------------------------------------
  // Constants and Types
  //----------------------------------------------------------------------------

  static inline constexpr glm::ivec3 kSize = {16, 16, 16};

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  Chunk(World* world, glm::ivec3 index) : world_(world), index_(index) {}
  Chunk(const Chunk&) = delete;
  Chunk(Chunk&&) = delete;
  Chunk& operator=(const Chunk&) = delete;
  Chunk& operator=(Chunk&&) = delete;
  ~Chunk() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  World* GetWorld() const { return world_; }
  const glm::ivec3& GetIndex() const { return index_; }

  //----------------------------------------------------------------------------
  // Block access
  //----------------------------------------------------------------------------

  // Returns if the chunk is entirely empty
  bool IsEmpty() { return empty_; }

  // After modifying block data, it will not be reflected properly until Update
  // is called.
  void Set(int x, int y, int z, BlockId new_block) {
    assert(x >= 0);
    assert(x < kSize.x);
    assert(y >= 0);
    assert(y < kSize.y);
    assert(z >= 0);
    assert(z < kSize.z);
    blocks_[x][y][z] = new_block;
    modified_ = true;
  }
  const BlockId& Get(int x, int y, int z) const { return blocks_[x][y][z]; }

  // Explicitly invalidate the chunk.
  //
  // This will force Update to rebuild everything if needed.
  void Invalidate() { modified_ = true; }

  // Updated the chunk based on the current block data.
  //
  // After this returns, any mesh will be updated and IsEmpty() will be correct.
  void Update();

  //----------------------------------------------------------------------------
  // Mesh management
  //----------------------------------------------------------------------------

  // Returns true if this may have mesh to render.
  bool HasMesh() { return has_mesh_; }

  // Returns the mesh to render.
  absl::Span<gb::Mesh* const> GetMesh() const { return mesh_; }

  // Returns the instance data to use with all mesh.
  gb::BindingData* GetInstanceData() const { return instance_data_.get(); }

  // Builds mesh for this chunk.
  bool BuildMesh();

  // Clears mesh for this chunk.
  void ClearMesh();

 private:
  struct MeshContext {
    Chunk* neighbor_chunks[6];
    std::vector<Vertex> vertices;
    std::vector<gb::Triangle> triangles;
  };

  void AddMesh(MeshContext* context, int x, int y, int z);

  void UpdateEmpty();
  bool UpdateMesh();

  World* const world_;
  const glm::ivec3 index_;
  bool modified_ = false;
  bool empty_ = true;
  bool has_mesh_ = false;
  BlockId blocks_[kSize.x][kSize.y][kSize.z] = {};
  gb::ResourceSet resources_;
  absl::InlinedVector<gb::Mesh*, 2> mesh_;
  std::unique_ptr<gb::BindingData> instance_data_;
};

#endif  // CHUNK_H_
