// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_buffer.h"

#include "absl/memory/memory.h"
#include "gb/render/vulkan/vulkan_backend.h"

namespace gb {

std::unique_ptr<VulkanBuffer> VulkanBuffer::Create(
    VulkanBackend* backend, const vk::BufferCreateInfo& create_info,
    VmaMemoryUsage memory_usage) {
  auto buffer = absl::WrapUnique(new VulkanBuffer);
  if (!buffer->Init(backend, create_info, memory_usage)) {
    return nullptr;
  }
  return buffer;
}

std::unique_ptr<VulkanBuffer> VulkanBuffer::Create(
    VulkanBackend* backend, vk::BufferUsageFlags usage, vk::DeviceSize size,
    VmaMemoryUsage memory_usage) {
  return Create(backend, vk::BufferCreateInfo({}, size, usage), memory_usage);
}

VulkanBuffer::VulkanBuffer() {}

VulkanBuffer::~VulkanBuffer() {
  auto device = backend_->GetDevice();
  UnmapData();
  auto* gc = backend_->GetGarbageCollector();
  gc->Dispose(buffer_, allocation_);
}

bool VulkanBuffer::Init(VulkanBackend* backend,
                        const vk::BufferCreateInfo& create_info,
                        VmaMemoryUsage memory_usage) {
  backend_ = backend;
  memory_usage_ = memory_usage;

  auto device = backend_->GetDevice();

  VmaAllocationCreateInfo alloc_info = {};
  alloc_info.usage = memory_usage;
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocationInfo allocation_info = {};
  if (vmaCreateBuffer(backend_->GetAllocator(),
                      &(VkBufferCreateInfo&)create_info, &alloc_info, &buffer,
                      &allocation_, &allocation_info) != VK_SUCCESS) {
    return false;
  }
  size_ = create_info.size;
  buffer_ = buffer;

  if (memory_usage == VMA_MEMORY_USAGE_CPU_ONLY) {
    if (vmaMapMemory(backend_->GetAllocator(), allocation_, &data_) !=
        VK_SUCCESS) {
      return false;
    }
  }
  return true;
}

bool VulkanBuffer::MapData() {
  if (data_ != nullptr) {
    return true;
  }
  if (memory_usage_ == VMA_MEMORY_USAGE_GPU_ONLY) {
    return false;
  }
  if (vmaMapMemory(backend_->GetAllocator(), allocation_, &data_) !=
      VK_SUCCESS) {
    return false;
  }
  if (memory_usage_ != VMA_MEMORY_USAGE_CPU_ONLY &&
      vmaInvalidateAllocation(backend_->GetAllocator(), allocation_, 0,
                              VK_WHOLE_SIZE) != VK_SUCCESS) {
    vmaUnmapMemory(backend_->GetAllocator(), allocation_);
    return false;
  }
  return true;
}

void VulkanBuffer::UnmapData() {
  if (data_ == nullptr) {
    return;
  }
  if (memory_usage_ != VMA_MEMORY_USAGE_CPU_ONLY) {
    vmaFlushAllocation(backend_->GetAllocator(), allocation_, 0, VK_WHOLE_SIZE);
  }
  vmaUnmapMemory(backend_->GetAllocator(), allocation_);
  data_ = nullptr;
}

}  // namespace gb
