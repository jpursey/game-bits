// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_render_buffer.h"

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/vulkan/vulkan_render_state.h"

namespace gb {

namespace {

constexpr int kNeverUsedFrame = -1000;

//==============================================================================
// VulkanStaticWriteBuffer
//==============================================================================

class VulkanStaticWriteBuffer final : public VulkanRenderBuffer {
 public:
  static std::unique_ptr<VulkanStaticWriteBuffer> Create(VulkanBackend* backend,
                                                         VulkanBufferType type,
                                                         int value_size,
                                                         int capacity,
                                                         int align_size);
  ~VulkanStaticWriteBuffer() override;

 protected:
  bool DoClear(int offset, int size) override;
  bool DoSet(const void* data, int size) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;
  const void* DoGetData() const override;

 private:
  VulkanStaticWriteBuffer(VulkanBackend* backend, VulkanBufferType type,
                          int value_size, int capacity, int align_size)
      : VulkanRenderBuffer(backend, type, DataVolatility::kStaticWrite,
                           value_size, capacity, align_size) {}

  bool Init();
  bool PrepForWrite();

  bool dirty_ = false;
  int render_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanBuffer> device_buffer_;
  std::unique_ptr<VulkanBuffer> host_buffer_;
};

std::unique_ptr<VulkanStaticWriteBuffer> VulkanStaticWriteBuffer::Create(
    VulkanBackend* backend, VulkanBufferType type, int value_size, int capacity,
    int align_size) {
  auto render_buffer = absl::WrapUnique(new VulkanStaticWriteBuffer(
      backend, type, value_size, capacity, align_size));
  if (!render_buffer->Init()) {
    return nullptr;
  }
  return render_buffer;
}

bool VulkanStaticWriteBuffer::Init() {
  device_buffer_ = CreateDeviceBuffer();
  if (device_buffer_ == nullptr) {
    return false;
  }
  SetAllBufferHandles(device_buffer_->Get());
  return true;
}

VulkanStaticWriteBuffer::~VulkanStaticWriteBuffer() {}

bool VulkanStaticWriteBuffer::PrepForWrite() {
  RENDER_ASSERT(!dirty_);

  host_buffer_ = CreateHostBuffer();
  if (host_buffer_ == nullptr) {
    return false;
  }

  if (FrameInUse(render_frame_)) {
    auto device_buffer = CreateDeviceBuffer();
    if (device_buffer == nullptr) {
      host_buffer_.reset();
      return false;
    }
    device_buffer_ = std::move(device_buffer);
    SetAllBufferHandles(device_buffer_->Get());
    render_frame_ = kNeverUsedFrame;
  }
  return true;
}

bool VulkanStaticWriteBuffer::DoClear(int offset, int size) {
  RENDER_ASSERT(offset == 0 || GetValueSize() == GetAlignSize());
  if (dirty_) {
    std::memset(static_cast<uint8_t*>(host_buffer_->GetData()) +
                    (offset * GetAlignSize()),
                0, size * GetValueSize());
    return true;
  }

  if (!PrepForWrite()) {
    return false;
  }

  std::memset(static_cast<uint8_t*>(host_buffer_->GetData()) +
                  (offset * GetAlignSize()),
              0, size * GetValueSize());
  dirty_ = true;
  return true;
}

bool VulkanStaticWriteBuffer::DoSet(const void* data, int size) {
  RENDER_ASSERT(size == 1 || GetValueSize() == GetAlignSize());
  if (dirty_) {
    std::memcpy(host_buffer_->GetData(), data, size * GetValueSize());
    return true;
  }

  if (!PrepForWrite()) {
    return false;
  }

  std::memcpy(host_buffer_->GetData(), data, size * GetValueSize());
  dirty_ = true;
  return true;
}

void* VulkanStaticWriteBuffer::DoEditBegin() { return nullptr; }
void VulkanStaticWriteBuffer::OnEditEnd(bool modified) {}

void VulkanStaticWriteBuffer::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  if (!dirty_) {
    return;
  }

  CopyBuffer(host_buffer_->Get(), device_buffer_->Get(), state);
  host_buffer_.reset();
  dirty_ = false;
}

const void* VulkanStaticWriteBuffer::DoGetData() const { return nullptr; }

//==============================================================================
// VulkanStaticReadWriteBuffer
//==============================================================================

class VulkanStaticReadWriteBuffer final : public VulkanRenderBuffer {
 public:
  static std::unique_ptr<VulkanStaticReadWriteBuffer> Create(
      VulkanBackend* backend, VulkanBufferType type, int value_size,
      int capacity, int align_size);
  ~VulkanStaticReadWriteBuffer() override;

 protected:
  bool DoClear(int offset, int size) override;
  bool DoSet(const void* data, int size) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;
  const void* DoGetData() const override;

 private:
  VulkanStaticReadWriteBuffer(VulkanBackend* backend, VulkanBufferType type,
                              int value_size, int capacity, int align_size)
      : VulkanRenderBuffer(backend, type, DataVolatility::kStaticReadWrite,
                           value_size, capacity, align_size) {}

  bool Init();
  bool EnsureHostBufferIsWritable(bool copy_contents);
  bool EnsureDeviceBufferIsWritable();

  std::unique_ptr<VulkanBuffer> device_buffer_;
  std::unique_ptr<VulkanBuffer> host_buffer_;
  bool was_dirty_ = false;
  bool dirty_ = false;
  int render_frame_ = kNeverUsedFrame;
  int transfer_frame_ = kNeverUsedFrame;
};

std::unique_ptr<VulkanStaticReadWriteBuffer>
VulkanStaticReadWriteBuffer::Create(VulkanBackend* backend,
                                    VulkanBufferType type, int value_size,
                                    int capacity, int align_size) {
  auto render_buffer = absl::WrapUnique(new VulkanStaticReadWriteBuffer(
      backend, type, value_size, capacity, align_size));
  if (!render_buffer->Init()) {
    return nullptr;
  }
  return render_buffer;
}

bool VulkanStaticReadWriteBuffer::Init() {
  device_buffer_ = CreateDeviceBuffer();
  host_buffer_ = CreateHostBuffer();
  if (device_buffer_ == nullptr || host_buffer_ == nullptr) {
    return false;
  }
  SetAllBufferHandles(device_buffer_->Get());
  return true;
}

VulkanStaticReadWriteBuffer::~VulkanStaticReadWriteBuffer() {}

bool VulkanStaticReadWriteBuffer::DoClear(int offset, int size) {
  RENDER_ASSERT(offset == 0 || GetValueSize() == GetAlignSize());

  if (!EnsureHostBufferIsWritable(false)) {
    return false;
  }
  std::memset(static_cast<uint8_t*>(host_buffer_->GetData()) +
                  (offset * GetAlignSize()),
              0, size * GetValueSize());

  if (dirty_) {
    return true;
  }

  if (!EnsureDeviceBufferIsWritable()) {
    return false;
  }
  dirty_ = true;
  return true;
}

bool VulkanStaticReadWriteBuffer::DoSet(const void* data, int size) {
  RENDER_ASSERT(size == 1 || GetValueSize() == GetAlignSize());
  if (!EnsureHostBufferIsWritable(false)) {
    return false;
  }
  std::memcpy(host_buffer_->GetData(), data, size * GetValueSize());

  if (dirty_) {
    return true;
  }

  if (!EnsureDeviceBufferIsWritable()) {
    return false;
  }
  dirty_ = true;
  return true;
}

void* VulkanStaticReadWriteBuffer::DoEditBegin() {
  if (!EnsureHostBufferIsWritable(true)) {
    return nullptr;
  }

  // Explicitly clear the dirty flag, as we do not want to initiate a transfer
  // until OnEditEnd is called.
  was_dirty_ = dirty_;
  dirty_ = false;
  return host_buffer_->GetData();
}

void VulkanStaticReadWriteBuffer::OnEditEnd(bool modified) {
  if (!modified) {
    dirty_ = was_dirty_;
    return;
  }
  if (!EnsureDeviceBufferIsWritable()) {
    return;
  }
  dirty_ = true;
}

void VulkanStaticReadWriteBuffer::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  if (!dirty_) {
    return;
  }

  CopyBuffer(host_buffer_->Get(), device_buffer_->Get(), state);
  transfer_frame_ = render_frame_;
  dirty_ = false;
}

const void* VulkanStaticReadWriteBuffer::DoGetData() const {
  return host_buffer_->GetData();
}

bool VulkanStaticReadWriteBuffer::EnsureHostBufferIsWritable(
    bool copy_contents) {
  if (!FrameInUse(transfer_frame_)) {
    return true;
  }

  auto host_buffer = CreateHostBuffer();
  if (host_buffer == nullptr) {
    return false;
  }
  if (copy_contents) {
    std::memcpy(host_buffer->GetData(), host_buffer_->GetData(),
                host_buffer_->GetSize());
  }
  host_buffer_ = std::move(host_buffer);

  // This host buffer has never been transferred.
  transfer_frame_ = kNeverUsedFrame;
  return true;
}

bool VulkanStaticReadWriteBuffer::EnsureDeviceBufferIsWritable() {
  if (!FrameInUse(render_frame_)) {
    return true;
  }

  auto device_buffer = CreateDeviceBuffer();
  if (device_buffer == nullptr) {
    return false;
  }
  device_buffer_ = std::move(device_buffer);
  SetAllBufferHandles(device_buffer_->Get());

  // This device buffer has never been rendered.
  render_frame_ = kNeverUsedFrame;
  return true;
}

//==============================================================================
// VulkanPerFrameBuffer
//==============================================================================

class VulkanPerFrameBuffer : public VulkanRenderBuffer {
 public:
  static std::unique_ptr<VulkanPerFrameBuffer> Create(VulkanBackend* backend,
                                                      VulkanBufferType type,
                                                      int value_size,
                                                      int capacity,
                                                      int align_size);
  ~VulkanPerFrameBuffer() override;

 protected:
  bool DoClear(int offset, int size) override;
  bool DoSet(const void* data, int size) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;
  const void* DoGetData() const override;

 private:
  struct Buffers {
    Buffers() = default;
    bool dirty = false;
    std::unique_ptr<VulkanBuffer> buffer;
  };

  VulkanPerFrameBuffer(VulkanBackend* backend, VulkanBufferType type,
                       int value_size, int capacity, int align_size)
      : VulkanRenderBuffer(backend, type, DataVolatility::kPerFrame, value_size,
                           capacity, align_size) {}

  bool Init();

  void* local_buffer_ = nullptr;
  Buffers buffers_[kMaxFramesInFlight];
};

std::unique_ptr<VulkanPerFrameBuffer> VulkanPerFrameBuffer::Create(
    VulkanBackend* backend, VulkanBufferType type, int value_size, int capacity,
    int align_size) {
  auto render_buffer = absl::WrapUnique(new VulkanPerFrameBuffer(
      backend, type, value_size, capacity, align_size));
  if (!render_buffer->Init()) {
    return nullptr;
  }
  return render_buffer;
}

bool VulkanPerFrameBuffer::Init() {
  for (auto& buffers : buffers_) {
    buffers.buffer = CreateDeviceBuffer(VMA_MEMORY_USAGE_CPU_TO_GPU);
    if (buffers.buffer == nullptr) {
      return false;
    }
  }
  local_buffer_ = std::calloc(GetAlignSize() * GetCapacity(), 1);
  if (local_buffer_ == nullptr) {
    return false;
  }
  for (int i = 0; i < kMaxFramesInFlight; ++i) {
    SetBufferHandle(i, buffers_[i].buffer->Get());
  }
  return true;
}

VulkanPerFrameBuffer::~VulkanPerFrameBuffer() { std::free(local_buffer_); }

bool VulkanPerFrameBuffer::DoClear(int offset, int size) {
  RENDER_ASSERT(offset == 0 || GetValueSize() == GetAlignSize());
  std::memset(static_cast<uint8_t*>(local_buffer_) + (offset * GetAlignSize()),
              0, size * GetValueSize());
  for (auto& buffers : buffers_) {
    buffers.dirty = true;
  }
  return true;
}

bool VulkanPerFrameBuffer::DoSet(const void* data, int size) {
  RENDER_ASSERT(size == 1 || GetValueSize() == GetAlignSize());
  std::memcpy(local_buffer_, data, size * GetValueSize());
  for (auto& buffers : buffers_) {
    buffers.dirty = true;
  }
  return true;
}

void* VulkanPerFrameBuffer::DoEditBegin() { return local_buffer_; }

void VulkanPerFrameBuffer::OnEditEnd(bool modified) {
  if (modified) {
    for (auto& buffers : buffers_) {
      buffers.dirty = true;
    }
  }
}

void VulkanPerFrameBuffer::DoRender(VulkanRenderState* state) {
  auto& buffers = buffers_[state->frame % kMaxFramesInFlight];
  if (!buffers.dirty) {
    return;
  }

  // TODO: We should just keep the data mapped at all times and call
  // vmaFlushAllocation after the memory is written, or add it to a list of
  // allocations to flush in RenderState.
  if (buffers.buffer->MapData()) {
    std::memcpy(buffers.buffer->GetData(), local_buffer_,
                GetSize() * GetAlignSize());
    buffers.dirty = false;
    buffers.buffer->UnmapData();
  }
}

const void* VulkanPerFrameBuffer::DoGetData() const { return local_buffer_; }

}  // namespace

//==============================================================================
// VulkanRenderBuffer
//==============================================================================

std::unique_ptr<VulkanRenderBuffer> VulkanRenderBuffer::Create(
    VulkanInternal, VulkanBackend* backend, VulkanBufferType type,
    DataVolatility volatility, int value_size, int capacity, int align_size) {
  switch (volatility) {
    case DataVolatility::kInvalid:
      break;
    case DataVolatility::kStaticWrite:
      return VulkanStaticWriteBuffer::Create(backend, type, value_size,
                                             capacity, align_size);
    case DataVolatility::kStaticReadWrite:
      return VulkanStaticReadWriteBuffer::Create(backend, type, value_size,
                                                 capacity, align_size);
    case DataVolatility::kPerFrame:
      return VulkanPerFrameBuffer::Create(backend, type, value_size, capacity,
                                          align_size);
  }
  return nullptr;
}

VulkanRenderBuffer::~VulkanRenderBuffer() {
  LOG_IF(ERROR, IsEditing())
      << "View still active in VulkanRenderBuffer destructor.";
}

void VulkanRenderBuffer::GetValue(int offset, void* data) {
  RENDER_ASSERT(align_size_ >= GetValueSize());
  const uint8_t* buffer_data = static_cast<const uint8_t*>(DoGetData());
  if (buffer_data == nullptr) {
    return;
  }
  std::memcpy(data, buffer_data + (offset * align_size_), GetValueSize());
}

void VulkanRenderBuffer::SetValue(int offset, const void* data) {
  RENDER_ASSERT(align_size_ >= GetValueSize());
  uint8_t* buffer_data = static_cast<uint8_t*>(DoEditBegin());
  if (buffer_data == nullptr) {
    return;
  }
  std::memcpy(buffer_data + (offset * align_size_), data, GetValueSize());
  OnEditEnd(true);
}

std::unique_ptr<VulkanBuffer> VulkanRenderBuffer::CreateDeviceBuffer(
    VmaMemoryUsage memory_usage) {
  vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eTransferDst;
  switch (type_) {
    case VulkanBufferType::kUniform:
      usage |= vk::BufferUsageFlagBits::eUniformBuffer;
      break;
    case VulkanBufferType::kVertex:
      usage |= vk::BufferUsageFlagBits::eVertexBuffer;
      break;
    case VulkanBufferType::kIndex:
      usage |= vk::BufferUsageFlagBits::eIndexBuffer;
      break;
  }
  return VulkanBuffer::Create(backend_,
                              vk::BufferCreateInfo()
                                  .setSize(GetCapacity() * align_size_)
                                  .setUsage(usage)
                                  .setSharingMode(vk::SharingMode::eExclusive),
                              memory_usage);
}

std::unique_ptr<VulkanBuffer> VulkanRenderBuffer::CreateHostBuffer() {
  return VulkanBuffer::Create(
      backend_,
      vk::BufferCreateInfo()
          .setSize(GetCapacity() * align_size_)
          .setUsage(vk::BufferUsageFlagBits::eTransferSrc)
          .setSharingMode(vk::SharingMode::eExclusive),
      VMA_MEMORY_USAGE_CPU_ONLY);
}

void VulkanRenderBuffer::CopyBuffer(vk::Buffer host_buffer,
                                    vk::Buffer device_buffer,
                                    VulkanRenderState* state) {
  vk::AccessFlags dst_access;
  switch (type_) {
    case VulkanBufferType::kUniform:
      dst_access = vk::AccessFlagBits::eUniformRead;
      break;
    case VulkanBufferType::kVertex:
      dst_access = vk::AccessFlagBits::eVertexAttributeRead;
      break;
    case VulkanBufferType::kIndex:
      dst_access = vk::AccessFlagBits::eIndexRead;
      break;
    default:
      LOG(FATAL) << "Unhandled buffer type for CopyBuffer";
  }
  vk::DeviceSize copy_size = GetSize() * align_size_;
  state->buffer_updates.emplace_back(host_buffer, device_buffer, dst_access,
                                     copy_size);
}

}  // namespace gb
