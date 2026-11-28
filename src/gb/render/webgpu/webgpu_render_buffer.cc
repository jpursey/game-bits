// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_render_buffer.h"

#include <algorithm>
#include <cstring>

#include "absl/log/log.h"
#include "absl/memory/memory.h"

namespace gb {

namespace {

// WebGPU writes buffers in whole words of this many bytes.
constexpr int64_t kWriteAlignment = 4;

int64_t AlignUp(int64_t value) {
  return (value + kWriteAlignment - 1) / kWriteAlignment * kWriteAlignment;
}

}  // namespace

std::unique_ptr<WebGpuRenderBuffer> WebGpuRenderBuffer::Create(
    WebGpuBackend* backend, wgpu::BufferUsage usage, DataVolatility volatility,
    int value_size, int capacity) {
  const int64_t gpu_bytes =
      std::max(AlignUp(int64_t{value_size} * capacity), kWriteAlignment);
  wgpu::BufferDescriptor descriptor;
  descriptor.usage = usage | wgpu::BufferUsage::CopyDst;
  descriptor.size = static_cast<uint64_t>(gpu_bytes);
  wgpu::Buffer buffer = backend->GetDevice().CreateBuffer(&descriptor);
  if (buffer == nullptr) {
    LOG(ERROR) << "Failed to create a WebGPU buffer of " << gpu_bytes
               << " bytes";
    return nullptr;
  }
  return absl::WrapUnique(new WebGpuRenderBuffer(
      backend, volatility, value_size, capacity, std::move(buffer), gpu_bytes));
}

WebGpuRenderBuffer::WebGpuRenderBuffer(WebGpuBackend* backend,
                                       DataVolatility volatility,
                                       int value_size, int capacity,
                                       wgpu::Buffer buffer, int64_t gpu_bytes)
    : RenderBuffer(volatility, value_size, capacity),
      backend_(backend),
      buffer_(std::move(buffer)),
      gpu_bytes_(gpu_bytes),
      data_(gpu_bytes) {
  backend_->AddAllocatedBytes(gpu_bytes_);
}

WebGpuRenderBuffer::~WebGpuRenderBuffer() {
  backend_->RemovePendingWrite(this);
  buffer_.Destroy();
  backend_->AddAllocatedBytes(-gpu_bytes_);
}

bool WebGpuRenderBuffer::DoClear(int offset, int size) {
  const int64_t begin = int64_t{offset} * GetValueSize();
  const int64_t end = begin + int64_t{size} * GetValueSize();
  std::memset(data_.data() + begin, 0, end - begin);
  MarkPending(begin, end);
  return true;
}

bool WebGpuRenderBuffer::DoSet(int offset, const void* data, int size) {
  const int64_t begin = int64_t{offset} * GetValueSize();
  const int64_t end = begin + int64_t{size} * GetValueSize();
  std::memcpy(data_.data() + begin, data, end - begin);
  MarkPending(begin, end);
  return true;
}

bool WebGpuRenderBuffer::DoGet(int offset, void* data, int size) const {
  std::memcpy(data, data_.data() + int64_t{offset} * GetValueSize(),
              int64_t{size} * GetValueSize());
  return true;
}

void* WebGpuRenderBuffer::DoEditBegin() { return data_.data(); }

void WebGpuRenderBuffer::OnEditEnd(bool modified) {
  if (modified) {
    MarkPending(0, int64_t{GetCapacity()} * GetValueSize());
  }
}

void WebGpuRenderBuffer::MarkPending(int64_t begin, int64_t end) {
  if (begin >= end) {
    return;
  }
  if (GetVolatility() == DataVolatility::kStaticWrite) {
    WriteRange(backend_->GetQueue(), begin, end);
    return;
  }
  if (pending_begin_ >= pending_end_) {
    pending_begin_ = begin;
    pending_end_ = end;
    backend_->AddPendingWrite(this);
    return;
  }
  pending_begin_ = std::min(pending_begin_, begin);
  pending_end_ = std::max(pending_end_, end);
}

void WebGpuRenderBuffer::WritePending(const wgpu::Queue& queue) {
  if (pending_begin_ >= pending_end_) {
    return;
  }
  WriteRange(queue, pending_begin_, pending_end_);
  pending_begin_ = pending_end_ = 0;
}

void WebGpuRenderBuffer::WriteRange(const wgpu::Queue& queue, int64_t begin,
                                    int64_t end) {
  const int64_t aligned_begin = begin / kWriteAlignment * kWriteAlignment;
  const int64_t aligned_end = AlignUp(end);
  queue.WriteBuffer(buffer_, static_cast<uint64_t>(aligned_begin),
                    data_.data() + aligned_begin,
                    static_cast<size_t>(aligned_end - aligned_begin));
}

}  // namespace gb
