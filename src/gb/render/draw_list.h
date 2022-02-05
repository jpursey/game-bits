// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_DRAW_LIST_H_
#define GB_RENDER_DRAW_LIST_H_

#include <vector>

#include "gb/render/render_types.h"

namespace gb {

// Internal draw command passed to the backend, in order to do drawing.
struct DrawCommand {
  enum class Type {
    kPipeline,      // Sets the render pipeline via "pipeline".
    kVertices,      // Sets the vertex data via "buffer".
    kIndices,       // Sets the index data via "buffer".
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
    uint16_t vertex_offset;
  };

  DrawCommand(Type in_type) : type(in_type) {}
  DrawCommand(Type in_type, RenderBuffer* in_buffer)
      : type(in_type), buffer(in_buffer) {}
  DrawCommand(Type in_type, RenderPipeline* in_pipeline)
      : type(in_type), pipeline(in_pipeline) {}
  DrawCommand(Type in_type, BindingData* in_binding_data)
      : type(in_type), binding_data(in_binding_data) {}
  DrawCommand(Type in_type, Rect in_rect) : type(in_type), rect(in_rect) {}
  DrawCommand(Type in_type, Draw in_draw) : type(in_type), draw(in_draw) {}

  Type type;
  union {
    RenderBuffer* buffer;
    RenderPipeline* pipeline;
    BindingData* binding_data;
    Rect rect;
    Draw draw;
  };
};

// This class defines an ordered list of drawing commands wich may be passed to
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

  // Clears material, mesh, and any instance data currently set in the draw
  // list.
  void ClearBindings();

  // Sets the material and material data for the next Draw command.
  //
  // If material data is not provided, the material's default material data will
  // be used.
  //
  // If mesh is currently set, then this material must be compatible with the
  // current mesh (it must support the mesh's vertex type).
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
  // next Draw command.
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

  // Sets the scissor rectangle for subsequent draw commands.
  //
  // These are in pixels and are clipped to the current frame dimensions.
  void SetScissor(int x, int y, int width, int height);

  //----------------------------------------------------------------------------
  // Commands
  //----------------------------------------------------------------------------

  // Draws the mesh.
  void Draw();

  // Draws a partial mesh.
  void DrawPartial(int first_triangle, int triangle_count,
                   int first_vertex = 0);

  // Resets the state back to the initial state.
  void Reset();

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  const std::vector<DrawCommand>& GetCommands(RenderInternal) const {
    return commands_;
  }

 private:
  MaterialType* current_material_type_ = nullptr;
  BindingData* current_instance_data_ = nullptr;
  Mesh* current_mesh_ = nullptr;
  std::vector<DrawCommand> commands_;
};

}  // namespace gb

#endif  // GB_RENDER_DRAW_LIST_H_
