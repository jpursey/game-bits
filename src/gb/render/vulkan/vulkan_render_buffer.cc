// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_render_buffer.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "absl/container/inlined_vector.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/types/span.h"
#include "gb/render/vulkan/vulkan_render_state.h"

namespace gb {

namespace {

constexpr int kNeverUsedFrame = -1000;

//==============================================================================
// WrittenRanges
//==============================================================================

// The ranges of values written to a buffer since they were last copied to the
// device. The ranges are sorted, and none overlap or touch.
class WrittenRanges final {
 public:
  // The values from begin up to (but not including) end.
  struct Range {
    int begin;
    int end;
  };

  // If `can_copy_gaps` is true, the buffer copied from holds every value, so
  // values between the ranges may be copied too. Then more than kMaxRanges
  // ranges are merged into one, which bounds the copies scattered writes make.
  explicit WrittenRanges(bool can_copy_gaps) : can_copy_gaps_(can_copy_gaps) {}

  bool IsEmpty() const { return ranges_.empty(); }
  absl::Span<const Range> Get() const { return ranges_; }

  // Adds the `size` values starting at `offset`, merging them with any range
  // they overlap or touch.
  void Add(int offset, int size);

  void Clear() { ranges_.clear(); }

 private:
  static constexpr int kMaxRanges = 8;

  const bool can_copy_gaps_;
  absl::InlinedVector<Range, 1> ranges_;
};

void WrittenRanges::Add(int offset, int size) {
  int begin = offset;
  int end = offset + size;
  const int count = static_cast<int>(ranges_.size());
  int first = 0;
  while (first < count && ranges_[first].end < begin) {
    ++first;
  }
  int last = first;
  while (last < count && ranges_[last].begin <= end) {
    begin = std::min(begin, ranges_[last].begin);
    end = std::max(end, ranges_[last].end);
    ++last;
  }
  ranges_.erase(ranges_.begin() + first, ranges_.begin() + last);
  ranges_.insert(ranges_.begin() + first, {begin, end});
  if (can_copy_gaps_ && static_cast<int>(ranges_.size()) > kMaxRanges) {
    const Range all = {ranges_.front().begin, ranges_.back().end};
    ranges_.assign(1, all);
  }
}

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
  bool DoSet(int offset, const void* data, int size) override;
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
  bool PrepForWrite(int offset, int size);

  // The values written since the last transfer, which only the host buffer
  // holds. Other values are only on the device.
  WrittenRanges written_{/*can_copy_gaps=*/false};
  int render_frame_ = kNeverUsedFrame;
  std::unique_ptr<VulkanBuffer> device_buffer_;

  // Null while no values are written.
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

bool VulkanStaticWriteBuffer::PrepForWrite(int offset, int size) {
  if (host_buffer_ != nullptr) {
    return true;
  }

  // A device buffer in use is replaced, which loses the values the write
  // doesn't set, as they can't be read back.
  const bool in_use = FrameInUse(render_frame_);
  if (in_use && !SetsAllValues(offset, size)) {
    return false;
  }

  host_buffer_ = CreateHostBuffer();
  if (host_buffer_ == nullptr) {
    return false;
  }

  if (in_use) {
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
  if (!PrepForWrite(offset, size)) {
    return false;
  }
  written_.Add(offset, size);

  std::memset(static_cast<uint8_t*>(host_buffer_->GetData()) +
                  (offset * GetAlignSize()),
              0, size * GetValueSize());
  return true;
}

bool VulkanStaticWriteBuffer::DoSet(int offset, const void* data, int size) {
  RENDER_ASSERT(size == 1 || GetValueSize() == GetAlignSize());
  if (!PrepForWrite(offset, size)) {
    return false;
  }
  written_.Add(offset, size);

  std::memcpy(static_cast<uint8_t*>(host_buffer_->GetData()) +
                  (offset * GetAlignSize()),
              data, size * GetValueSize());
  return true;
}

void* VulkanStaticWriteBuffer::DoEditBegin() { return nullptr; }
void VulkanStaticWriteBuffer::OnEditEnd(bool modified) {}

void VulkanStaticWriteBuffer::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  if (written_.IsEmpty()) {
    return;
  }

  for (const WrittenRanges::Range& range : written_.Get()) {
    CopyBuffer(host_buffer_->Get(), device_buffer_->Get(), state, range.begin,
               range.end);
  }
  host_buffer_.reset();
  written_.Clear();
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
  bool DoSet(int offset, const void* data, int size) override;
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

  // Notes that values were written to the host buffer, to copy them to the
  // device buffer when it is next rendered.
  bool AddWritten(int offset, int size);

  std::unique_ptr<VulkanBuffer> device_buffer_;
  std::unique_ptr<VulkanBuffer> host_buffer_;

  // The values in the host buffer that the device buffer doesn't have yet.
  WrittenRanges written_{/*can_copy_gaps=*/true};
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
  if (!EnsureHostBufferIsWritable(
          /*copy_contents=*/!SetsAllValues(offset, size))) {
    return false;
  }
  std::memset(static_cast<uint8_t*>(host_buffer_->GetData()) +
                  (offset * GetAlignSize()),
              0, size * GetValueSize());
  return AddWritten(offset, size);
}

bool VulkanStaticReadWriteBuffer::DoSet(int offset, const void* data,
                                        int size) {
  RENDER_ASSERT(size == 1 || GetValueSize() == GetAlignSize());
  if (!EnsureHostBufferIsWritable(
          /*copy_contents=*/!SetsAllValues(offset, size))) {
    return false;
  }
  std::memcpy(static_cast<uint8_t*>(host_buffer_->GetData()) +
                  (offset * GetAlignSize()),
              data, size * GetValueSize());
  return AddWritten(offset, size);
}

void* VulkanStaticReadWriteBuffer::DoEditBegin() {
  if (!EnsureHostBufferIsWritable(true)) {
    return nullptr;
  }
  return host_buffer_->GetData();
}

void VulkanStaticReadWriteBuffer::OnEditEnd(bool modified) {
  if (modified) {
    AddWritten(0, GetCapacity());
  } else if (!written_.IsEmpty()) {
    // Values written before editing weren't copied while it was in progress,
    // and the device buffer may have been rendered since.
    EnsureDeviceBufferIsWritable();
  }
}

void VulkanStaticReadWriteBuffer::DoRender(VulkanRenderState* state) {
  render_frame_ = state->frame;

  // Values aren't copied while the host buffer is being edited.
  if (written_.IsEmpty() || IsEditing()) {
    return;
  }

  for (const WrittenRanges::Range& range : written_.Get()) {
    CopyBuffer(host_buffer_->Get(), device_buffer_->Get(), state, range.begin,
               range.end);
  }
  transfer_frame_ = render_frame_;
  written_.Clear();
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

  // This device buffer has never been rendered, and has none of the values.
  render_frame_ = kNeverUsedFrame;
  written_.Add(0, GetCapacity());
  return true;
}

bool VulkanStaticReadWriteBuffer::AddWritten(int offset, int size) {
  if (!EnsureDeviceBufferIsWritable()) {
    return false;
  }
  written_.Add(offset, size);
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
  bool DoSet(int offset, const void* data, int size) override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;
  void DoRender(VulkanRenderState* state) override;
  const void* DoGetData() const override;

 private:
  struct Buffers {
    Buffers() = default;

    // The values in the local buffer that this buffer doesn't have yet.
    WrittenRanges written{/*can_copy_gaps=*/true};
    std::unique_ptr<VulkanBuffer> buffer;
  };

  VulkanPerFrameBuffer(VulkanBackend* backend, VulkanBufferType type,
                       int value_size, int capacity, int align_size)
      : VulkanRenderBuffer(backend, type, DataVolatility::kPerFrame, value_size,
                           capacity, align_size) {}

  bool Init();

  // Notes that values were written to the local buffer, to copy them to each
  // frame's buffer when it is next rendered.
  void AddWritten(int offset, int size);

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
  AddWritten(offset, size);
  return true;
}

bool VulkanPerFrameBuffer::DoSet(int offset, const void* data, int size) {
  RENDER_ASSERT(size == 1 || GetValueSize() == GetAlignSize());
  std::memcpy(static_cast<uint8_t*>(local_buffer_) + (offset * GetAlignSize()),
              data, size * GetValueSize());
  AddWritten(offset, size);
  return true;
}

void* VulkanPerFrameBuffer::DoEditBegin() { return local_buffer_; }

void VulkanPerFrameBuffer::OnEditEnd(bool modified) {
  if (modified) {
    AddWritten(0, GetCapacity());
  }
}

void VulkanPerFrameBuffer::DoRender(VulkanRenderState* state) {
  auto& buffers = buffers_[state->frame % kMaxFramesInFlight];
  if (buffers.written.IsEmpty()) {
    return;
  }

  // TODO: We should just keep the data mapped at all times and call
  // vmaFlushAllocation after the memory is written, or add it to a list of
  // allocations to flush in RenderState.
  if (buffers.buffer->MapData()) {
    for (const WrittenRanges::Range& range : buffers.written.Get()) {
      const int end = std::min(range.end, GetSize());
      if (range.begin < end) {
        const int byte_offset = range.begin * GetAlignSize();
        std::memcpy(
            static_cast<uint8_t*>(buffers.buffer->GetData()) + byte_offset,
            static_cast<const uint8_t*>(local_buffer_) + byte_offset,
            (end - range.begin) * GetAlignSize());
      }
    }
    buffers.written.Clear();
    buffers.buffer->UnmapData();
  }
}

void VulkanPerFrameBuffer::AddWritten(int offset, int size) {
  for (auto& buffers : buffers_) {
    buffers.written.Add(offset, size);
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

bool VulkanRenderBuffer::DoGet(int offset, void* data, int size) const {
  const uint8_t* buffer_data = static_cast<const uint8_t*>(DoGetData());
  const int value_size = GetValueSize();
  if (align_size_ == value_size) {
    std::memcpy(data, buffer_data + (offset * value_size), size * value_size);
    return true;
  }
  uint8_t* out = static_cast<uint8_t*>(data);
  for (int i = 0; i < size; ++i) {
    std::memcpy(out + (i * value_size),
                buffer_data + ((offset + i) * align_size_), value_size);
  }
  return true;
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
                                    VulkanRenderState* state, int begin,
                                    int end) {
  const vk::DeviceSize copy_offset = begin * align_size_;
  const vk::DeviceSize copy_end = std::min(end, GetSize()) * align_size_;
  if (copy_end <= copy_offset) {
    return;
  }

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
  state->buffer_updates.emplace_back(host_buffer, device_buffer, dst_access,
                                     copy_offset, copy_end - copy_offset);
}

}  // namespace gb
