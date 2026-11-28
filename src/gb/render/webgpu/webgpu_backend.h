// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_BACKEND_H_
#define GB_RENDER_WEBGPU_WEBGPU_BACKEND_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "absl/container/flat_hash_set.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/base/validated_context.h"
#include "gb/image/image.h"
#include "gb/render/render_backend.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

//==============================================================================
// WebGpuPendingWrite
//==============================================================================

// A resource whose changes are written to the GPU as the next frame ends. It
// registers itself with WebGpuBackend::AddPendingWrite() as it changes, and
// removes itself with RemovePendingWrite() when destroyed.
class WebGpuPendingWrite {
 public:
  WebGpuPendingWrite(const WebGpuPendingWrite&) = delete;
  WebGpuPendingWrite& operator=(const WebGpuPendingWrite&) = delete;

  // Writes the changes to the GPU with `queue`, ahead of the frame's commands.
  virtual void WritePending(const wgpu::Queue& queue) = 0;

 protected:
  WebGpuPendingWrite() = default;
  ~WebGpuPendingWrite() = default;
};

//==============================================================================
// WebGpuBackend
//==============================================================================

// The WebGPU implementation of RenderBackend, for a program in a browser page.
//
// It draws into a canvas on the page (by default "#canvas", which is SDL's
// window), at the canvas's size, which is read as each frame begins. Frames
// are multisampled (4 samples) and sRGB, as Vulkan's swap chain is, and copied
// into the canvas as each frame ends.
//
// Every WebGPU error the browser reports is logged as an error.
//
// Writes to a resource are ordered with the frames around them: a change made
// after a frame ends is seen by the next frame, not the one ended. A resource
// releases its GPU memory when it is destroyed, which is safe because every
// command is recorded and submitted within EndFrame().
//
// A WGSL shader written by hand for CreateShaderCode() numbers its bindings as
// GetWgslBinding() (gb/render/webgpu/webgpu_format.h) says.
//
// This class meets the thread-safety standards required by all RenderBackend
// derived classes. WebGPU objects belong to the browser's main thread, so in a
// web build with threads, a factory method called on another thread logs an
// error and returns null.
class WebGpuBackend final : public RenderBackend {
 public:
  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // OPTIONAL: The CSS selector of the canvas to draw into.
  static inline constexpr const char* kKeyCanvasSelector = "CanvasSelector";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintCanvasSelector,
                                             kInOptional, std::string,
                                             kKeyCanvasSelector,
                                             std::string("#canvas"));

  // SCOPED: This backend instance.
  static GB_CONTEXT_CONSTRAINT(kConstraintBackend, kScoped, WebGpuBackend);

  using Contract =
      ContextContract<kConstraintCanvasSelector, kConstraintBackend>;

  // How long a wait for the browser (an adapter, a device, or a frame read
  // back) may take before it fails.
  static constexpr int kWaitTimeoutSecs = 10;

  // The number of samples each frame is drawn with.
  static constexpr int kSampleCount = 4;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates the backend, waiting for the browser's adapter and device. Returns
  // null, having logged why, if there is no device with what gb_render needs.
  //
  // Call it from main(), in a page linked with JSPI (as gb_add_win_executable
  // links one), on the browser's main thread.
  static std::unique_ptr<WebGpuBackend> Create(Contract contract);

  ~WebGpuBackend() override;

  //----------------------------------------------------------------------------
  // Properties
  //
  // Thread-safety: These are thread-safe.
  //----------------------------------------------------------------------------

  // Bytes of GPU memory in the buffers and textures the backend holds.
  int64_t GetAllocatedBytes() const { return allocated_bytes_; }

  //----------------------------------------------------------------------------
  // For the backend's resources
  //----------------------------------------------------------------------------

  const wgpu::Device& GetDevice() const { return device_; }
  const wgpu::Queue& GetQueue() const { return queue_; }

  // Adds `bytes` (which may be negative) to GetAllocatedBytes().
  void AddAllocatedBytes(int64_t bytes) { allocated_bytes_ += bytes; }

  // Registers `write` to be written as the next frame ends, if it isn't
  // already. It must call RemovePendingWrite() before it is destroyed.
  //
  // These are thread-safe.
  void AddPendingWrite(WebGpuPendingWrite* write);
  void RemovePendingWrite(WebGpuPendingWrite* write);

  //----------------------------------------------------------------------------
  // RenderBackend
  //----------------------------------------------------------------------------

  FrameDimensions GetFrameDimensions(RenderInternal) const override;
  bool CanFilter(RenderInternal, TextureFormat format) const override;
  Texture* CreateTexture(RenderInternal, ResourceEntry entry,
                         DataVolatility volatility, int width, int height,
                         const SamplerOptions& options,
                         TextureFormat format) override;
  TextureArray* CreateTextureArray(RenderInternal, ResourceEntry entry,
                                   DataVolatility volatility, int count,
                                   int width, int height,
                                   const SamplerOptions& options,
                                   TextureFormat format) override;
  VolumeTexture* CreateVolumeTexture(RenderInternal, ResourceEntry entry,
                                     DataVolatility volatility, int width,
                                     int height, int depth,
                                     const SamplerOptions& options,
                                     TextureFormat format) override;
  RenderTarget* CreateRenderTarget(RenderInternal, ResourceEntry entry,
                                   TextureFormat format,
                                   const RenderTargetSize& size, int width,
                                   int height,
                                   const SamplerOptions& options) override;
  std::unique_ptr<ShaderCode> CreateShaderCode(RenderInternal, const void* code,
                                               int64_t code_size) override;
  std::unique_ptr<BindingLayout> CreateBindingLayout(
      RenderInternal, absl::Span<const Binding> bindings) override;
  std::unique_ptr<RenderPass> CreatePass(RenderInternal,
                                         const RenderPassConfig& config,
                                         int pass_order) override;
  std::unique_ptr<RenderView> CreateView(RenderInternal, RenderPass* pass,
                                         BindingLayout* binding_layout,
                                         int view_order) override;
  std::unique_ptr<RenderPipeline> CreatePipeline(
      RenderInternal, BindingLayout* binding_layout,
      const VertexType* vertex_type, absl::Span<const Binding> bindings,
      ShaderCode* vertex_shader, ShaderCode* fragment_shader,
      const MaterialConfig& config) override;
  std::unique_ptr<RenderBuffer> CreateVertexBuffer(
      RenderInternal, DataVolatility volatility, int vertex_size,
      int vertex_capacity) override;
  std::unique_ptr<RenderBuffer> CreateIndexBuffer(RenderInternal,
                                                  DataVolatility volatility,
                                                  int index_capacity) override;
  std::unique_ptr<RenderBuffer> CreateStorageBuffer(RenderInternal,
                                                    DataVolatility volatility,
                                                    int value_size,
                                                    int capacity) override;
  std::unique_ptr<MeshPoolBuffers> CreateMeshPoolBuffers(
      RenderInternal, int vertex_size, IndexType index_type,
      int vertex_capacity, int index_capacity) override;
  void SetClearColor(RenderInternal, Pixel color) override;
  void SetDefaultBindings(RenderInternal,
                          const DefaultBindings& defaults) override;
  bool BeginFrame(RenderInternal,
                  absl::Span<RenderPass* const> passes) override;
  void Draw(RenderInternal, RenderView* view, RenderPipeline* pipeline,
            BindingData* material_data, BindingData* instance_data,
            RenderBuffer* vertices, RenderBuffer* indices) override;
  void Draw(RenderInternal, RenderView* view,
            absl::Span<const DrawCommand> commands) override;
  void EndFrame(RenderInternal) override;
  std::unique_ptr<Image> EndFrameAndReadImage(RenderInternal) override;

 private:
  explicit WebGpuBackend(ValidatedContext context);

  bool Init();
  bool InitDevice();
  bool InitSurface();

  // Waits for `future`, returning false, having logged why, if it fails or
  // takes longer than kWaitTimeoutSecs. `what` names what is waited for.
  bool Wait(const wgpu::Future& future, std::string_view what) const;

  // Returns true if WebGPU can be used on this thread, logging why `what`
  // fails if not.
  bool ValidateThread(std::string_view what) const;

  FrameDimensions DoGetFrameDimensions() const;

  // Logs that `what` isn't supported by the backend yet.
  void LogUnsupported(std::string_view what) const;

  // Configures the canvas, and makes the frame's textures, for a frame of the
  // size.
  void ResizeFrame(int width, int height);

  // Writes the pending changes, then encodes and submits the frame, copying it
  // into the canvas. If `read_buffer` is not null, the frame is also copied
  // into it, with rows `read_row_bytes` apart.
  void DoEndFrame(const wgpu::Buffer& read_buffer, int read_row_bytes);

  // Writes every pending change to the GPU.
  void WritePending();

  ValidatedContext context_;
  const std::string canvas_selector_;
  std::atomic<int64_t> allocated_bytes_ = 0;

  // Destroyed in the reverse order, the instance last.
  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  wgpu::Queue queue_;
  wgpu::Surface surface_;

  // The frame's multisampled color and depth, and the color resolved, which is
  // copied into the canvas.
  wgpu::Texture frame_color_;
  wgpu::Texture frame_depth_;
  wgpu::Texture frame_texture_;

  wgpu::Color clear_color_ = {0, 0, 0, 1};

  mutable absl::Mutex frame_mutex_;
  FrameDimensions frame_dimensions_ ABSL_GUARDED_BY(frame_mutex_) = {0, 0};

  absl::Mutex pending_mutex_;
  absl::flat_hash_set<WebGpuPendingWrite*> pending_writes_
      ABSL_GUARDED_BY(pending_mutex_);
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_BACKEND_H_
