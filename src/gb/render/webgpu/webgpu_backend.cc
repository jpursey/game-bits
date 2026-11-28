// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_backend.h"

#include <emscripten/html5.h>
#include <emscripten/threading.h>

#include <cstring>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/webgpu/webgpu_render_buffer.h"
#include "gb/render/webgpu/webgpu_texture.h"
#include "gb/render/webgpu/webgpu_texture_array.h"
#include "gb/render/webgpu/webgpu_volume_texture.h"

namespace gb {

namespace {

// The canvas is held as RGBA8Unorm, which every browser takes, and the frame
// as its sRGB form, which is copied into it byte for byte.
constexpr wgpu::TextureFormat kCanvasFormat = wgpu::TextureFormat::RGBA8Unorm;
constexpr wgpu::TextureFormat kFrameFormat =
    wgpu::TextureFormat::RGBA8UnormSrgb;
constexpr wgpu::TextureFormat kDepthFormat = wgpu::TextureFormat::Depth32Float;

// Rows of a texture copied to a buffer are this many bytes apart, at least.
constexpr int kCopyRowAlignment = 256;

constexpr uint64_t kWaitTimeoutNs =
    uint64_t{WebGpuBackend::kWaitTimeoutSecs} * 1'000'000'000;

std::string_view ToStringView(wgpu::StringView text) {
  return std::string_view(text);
}

wgpu::Texture CreateFrameTexture(const wgpu::Device& device,
                                 wgpu::TextureFormat format, int width,
                                 int height, int sample_count,
                                 wgpu::TextureUsage usage) {
  wgpu::TextureDescriptor descriptor;
  descriptor.usage = usage;
  descriptor.size = {static_cast<uint32_t>(width),
                     static_cast<uint32_t>(height), 1};
  descriptor.format = format;
  descriptor.sampleCount = static_cast<uint32_t>(sample_count);
  return device.CreateTexture(&descriptor);
}

}  // namespace

//------------------------------------------------------------------------------
// Construction / Destruction
//------------------------------------------------------------------------------

std::unique_ptr<WebGpuBackend> WebGpuBackend::Create(Contract contract) {
  ValidatedContext context(std::move(contract));
  if (!context.IsValid()) {
    return nullptr;
  }
  auto backend = absl::WrapUnique(new WebGpuBackend(std::move(context)));
  if (!backend->ValidateThread("Creating the backend") || !backend->Init()) {
    return nullptr;
  }
  return backend;
}

WebGpuBackend::WebGpuBackend(ValidatedContext context)
    : context_(std::move(context)),
      canvas_selector_(context_.GetValue<std::string>(kKeyCanvasSelector)) {}

WebGpuBackend::~WebGpuBackend() {
  // A page whose program returns still holding WebGPU objects never exits, so
  // each is released here, the device and instance last.
  if (frame_color_ != nullptr) {
    frame_color_.Destroy();
    frame_depth_.Destroy();
    frame_texture_.Destroy();
  }
  frame_color_ = nullptr;
  frame_depth_ = nullptr;
  frame_texture_ = nullptr;
  if (surface_ != nullptr) {
    surface_.Unconfigure();
  }
  surface_ = nullptr;
  queue_ = nullptr;
  device_ = nullptr;
  adapter_ = nullptr;
  instance_ = nullptr;
  LOG_IF(ERROR, !context_.Complete())
      << "WebGpuBackend's context is not complete";
}

bool WebGpuBackend::Init() {
  if (!InitDevice() || !InitSurface()) {
    return false;
  }
  context_.SetPtr<WebGpuBackend>(this);
  return true;
}

bool WebGpuBackend::InitDevice() {
  const wgpu::InstanceFeatureName instance_feature =
      wgpu::InstanceFeatureName::TimedWaitAny;
  wgpu::InstanceDescriptor instance_descriptor;
  instance_descriptor.requiredFeatureCount = 1;
  instance_descriptor.requiredFeatures = &instance_feature;
  instance_ = wgpu::CreateInstance(&instance_descriptor);
  if (instance_ == nullptr) {
    LOG(ERROR) << "This browser has no WebGPU";
    return false;
  }

  std::string adapter_error;
  wgpu::Future adapter_future = instance_.RequestAdapter(
      nullptr, wgpu::CallbackMode::WaitAnyOnly,
      [this, &adapter_error](wgpu::RequestAdapterStatus status,
                             wgpu::Adapter adapter, wgpu::StringView message) {
        if (status == wgpu::RequestAdapterStatus::Success) {
          adapter_ = std::move(adapter);
        } else {
          adapter_error = ToStringView(message);
        }
      });
  if (!Wait(adapter_future, "a WebGPU adapter")) {
    return false;
  }
  if (adapter_ == nullptr) {
    LOG(ERROR) << "No WebGPU adapter: " << adapter_error;
    return false;
  }

  // The device takes the adapter's limits where gb_render needs more than
  // WebGPU's defaults.
  std::vector<wgpu::FeatureName> features;
  if (!adapter_.HasFeature(wgpu::FeatureName::IndirectFirstInstance)) {
    LOG(ERROR) << "The WebGPU adapter lacks indirect-first-instance";
    return false;
  }
  features.push_back(wgpu::FeatureName::IndirectFirstInstance);
  if (adapter_.HasFeature(wgpu::FeatureName::Float32Filterable)) {
    features.push_back(wgpu::FeatureName::Float32Filterable);
  }
  wgpu::Limits adapter_limits;
  adapter_.GetLimits(&adapter_limits);
  wgpu::Limits limits;
  limits.maxTextureArrayLayers = adapter_limits.maxTextureArrayLayers;
  limits.maxBufferSize = adapter_limits.maxBufferSize;
  limits.maxStorageBufferBindingSize =
      adapter_limits.maxStorageBufferBindingSize;

  wgpu::DeviceDescriptor device_descriptor;
  device_descriptor.requiredFeatureCount = features.size();
  device_descriptor.requiredFeatures = features.data();
  device_descriptor.requiredLimits = &limits;
  device_descriptor.SetUncapturedErrorCallback(
      [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
        LOG(ERROR) << "WebGPU error (" << static_cast<int>(type)
                   << "): " << ToStringView(message);
      });
  device_descriptor.SetDeviceLostCallback(
      wgpu::CallbackMode::AllowSpontaneous,
      [](const wgpu::Device&, wgpu::DeviceLostReason reason,
         wgpu::StringView message) {
        if (reason != wgpu::DeviceLostReason::Destroyed &&
            reason != wgpu::DeviceLostReason::CallbackCancelled) {
          LOG(ERROR) << "The WebGPU device was lost: " << ToStringView(message);
        }
      });

  std::string device_error;
  wgpu::Future device_future = adapter_.RequestDevice(
      &device_descriptor, wgpu::CallbackMode::WaitAnyOnly,
      [this, &device_error](wgpu::RequestDeviceStatus status,
                            wgpu::Device device, wgpu::StringView message) {
        if (status == wgpu::RequestDeviceStatus::Success) {
          device_ = std::move(device);
        } else {
          device_error = ToStringView(message);
        }
      });
  if (!Wait(device_future, "a WebGPU device")) {
    return false;
  }
  if (device_ == nullptr) {
    LOG(ERROR) << "No WebGPU device: " << device_error;
    return false;
  }
  queue_ = device_.GetQueue();
  return true;
}

bool WebGpuBackend::InitSurface() {
  wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas;
  canvas.selector = std::string_view(canvas_selector_);
  wgpu::SurfaceDescriptor descriptor;
  descriptor.nextInChain = &canvas;
  surface_ = instance_.CreateSurface(&descriptor);
  if (surface_ == nullptr) {
    LOG(ERROR) << "Failed to create a WebGPU surface for the canvas \""
               << canvas_selector_ << "\"";
    return false;
  }
  return true;
}

bool WebGpuBackend::Wait(const wgpu::Future& future,
                         std::string_view what) const {
  const wgpu::WaitStatus status = instance_.WaitAny(future, kWaitTimeoutNs);
  if (status == wgpu::WaitStatus::Success) {
    return true;
  }
  if (status == wgpu::WaitStatus::TimedOut) {
    LOG(ERROR) << "Timed out waiting " << kWaitTimeoutSecs << " seconds for "
               << what;
  } else {
    LOG(ERROR) << "Failed to wait for " << what;
  }
  return false;
}

bool WebGpuBackend::ValidateThread(std::string_view what) const {
  if (emscripten_is_main_runtime_thread()) {
    return true;
  }
  LOG(ERROR) << what
             << " failed: the WebGPU backend is only used on the browser's "
                "main thread";
  return false;
}

void WebGpuBackend::LogUnsupported(std::string_view what) const {
  LOG(ERROR) << "The WebGPU backend doesn't support " << what << " yet";
}

//------------------------------------------------------------------------------
// Pending writes
//------------------------------------------------------------------------------

void WebGpuBackend::AddPendingWrite(WebGpuPendingWrite* write) {
  absl::MutexLock lock(&pending_mutex_);
  pending_writes_.insert(write);
}

void WebGpuBackend::RemovePendingWrite(WebGpuPendingWrite* write) {
  absl::MutexLock lock(&pending_mutex_);
  pending_writes_.erase(write);
}

void WebGpuBackend::WritePending() {
  // The lock is held while writing, so a resource can't be destroyed on
  // another thread as it is written.
  absl::MutexLock lock(&pending_mutex_);
  for (WebGpuPendingWrite* write : pending_writes_) {
    write->WritePending(queue_);
  }
  pending_writes_.clear();
}

//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

FrameDimensions WebGpuBackend::GetFrameDimensions(RenderInternal) const {
  return DoGetFrameDimensions();
}

FrameDimensions WebGpuBackend::DoGetFrameDimensions() const {
  absl::MutexLock lock(&frame_mutex_);
  return frame_dimensions_;
}

bool WebGpuBackend::CanFilter(RenderInternal, TextureFormat format) const {
  switch (format) {
    case TextureFormat::kRgba8Srgb:
    case TextureFormat::kRgba8:
    case TextureFormat::kRg8:
    case TextureFormat::kR8:
    case TextureFormat::kRgba4:
      return true;
    case TextureFormat::kDepth:
      return false;
  }
  return false;
}

//------------------------------------------------------------------------------
// Factory methods
//------------------------------------------------------------------------------

Texture* WebGpuBackend::CreateTexture(RenderInternal, ResourceEntry entry,
                                      DataVolatility volatility, int width,
                                      int height, const SamplerOptions& options,
                                      TextureFormat format) {
  if (!ValidateThread("Creating a texture")) {
    return nullptr;
  }
  if (options.address_mode == SamplerAddressMode::kClampBorder) {
    LOG(ERROR) << "WebGPU has no clamp to border: use kClampEdge";
    return nullptr;
  }
  return WebGpuTexture::Create(std::move(entry), this, volatility, width,
                               height, options, format);
}

TextureArray* WebGpuBackend::CreateTextureArray(
    RenderInternal, ResourceEntry entry, DataVolatility volatility, int count,
    int width, int height, const SamplerOptions& options,
    TextureFormat format) {
  if (!ValidateThread("Creating a texture array")) {
    return nullptr;
  }
  if (options.address_mode == SamplerAddressMode::kClampBorder) {
    LOG(ERROR) << "WebGPU has no clamp to border: use kClampEdge";
    return nullptr;
  }
  return WebGpuTextureArray::Create(std::move(entry), this, volatility, count,
                                    width, height, options, format);
}

VolumeTexture* WebGpuBackend::CreateVolumeTexture(
    RenderInternal, ResourceEntry entry, DataVolatility volatility, int width,
    int height, int depth, const SamplerOptions& options,
    TextureFormat format) {
  if (!ValidateThread("Creating a volume texture")) {
    return nullptr;
  }
  if (options.address_mode == SamplerAddressMode::kClampBorder) {
    LOG(ERROR) << "WebGPU has no clamp to border: use kClampEdge";
    return nullptr;
  }
  return WebGpuVolumeTexture::Create(std::move(entry), this, volatility, width,
                                     height, depth, options, format);
}

RenderTarget* WebGpuBackend::CreateRenderTarget(RenderInternal, ResourceEntry,
                                                TextureFormat,
                                                const RenderTargetSize&, int,
                                                int, const SamplerOptions&) {
  LogUnsupported("render targets");
  return nullptr;
}

std::unique_ptr<ShaderCode> WebGpuBackend::CreateShaderCode(RenderInternal,
                                                            const void*,
                                                            int64_t) {
  LogUnsupported("shader code");
  return nullptr;
}

std::unique_ptr<BindingLayout> WebGpuBackend::CreateBindingLayout(
    RenderInternal, absl::Span<const Binding>) {
  LogUnsupported("binding layouts");
  return nullptr;
}

std::unique_ptr<RenderPass> WebGpuBackend::CreatePass(RenderInternal,
                                                      const RenderPassConfig&,
                                                      int) {
  LogUnsupported("passes");
  return nullptr;
}

std::unique_ptr<RenderView> WebGpuBackend::CreateView(RenderInternal,
                                                      RenderPass*,
                                                      BindingLayout*, int) {
  LogUnsupported("views");
  return nullptr;
}

std::unique_ptr<RenderPipeline> WebGpuBackend::CreatePipeline(
    RenderInternal, BindingLayout*, const VertexType*,
    absl::Span<const Binding>, ShaderCode*, ShaderCode*,
    const MaterialConfig&) {
  LogUnsupported("pipelines");
  return nullptr;
}

std::unique_ptr<RenderBuffer> WebGpuBackend::CreateVertexBuffer(
    RenderInternal, DataVolatility volatility, int vertex_size,
    int vertex_capacity) {
  if (!ValidateThread("Creating a vertex buffer")) {
    return nullptr;
  }
  return WebGpuRenderBuffer::Create(this, wgpu::BufferUsage::Vertex, volatility,
                                    vertex_size, vertex_capacity);
}

std::unique_ptr<RenderBuffer> WebGpuBackend::CreateIndexBuffer(
    RenderInternal, DataVolatility volatility, int index_capacity) {
  if (!ValidateThread("Creating an index buffer")) {
    return nullptr;
  }
  return WebGpuRenderBuffer::Create(this, wgpu::BufferUsage::Index, volatility,
                                    sizeof(uint16_t), index_capacity);
}

std::unique_ptr<RenderBuffer> WebGpuBackend::CreateStorageBuffer(
    RenderInternal, DataVolatility volatility, int value_size, int capacity) {
  if (!ValidateThread("Creating a storage buffer")) {
    return nullptr;
  }

  // A storage buffer of MeshDraw values can be drawn from.
  return WebGpuRenderBuffer::Create(
      this, wgpu::BufferUsage::Storage | wgpu::BufferUsage::Indirect,
      volatility, value_size, capacity);
}

std::unique_ptr<MeshPoolBuffers> WebGpuBackend::CreateMeshPoolBuffers(
    RenderInternal, int, IndexType, int, int) {
  LogUnsupported("mesh pools");
  return nullptr;
}

//------------------------------------------------------------------------------
// Rendering
//------------------------------------------------------------------------------

void WebGpuBackend::SetClearColor(RenderInternal, Pixel color) {
  // The frame is sRGB, so the clear color, as Vulkan takes it, is encoded as
  // the frame is written.
  clear_color_ = {color.r / 255.0, color.g / 255.0, color.b / 255.0,
                  color.a / 255.0};
}

void WebGpuBackend::SetDefaultBindings(RenderInternal, const DefaultBindings&) {
}

bool WebGpuBackend::BeginFrame(RenderInternal, absl::Span<RenderPass* const>) {
  int width = 0;
  int height = 0;
  if (emscripten_get_canvas_element_size(canvas_selector_.c_str(), &width,
                                         &height) !=
      EMSCRIPTEN_RESULT_SUCCESS) {
    LOG(ERROR) << "Failed to read the size of the canvas \"" << canvas_selector_
               << "\"";
    return false;
  }
  if (width <= 0 || height <= 0) {
    return false;
  }
  bool resized = false;
  {
    absl::MutexLock lock(&frame_mutex_);
    if (frame_dimensions_.width != width ||
        frame_dimensions_.height != height) {
      frame_dimensions_ = {width, height};
      resized = true;
    }
  }
  if (resized) {
    ResizeFrame(width, height);
  }
  return true;
}

void WebGpuBackend::ResizeFrame(int width, int height) {
  wgpu::SurfaceConfiguration config;
  config.device = device_;
  config.format = kCanvasFormat;
  config.usage =
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst;
  config.width = static_cast<uint32_t>(width);
  config.height = static_cast<uint32_t>(height);
  config.alphaMode = wgpu::CompositeAlphaMode::Opaque;
  surface_.Configure(&config);

  if (frame_color_ != nullptr) {
    frame_color_.Destroy();
    frame_depth_.Destroy();
    frame_texture_.Destroy();
  }
  frame_color_ =
      CreateFrameTexture(device_, kFrameFormat, width, height, kSampleCount,
                         wgpu::TextureUsage::RenderAttachment);
  frame_depth_ =
      CreateFrameTexture(device_, kDepthFormat, width, height, kSampleCount,
                         wgpu::TextureUsage::RenderAttachment);
  frame_texture_ = CreateFrameTexture(
      device_, kFrameFormat, width, height, 1,
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc);
}

void WebGpuBackend::Draw(RenderInternal, RenderView*, RenderPipeline*,
                         BindingData*, BindingData*, RenderBuffer*,
                         RenderBuffer*) {
  // Nothing can be drawn yet, as no view or pipeline can be created.
}

void WebGpuBackend::Draw(RenderInternal, RenderView*,
                         absl::Span<const DrawCommand>) {
  // Nothing can be drawn yet, as no view can be created.
}

void WebGpuBackend::EndFrame(RenderInternal) { DoEndFrame(nullptr, 0); }

std::unique_ptr<Image> WebGpuBackend::EndFrameAndReadImage(RenderInternal) {
  const FrameDimensions frame = DoGetFrameDimensions();
  const int row_bytes = (frame.width * 4 + kCopyRowAlignment - 1) /
                        kCopyRowAlignment * kCopyRowAlignment;
  const int64_t buffer_size = int64_t{row_bytes} * frame.height;
  wgpu::BufferDescriptor descriptor;
  descriptor.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  descriptor.size = static_cast<uint64_t>(buffer_size);
  wgpu::Buffer buffer = device_.CreateBuffer(&descriptor);
  DoEndFrame(buffer, row_bytes);

  bool mapped = false;
  wgpu::Future future = buffer.MapAsync(
      wgpu::MapMode::Read, 0, static_cast<size_t>(buffer_size),
      wgpu::CallbackMode::WaitAnyOnly,
      [&mapped](wgpu::MapAsyncStatus status, wgpu::StringView message) {
        if (status == wgpu::MapAsyncStatus::Success) {
          mapped = true;
        } else {
          LOG(ERROR) << "Failed to map the frame read back: "
                     << ToStringView(message);
        }
      });
  if (!Wait(future, "the frame to be read back") || !mapped) {
    buffer.Destroy();
    return nullptr;
  }

  // The canvas is opaque, so the image is too.
  const uint8_t* src =
      static_cast<const uint8_t*>(buffer.GetConstMappedRange());
  auto image = std::make_unique<Image>(frame.width, frame.height);
  Pixel* dst = image->Edit().ModifyPixels();
  for (int y = 0; y < frame.height; ++y) {
    std::memcpy(dst + int64_t{y} * frame.width, src + int64_t{y} * row_bytes,
                int64_t{frame.width} * 4);
    for (int x = 0; x < frame.width; ++x) {
      dst[int64_t{y} * frame.width + x].a = 255;
    }
  }
  buffer.Unmap();
  buffer.Destroy();
  return image;
}

void WebGpuBackend::DoEndFrame(const wgpu::Buffer& read_buffer,
                               int read_row_bytes) {
  WritePending();
  const FrameDimensions frame = DoGetFrameDimensions();
  const wgpu::Extent3D frame_size = {static_cast<uint32_t>(frame.width),
                                     static_cast<uint32_t>(frame.height), 1};

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  wgpu::RenderPassColorAttachment color;
  color.view = frame_color_.CreateView();
  color.resolveTarget = frame_texture_.CreateView();
  color.loadOp = wgpu::LoadOp::Clear;
  color.storeOp = wgpu::StoreOp::Discard;
  color.clearValue = clear_color_;
  wgpu::RenderPassDepthStencilAttachment depth;
  depth.view = frame_depth_.CreateView();
  depth.depthLoadOp = wgpu::LoadOp::Clear;
  depth.depthStoreOp = wgpu::StoreOp::Discard;
  depth.depthClearValue = 1.0f;
  wgpu::RenderPassDescriptor pass_descriptor;
  pass_descriptor.colorAttachmentCount = 1;
  pass_descriptor.colorAttachments = &color;
  pass_descriptor.depthStencilAttachment = &depth;
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&pass_descriptor);
  pass.End();

  // The canvas has a new texture after every return to the browser, so it is
  // only taken here, as the frame is copied into it.
  wgpu::SurfaceTexture canvas;
  surface_.GetCurrentTexture(&canvas);
  wgpu::TexelCopyTextureInfo source;
  source.texture = frame_texture_;
  if (canvas.status == wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal ||
      canvas.status ==
          wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
    wgpu::TexelCopyTextureInfo destination;
    destination.texture = canvas.texture;
    encoder.CopyTextureToTexture(&source, &destination, &frame_size);
  } else {
    LOG(ERROR) << "Failed to get the canvas's texture ("
               << static_cast<int>(canvas.status) << ")";
  }
  if (read_buffer != nullptr) {
    wgpu::TexelCopyBufferInfo destination;
    destination.buffer = read_buffer;
    destination.layout.bytesPerRow = static_cast<uint32_t>(read_row_bytes);
    destination.layout.rowsPerImage = static_cast<uint32_t>(frame.height);
    encoder.CopyTextureToBuffer(&source, &destination, &frame_size);
  }
  wgpu::CommandBuffer commands = encoder.Finish();
  queue_.Submit(1, &commands);
}

}  // namespace gb
