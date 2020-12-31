// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_RENDER_BUFFER_H_
#define GB_RENDER_VULKAN_VULKAN_RENDER_BUFFER_H_

#include "gb/render/render_buffer.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

//==============================================================================
// VulkanRenderBuffer
//==============================================================================

// Vulkan base-class implementation of RenderBuffer
//
// This class is thread-compatible, except as noted.
class VulkanRenderBuffer : public RenderBuffer {
 public:
  struct BufferHandle {
    int version;
    vk::Buffer buffer;
  };

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new VulkanRenderBuffer of the requested type.
  //
  // This function is thread-safe.
  static std::unique_ptr<VulkanRenderBuffer> Create(
      VulkanInternal, VulkanBackend* backend, VulkanBufferType type,
      DataVolatility volatility, int value_size, int capacity,
      int align_size = 0);
  ~VulkanRenderBuffer() override;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  VulkanBufferType GetType() const { return type_; }
  int GetAlignSize() const { return align_size_; }
  const BufferHandle& GetBufferHandle(int frame) const {
    return buffer_handles_[frame % kMaxFramesInFlight];
  }
  vk::Buffer GetBuffer(int frame_index) const {
    return buffer_handles_[frame_index].buffer;
  }

  //----------------------------------------------------------------------------
  // Constant buffer operations
  //----------------------------------------------------------------------------

  void GetValue(int offset, void* data);
  void SetValue(int offset, const void* data);

  //----------------------------------------------------------------------------
  // Events
  //----------------------------------------------------------------------------

  void OnRender(VulkanRenderState* state) { DoRender(state); }

 protected:
  VulkanRenderBuffer(VulkanBackend* backend, VulkanBufferType type,
                     DataVolatility volatility, int value_size, int capacity,
                     int align_size)
      : RenderBuffer(volatility, value_size, capacity),
        backend_(backend),
        type_(type),
        align_size_(align_size != 0 ? align_size : value_size) {}

  // Properties.
  bool FrameInUse(int frame) const { return frame > backend_->GetFrame() - 2; }

  // Buffer operations.
  std::unique_ptr<VulkanBuffer> CreateDeviceBuffer(
      VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_GPU_ONLY);
  std::unique_ptr<VulkanBuffer> CreateHostBuffer();
  void CopyBuffer(vk::Buffer host_buffer, vk::Buffer device_buffer,
                  VulkanRenderState* state);
  void SetAllBufferHandles(vk::Buffer buffer) {
    for (BufferHandle& handle : buffer_handles_) {
      handle.version += 1;
      handle.buffer = buffer;
    }
  }
  void SetBufferHandle(int index, vk::Buffer buffer) {
    buffer_handles_[index].version += 1;
    buffer_handles_[index].buffer = buffer;
  }

  // Derived class implementation.
  virtual void DoRender(VulkanRenderState* state) = 0;
  virtual const void* DoGetData() const = 0;

 private:
  VulkanBackend* const backend_;
  const VulkanBufferType type_;
  const int align_size_;
  BufferHandle buffer_handles_[kMaxFramesInFlight] = {};
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_RENDER_BUFFER_H_
