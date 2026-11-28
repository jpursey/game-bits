// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_RENDER_BUFFER_H_
#define GB_RENDER_WEBGPU_WEBGPU_RENDER_BUFFER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "gb/render/render_buffer.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

// WebGPU implementation of a vertex, index, or storage buffer.
//
// It holds a GPU buffer and a CPU copy, which Get() and editing read, and which
// keeps the bytes around a write that isn't 4-byte aligned, as WebGPU writes
// buffers in whole 4-byte words. A kStaticWrite buffer writes each change to
// the GPU at once. Any other marks the bytes it changes, and writes them as the
// next frame ends.
//
// This class is thread-compatible.
class WebGpuRenderBuffer final : public RenderBuffer,
                                 public WebGpuPendingWrite {
 public:
  // Creates a buffer of `capacity` values of `value_size` bytes, for `usage`
  // (to which CopyDst is added). Returns null, having logged why, if it can't
  // be created. `backend` must outlive it.
  static std::unique_ptr<WebGpuRenderBuffer> Create(WebGpuBackend* backend,
                                                    wgpu::BufferUsage usage,
                                                    DataVolatility volatility,
                                                    int value_size,
                                                    int capacity);

  ~WebGpuRenderBuffer() override;

  const wgpu::Buffer& GetBuffer() const { return buffer_; }

  void WritePending(const wgpu::Queue& queue) override;

 protected:
  bool DoClear(int offset, int size) override;
  bool DoSet(int offset, const void* data, int size) override;
  bool DoGet(int offset, void* data, int size) const override;
  void* DoEditBegin() override;
  void OnEditEnd(bool modified) override;

 private:
  WebGpuRenderBuffer(WebGpuBackend* backend, DataVolatility volatility,
                     int value_size, int capacity, wgpu::Buffer buffer,
                     int64_t gpu_bytes);

  // Marks bytes [begin, end) of the CPU copy as changed, writing them at once
  // for kStaticWrite.
  void MarkPending(int64_t begin, int64_t end);

  // Writes the changed bytes, widened to whole 4-byte words, to the GPU.
  void WriteRange(const wgpu::Queue& queue, int64_t begin, int64_t end);

  WebGpuBackend* const backend_;
  wgpu::Buffer buffer_;
  const int64_t gpu_bytes_;

  // The CPU copy, as large as the GPU buffer, and the bytes of it changed
  // since the GPU was last written (none if begin is not below end).
  std::vector<uint8_t> data_;
  int64_t pending_begin_ = 0;
  int64_t pending_end_ = 0;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_RENDER_BUFFER_H_
