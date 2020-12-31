// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_BUFFER_H_
#define GB_RENDER_VULKAN_VULKAN_BUFFER_H_

#include <memory>
#include <utility>

#include "gb/render/vulkan/vulkan_allocator.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// This class wraps a vk::Buffer and its associated memory allocation.
//
// This class is thread-compatible, except as noted.
class VulkanBuffer final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // The Create functions are thread-safe.
  static std::unique_ptr<VulkanBuffer> Create(
      VulkanBackend* backend, const vk::BufferCreateInfo& create_info,
      VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_GPU_ONLY);
  static std::unique_ptr<VulkanBuffer> Create(
      VulkanBackend* backend, vk::BufferUsageFlags usage, vk::DeviceSize size,
      VmaMemoryUsage memory_usage = VMA_MEMORY_USAGE_GPU_ONLY);

  VulkanBuffer(const VulkanBuffer&) = delete;
  VulkanBuffer(VulkanBuffer&&) = delete;
  VulkanBuffer& operator=(const VulkanBuffer&) = delete;
  VulkanBuffer& operator=(VulkanBuffer&&) = delete;
  ~VulkanBuffer();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // These functions are thread-safe.
  vk::Buffer Get() const { return buffer_; }
  vk::DeviceSize GetSize() const { return size_; }
  VmaMemoryUsage GetMemoryUsage() const { return memory_usage_; }

  //----------------------------------------------------------------------------
  // Data access
  //----------------------------------------------------------------------------

  // Returns a pointer to the CPU accessible data for the buffer.
  //
  // When and if this is available is dependent on the requested memory usage as
  // follows:
  //  - VMA_MEMORY_USAGE_GPU_ONLY: This always returns null.
  //  - VMA_MEMORY_USAGE_CPU_ONLY: This always returns a pointer to the memory.
  //    Any changes are also immediately visible to the GPU.
  //  - VMA_MEMORY_USAGE_CPU_TO_GPU and VMA_MEMORY_USAGE_GPU_TO_CPU: This
  //    returns a pointer iff it is mapped (see MapData/UnmapData). These types
  //    of memory usage are not guaranteed to be visible to the GPU while
  //    mapped.
  void* GetData() { return data_; }
  const void* GetData() const { return data_; }

  // Maps the data so it is accessible to the CPU.
  //
  // If mapping fails, or it is not allowed (usage is
  // VMA_MEMORY_USAGE_GPU_ONLY), this will return false and GetData() will
  // continue to return false. If the memory is already mapped, this trivially
  // succeeds.
  bool MapData();

  // Unmaps the data so it is no longer visible to the CPU.
  //
  // This does nothing if the memory usage is VMA_MEMORY_USAGE_CPU_ONLY, or if
  // the data is already unmapped.
  void UnmapData();

 private:
  VulkanBuffer();

  bool Init(VulkanBackend* backend, const vk::BufferCreateInfo& create_info,
            VmaMemoryUsage memory_usage);

  VulkanBackend* backend_;
  VmaMemoryUsage memory_usage_ = VMA_MEMORY_USAGE_UNKNOWN;
  vk::Buffer buffer_;
  VmaAllocation allocation_;
  vk::DeviceSize size_ = 0;
  void* data_ = nullptr;
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_BUFFER_H_
