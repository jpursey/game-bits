// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_BACKEND_H_
#define GB_RENDER_WEBGPU_WEBGPU_BACKEND_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/function_ref.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/base/validated_context.h"
#include "gb/image/image.h"
#include "gb/render/render_backend.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

// WebGpuPendingWrite and WebGpuBackend refer to each other, and the backend
// refers to the resources it draws with, which refer to it.
class WebGpuBackend;
class WebGpuBindingData;
class WebGpuRenderBuffer;
class WebGpuRenderPipeline;
class WebGpuView;

//==============================================================================
// WebGpuPendingWrite
//==============================================================================

// A resource of the backend with changes to write to the GPU as the next frame
// ends. A subclass calls MarkPendingWrite() as it changes, and the backend
// calls WritePending() once as the frame ends. The registration ends then, or
// when this is destroyed.
class WebGpuPendingWrite {
 public:
  WebGpuPendingWrite(const WebGpuPendingWrite&) = delete;
  WebGpuPendingWrite& operator=(const WebGpuPendingWrite&) = delete;

  // Writes the changes to the GPU: with `queue` for writes, which come before
  // the frame's commands, and with `encoder` for commands, which come before
  // the frame's passes.
  virtual void WritePending(const wgpu::Queue& queue,
                            const wgpu::CommandEncoder& encoder) = 0;

 protected:
  // `backend` must outlive this.
  explicit WebGpuPendingWrite(WebGpuBackend* backend) : backend_(backend) {}
  ~WebGpuPendingWrite();

  WebGpuBackend* GetBackend() const { return backend_; }

  // Registers this to be written as the next frame ends, if it isn't already.
  void MarkPendingWrite();

 private:
  WebGpuBackend* const backend_;
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
// after a frame ends is seen by the next frame, not the one ended. What is
// drawn may be destroyed before the frame ends, as the frame holds what it
// draws with until it is submitted.
//
// A WGSL shader written by hand for CreateShaderCode() puts each binding in the
// group of its BindingSet (0 for kView, 1 for kMaterial, 2 for kInstance), at
// the binding number GetWgslBinding() (gb/render/webgpu/webgpu_format.h) gives.
//
// This class meets the thread-safety standards required by all RenderBackend
// derived classes. WebGPU objects belong to the browser's main thread, so in a
// web build with threads, a factory method called on another thread logs an
// error and returns null, and the resources it makes are used only on the main
// thread too.
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
  const wgpu::Limits& GetLimits() const { return limits_; }
  const wgpu::Queue& GetQueue() const { return queue_; }

  // Adds `bytes` (which may be negative) to GetAllocatedBytes().
  void AddAllocatedBytes(int64_t bytes) { allocated_bytes_ += bytes; }

  // Calls `create`, which creates WebGPU objects, within a validation error
  // scope, and waits for the scope. Returns false, having logged the browser's
  // message, if `create` made an error. `what` names what was created.
  //
  // This returns to the browser while it waits, so it is for creation, not for
  // each frame.
  bool CreateInErrorScope(std::string_view what,
                          absl::FunctionRef<void()> create) const;

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
  friend class WebGpuPendingWrite;

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

  // Returns true if WebGPU can sample with `options`, logging why not if it
  // can't.
  bool ValidateSamplerOptions(const SamplerOptions& options) const;

  // Logs that `what` isn't supported by the backend yet.
  void LogUnsupported(std::string_view what) const;

  // Reads the canvas's size, and resizes the frame if it changed. Returns false
  // if the canvas has no size, or it can't be read.
  bool UpdateFrameSize();

  // Configures the canvas, and makes the frame's textures, for a frame of the
  // size.
  void ResizeFrame(int width, int height);

  // Releases the frame's textures, if there are any.
  void DestroyFrameTextures();

  // Writes the pending changes, then encodes and submits the frame, copying it
  // into the canvas. If `read_buffer` is not null, the frame is also copied
  // into it, with rows `read_row_bytes` apart.
  void DoEndFrame(const wgpu::Buffer& read_buffer, int read_row_bytes);

  // Encodes the frame's draws, in view order, into the frame's pass.
  void EncodeDraws(const wgpu::RenderPassEncoder& pass);

  // Writes every pending change to the GPU, with `encoder` for commands.
  void WritePending(const wgpu::CommandEncoder& encoder);

  ValidatedContext context_;
  const std::string canvas_selector_;
  std::atomic<int64_t> allocated_bytes_ = 0;

  // Released in the reverse order, the instance last, as the backend is
  // destroyed: a page whose program returns still holding WebGPU objects never
  // exits.
  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  wgpu::Limits limits_;
  wgpu::Queue queue_;
  wgpu::Surface surface_;

  // The frame's multisampled color and depth, and the color resolved, which is
  // copied into the canvas.
  wgpu::Texture frame_color_;
  wgpu::Texture frame_depth_;
  wgpu::Texture frame_texture_;
  wgpu::TextureView frame_color_view_;
  wgpu::TextureView frame_depth_view_;
  wgpu::TextureView frame_texture_view_;

  wgpu::Color clear_color_ = {0, 0, 0, 1};

  mutable absl::Mutex frame_mutex_;
  FrameDimensions frame_dimensions_ ABSL_GUARDED_BY(frame_mutex_) = {0, 0};

  // Used only on the main thread, as the resources are.
  absl::flat_hash_set<WebGpuPendingWrite*> pending_writes_;

  // A mesh drawn in the frame. It holds what it draws with, so what it was
  // drawn from may be destroyed before the frame ends.
  struct FrameDraw {
    wgpu::RenderPipeline pipeline;
    wgpu::BindGroup view_group;
    wgpu::BindGroup material_group;
    wgpu::BindGroup instance_group;
    wgpu::Buffer vertices;
    wgpu::Buffer indices;
    uint32_t index_count = 0;
  };

  // The frame's draws, by view order, from BeginFrame() until the frame ends.
  std::map<int, std::vector<FrameDraw>> frame_draws_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_BACKEND_H_
