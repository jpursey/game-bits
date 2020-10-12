// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_GB_RENDER_BACKEND_H_
#define GB_RENDER_GB_RENDER_BACKEND_H_

#include <memory>

#include "gb/file/file.h"
#include "gb/render/binding.h"
#include "gb/render/binding_data.h"
#include "gb/render/render_buffer.h"
#include "gb/render/render_pipeline.h"
#include "gb/render/render_types.h"
#include "gb/render/shader_code.h"
#include "gb/render/texture.h"
#include "gb/resource/resource_entry.h"

namespace gb {

// This class defines a render backend for a specific graphics API.
//
// This is an internal class called by other render classes to implement all
// interaction with the underlying graphics API and GPU. Application code must
// create a derived class of RenderBackend when creating the RenderSystem, but
// otherwise should consider it an opaque class.
//
// Derived classes should assume that all method arguments are already valid. No
// additional checking is required, outside of limits that are specific to the
// derived class implementation or underlying graphics API or GPU.
//
// This class and all derived classes must be thread-compatible.
class RenderBackend {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  RenderBackend(const RenderBackend&) = delete;
  RenderBackend& operator=(const RenderBackend&) = delete;
  virtual ~RenderBackend() = default;

  //----------------------------------------------------------------------------
  // Derived class interface
  //----------------------------------------------------------------------------

  // Sets the clear color for the background before rendering takes place.
  virtual void SetClearColor(RenderInternal, Pixel color) = 0;

  // Returns the current dimensions of the render frame.
  //
  // This may change from frame to frame if the render target changes size
  // (for instance, a window resize or resolution change).
  virtual FrameDimensions GetFrameDimensions(RenderInternal) const = 0;

  // Creates a new 2D RGBA texture of the specified width and height.
  //
  // If the texture could not be created, this returns null. On success, the
  // resulting texture is considered uninitialized (all pixels are of unknown
  // value).
  virtual Texture* CreateTexture(RenderInternal, ResourceEntry entry,
                                 DataVolatility volatility, int width,
                                 int height, const SamplerOptions& options) = 0;

  // Creates the shader code compatible with this backend from the raw shader
  // code data.
  //
  // The code buffer contains a pointer to code_size bytes of platform-specific
  // shader data.
  //
  // If the shader code could not be created, this returns null.
  virtual std::unique_ptr<ShaderCode> CreateShaderCode(RenderInternal,
                                                       const void* code,
                                                       int64_t code_size) = 0;

  // Creates a new RenderSceneType compatible with this backend.
  //
  // Bindings contain common bindings for all binding sets that must be included
  // in the binding data generated for all scenes and material types created
  // with this scene type. This may be empty.
  //
  // If the RenderSceneType could not be created, this returns null.
  virtual std::unique_ptr<RenderSceneType> CreateSceneType(
      RenderInternal, absl::Span<const Binding> bindings) = 0;

  // Creates a new RenderScene for the specified scene type which is compatible
  // with this backend.
  //
  // The scene type is always an instance previously created via
  // CreateSceneType.
  //
  // Note: Scene default binding data is automatically copied into the created
  // RenderScene after this returns, so derived classes do not (and should not)
  // do so.
  //
  // If the RenderSceneType could not be created, this returns null.
  virtual std::unique_ptr<RenderScene> CreateScene(RenderInternal,
                                                   RenderSceneType* scene_type,
                                                   int scene_order) = 0;

  // Creates a new RenderPipeline compatible with this backend.
  //
  // The scene type and shader code are all instances previously create via
  // CreateSceneType and CreateShaderCode respectively. Bindings contain all
  // bindings the pipeline should support for all binding sets.
  //
  // If the RenderPipeline could not be created, this returns null.
  virtual std::unique_ptr<RenderPipeline> CreatePipeline(
      RenderInternal, RenderSceneType* scene_type,
      const VertexType* vertex_type, absl::Span<const Binding> bindings,
      ShaderCode* vertex_shader, ShaderCode* fragment_shader,
      const MaterialConfig& config) = 0;

  // Creates a new vertex buffer compatible with this backend.
  //
  // vertex_size is the size in bytes of a single vertex, and vertex_capacity is
  // the minimum number of vertices the buffer must be able to hold.
  //
  // If the RenderBuffer could not be created, this returns null.
  virtual std::unique_ptr<RenderBuffer> CreateVertexBuffer(
      RenderInternal, DataVolatility volatility, int vertex_size,
      int vertex_capacity) = 0;

  // Creates a new index buffer compatible with this backend.
  //
  // Indices are always uint16_t, and index_capacity is the minimum number of
  // indices the buffer must be able to hold.
  //
  // If the RenderBuffer could not be created, this returns null.
  virtual std::unique_ptr<RenderBuffer> CreateIndexBuffer(
      RenderInternal, DataVolatility volatility, int index_capacity) = 0;

  // Begins drawing the next frame.
  //
  // If the render backend is not currently able to render, this can return
  // false, at which point no draw commands will be issued and EndFrame will not
  // be called.
  virtual bool BeginFrame(RenderInternal) = 0;

  // Queues mesh to be be drawn the next time EndFrame is called.
  //
  // This will only be called after BeginFrame is called and before EndFrame
  // is called.
  virtual void Draw(RenderInternal, RenderScene* scene,
                    RenderPipeline* pipeline, BindingData* material_data,
                    BindingData* instance_data, RenderBuffer* vertices,
                    RenderBuffer* indices) = 0;

  // Queues an ordered list of draw commands to be executed next time EndFrame
  // is called.
  //
  // This will only be called after BeginFrame is called and before EndFrame
  // is called.
  virtual void Draw(RenderInternal, RenderScene* scene,
                    absl::Span<const DrawCommand> commands) = 0;

  // Completes draw operations, then renders and presents the frame to the
  // screen.
  virtual void EndFrame(RenderInternal) = 0;

 protected:
  RenderBackend() = default;
};

}  // namespace gb

#endif  // GB_RENDER_GB_RENDER_BACKEND_H_
