// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_DRAW_LIST_H_
#define GB_RENDER_DRAW_LIST_H_

#include <cstdint>
#include <limits>
#include <vector>

#include "gb/render/mesh_pool_buffers.h"
#include "gb/render/render_types.h"

namespace gb {

// Internal draw command passed to the backend, in order to do drawing.
struct DrawCommand {
  enum class Type {
    kPipeline,      // Sets the render pipeline via "pipeline".
    kVertices,      // Sets the vertex data via "buffer".
    kIndices,       // Sets the index data via "buffer".
    kMeshPool,      // Sets the vertex and index data via "mesh_pool_buffers".
    kMaterialData,  // Sets the material data via "binding_data".
    kInstanceData,  // Sets the instance data via "binding_data".
    kScissor,       // Sets the scissor position via "rect".
    kDraw,          // Defines draw request via "draw".
    kReset,         // Reset all context
  };

  struct Rect {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
  };
  struct Draw {
    uint32_t index_offset;
    uint32_t index_count;
    int32_t vertex_offset;
    uint32_t first_instance;
  };

  DrawCommand(Type in_type) : type(in_type) {}
  DrawCommand(Type in_type, RenderBuffer* in_buffer)
      : type(in_type), buffer(in_buffer) {}
  DrawCommand(Type in_type, MeshPoolBuffers* in_mesh_pool_buffers)
      : type(in_type), mesh_pool_buffers(in_mesh_pool_buffers) {}
  DrawCommand(Type in_type, RenderPipeline* in_pipeline)
      : type(in_type), pipeline(in_pipeline) {}
  DrawCommand(Type in_type, BindingData* in_binding_data)
      : type(in_type), binding_data(in_binding_data) {}
  DrawCommand(Type in_type, Rect in_rect) : type(in_type), rect(in_rect) {}
  DrawCommand(Type in_type, Draw in_draw) : type(in_type), draw(in_draw) {}

  Type type;
  union {
    RenderBuffer* buffer;
    MeshPoolBuffers* mesh_pool_buffers;
    RenderPipeline* pipeline;
    BindingData* binding_data;
    Rect rect;
    Draw draw;
  };
};

// This class defines an ordered list of drawing commands which may be passed to
// the render system.
//
// A draw list only remains valid if the mesh or resources added to it remain
// loaded and unchanged. If a mesh is edited or binding data changed, the draw
// list may no longer be valid and using it is undefined behavior.
//
// This class is thread-compatible.
class DrawList final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  DrawList() = default;
  DrawList(const DrawList&) = delete;
  DrawList(DrawList&&) = default;
  DrawList& operator=(const DrawList&) = delete;
  DrawList& operator=(DrawList&&) = default;
  ~DrawList() = default;

  //----------------------------------------------------------------------------
  // State
  //
  // The following functions set state for subsequent Draw requests. The state
  // will persist until it is explicitly changed.
  //----------------------------------------------------------------------------

  // Clears material, mesh or mesh pool, and any instance data currently set in
  // the draw list.
  void ClearBindings();

  // Sets the material and material data for the next Draw command.
  //
  // If material data is not provided, the material's default material data will
  // be used.
  //
  // If a mesh or mesh pool is currently set, then this material must be
  // compatible with it (it must support its vertex type).
  //
  // This must be called before SetMaterialData or SetInstanceData are called.
  void SetMaterial(Material* material, BindingData* material_data = nullptr);

  // Overrides the material data for subsequent draw commands.
  //
  // This must be compatible with the currently set material.
  void SetMaterialData(BindingData* material_data);

  // Overrides the instance data for subsequent draw commands.
  //
  // This must be compatible with the currently set material.
  void SetInstanceData(BindingData* instance_data);

  // Sets the mesh (and optionally material and instance data) for the
  // next Draw command, replacing any mesh pool.
  //
  // If a material is not provided, then the material must already be set and be
  // compatible with this mesh's vertex type.
  //
  // If instance data is provided, it must be compatible with the material. If
  // it is not provided, then the existing instance data will be used.
  void SetMesh(Mesh* mesh, Material* material,
               BindingData* instance_data = nullptr);
  void SetMesh(Mesh* mesh, BindingData* instance_data) {
    SetMesh(mesh, nullptr, instance_data);
  }
  void SetMesh(Mesh* mesh) { SetMesh(mesh, nullptr, nullptr); }

  // Sets the mesh pool (and optionally material and instance data) for the
  // next Draw(const MeshDraw&) command, replacing any mesh.
  //
  // The material and instance data are as for SetMesh().
  void SetMeshPool(MeshPool* pool, Material* material,
                   BindingData* instance_data = nullptr);
  void SetMeshPool(MeshPool* pool, BindingData* instance_data) {
    SetMeshPool(pool, nullptr, instance_data);
  }
  void SetMeshPool(MeshPool* pool) { SetMeshPool(pool, nullptr, nullptr); }

  // Sets the scissor rectangle for subsequent draw commands.
  //
  // These are in pixels, and must be in the range [0, 65535].
  void SetScissor(int x, int y, int width, int height);

  //----------------------------------------------------------------------------
  // Commands
  //----------------------------------------------------------------------------

  // Draws the mesh. A mesh must be set.
  void Draw();

  // Draws a partial mesh. A mesh must be set.
  //
  // "first_vertex" is added to each index, and must be within the mesh's
  // vertex capacity.
  void DrawPartial(int first_triangle, int triangle_count,
                   int first_vertex = 0);

  // Draws a range of the mesh pool, such as one from PooledMesh::GetDraw(). A
  // mesh pool must be set, and the draw must have one instance.
  //
  // The draw is not checked against the pool: it must be from a mesh that is
  // still in it.
  void Draw(const MeshDraw& draw);

  // Resets the state back to the initial state.
  void Reset();

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  const std::vector<DrawCommand>& GetCommands(RenderInternal) const {
    return commands_;
  }

 private:
  // Returns true if the value fits in the 16 bit fields of DrawCommand.
  static bool FitsUint16(int value) {
    return value >= 0 && value <= std::numeric_limits<uint16_t>::max();
  }

  // Sets the material and instance data for SetMesh() or SetMeshPool(),
  // clearing the current mesh and mesh pool first, and asserts the material
  // draws `vertex_type`.
  void SetMeshBindings(const VertexType* vertex_type, Material* material,
                       BindingData* instance_data);

  // Returns the vertex type of the current mesh or mesh pool, or null if
  // neither is set.
  const VertexType* GetVertexType() const;

  MaterialType* current_material_type_ = nullptr;
  BindingData* current_instance_data_ = nullptr;
  Mesh* current_mesh_ = nullptr;
  MeshPool* current_mesh_pool_ = nullptr;
  std::vector<DrawCommand> commands_;
};

}  // namespace gb

#endif  // GB_RENDER_DRAW_LIST_H_
