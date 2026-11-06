// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_mesh_pool_buffers.h"

#include <cstring>
#include <iterator>
#include <utility>

#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "gb/render/vulkan/vulkan_backend.h"

namespace gb {

std::unique_ptr<VulkanMeshPoolBuffers> VulkanMeshPoolBuffers::Create(
    VulkanInternal, VulkanBackend* backend, int vertex_size,
    IndexType index_type, int vertex_capacity, int index_capacity) {
  auto buffers = absl::WrapUnique(new VulkanMeshPoolBuffers(
      backend, vertex_size, index_type, vertex_capacity, index_capacity));
  buffers->vertex_buffer_ =
      buffers->CreateDeviceBuffer(vk::BufferUsageFlagBits::eVertexBuffer,
                                  int64_t{vertex_capacity} * vertex_size);
  buffers->index_buffer_ = buffers->CreateDeviceBuffer(
      vk::BufferUsageFlagBits::eIndexBuffer,
      int64_t{index_capacity} * GetIndexSize(index_type));
  if (buffers->vertex_buffer_ == nullptr || buffers->index_buffer_ == nullptr) {
    LOG(ERROR) << "Could not create mesh pool buffers";
    return nullptr;
  }
  backend->RegisterMeshPool({}, buffers.get());
  return buffers;
}

VulkanMeshPoolBuffers::VulkanMeshPoolBuffers(VulkanBackend* backend,
                                             int vertex_size,
                                             IndexType index_type,
                                             int vertex_capacity,
                                             int index_capacity)
    : backend_(backend),
      vertex_size_(vertex_size),
      index_size_(GetIndexSize(index_type)),
      index_type_(index_type == IndexType::kUint16 ? vk::IndexType::eUint16
                                                   : vk::IndexType::eUint32),
      vertex_capacity_(vertex_capacity),
      index_capacity_(index_capacity) {}

VulkanMeshPoolBuffers::~VulkanMeshPoolBuffers() {
  backend_->UnregisterMeshPool({}, this);
}

std::unique_ptr<VulkanBuffer> VulkanMeshPoolBuffers::CreateDeviceBuffer(
    vk::BufferUsageFlags usage, int64_t size) {
  // Growth copies from the old buffer to the new one.
  return VulkanBuffer::Create(backend_,
                              usage | vk::BufferUsageFlagBits::eTransferDst |
                                  vk::BufferUsageFlagBits::eTransferSrc,
                              size);
}

bool VulkanMeshPoolBuffers::WriteMesh(int vertex_offset, const void* vertices,
                                      int vertex_count, int index_offset,
                                      const void* indices, int index_count) {
  Copy copy;
  copy.vertex_offset = int64_t{vertex_offset} * vertex_size_;
  copy.vertex_size = int64_t{vertex_count} * vertex_size_;
  copy.index_offset = int64_t{index_offset} * index_size_;
  copy.index_size = int64_t{index_count} * index_size_;
  if (!ReserveStaging(copy.vertex_size + copy.index_size, &copy)) {
    return false;
  }

  // The reserved bytes are this write's alone, so they are filled without a
  // lock.
  uint8_t* data = static_cast<uint8_t*>(copy.staging_block->GetData()) +
                  copy.staging_offset;
  std::memcpy(data, vertices, copy.vertex_size);
  std::memcpy(data + copy.vertex_size, indices, copy.index_size);
  return copies_.enqueue(std::move(copy));
}

bool VulkanMeshPoolBuffers::ReserveStaging(int64_t size, Copy* copy) {
  // A write bigger than a block gets a block of its own.
  if (size > kStagingBlockSize) {
    copy->staging_block = CreateStagingBlock(size);
    copy->staging_offset = 0;
    return copy->staging_block != nullptr;
  }

  {
    absl::MutexLock lock(&staging_mutex_);
    if (TakeStaging(size, copy)) {
      return true;
    }
  }

  // The new block is created without the lock, so other writers aren't held
  // up. If another writer replaced the block meanwhile, its block is used if
  // it has room, and this one is dropped.
  std::shared_ptr<VulkanBuffer> block = CreateStagingBlock(kStagingBlockSize);
  if (block == nullptr) {
    return false;
  }
  absl::MutexLock lock(&staging_mutex_);
  if (TakeStaging(size, copy)) {
    return true;
  }

  // The old block is freed once the copies from it are recorded.
  staging_block_ = std::move(block);
  staging_used_ = 0;
  return TakeStaging(size, copy);
}

bool VulkanMeshPoolBuffers::TakeStaging(int64_t size, Copy* copy) {
  if (staging_block_ == nullptr ||
      staging_used_ + size > static_cast<int64_t>(staging_block_->GetSize())) {
    return false;
  }
  copy->staging_block = staging_block_;
  copy->staging_offset = staging_used_;
  staging_used_ += size;
  return true;
}

std::shared_ptr<VulkanBuffer> VulkanMeshPoolBuffers::CreateStagingBlock(
    int64_t size) {
  std::shared_ptr<VulkanBuffer> block =
      VulkanBuffer::Create(backend_, vk::BufferUsageFlagBits::eTransferSrc,
                           size, VMA_MEMORY_USAGE_CPU_ONLY);
  if (block == nullptr) {
    LOG(ERROR) << "Could not create a mesh pool staging block";
  }
  return block;
}

bool VulkanMeshPoolBuffers::Grow(int vertex_capacity, int index_capacity) {
  // The buffers are created without the lock, so a frame that ends meanwhile
  // isn't held up. The pool never grows from two threads at once, but if it
  // did, the bigger buffers are kept.
  int old_vertex_capacity;
  int old_index_capacity;
  {
    absl::MutexLock lock(&growth_mutex_);
    old_vertex_capacity = vertex_capacity_;
    old_index_capacity = index_capacity_;
  }
  std::unique_ptr<VulkanBuffer> vertex_buffer;
  if (vertex_capacity > old_vertex_capacity) {
    vertex_buffer = CreateDeviceBuffer(vk::BufferUsageFlagBits::eVertexBuffer,
                                       int64_t{vertex_capacity} * vertex_size_);
    if (vertex_buffer == nullptr) {
      return false;
    }
  }
  std::unique_ptr<VulkanBuffer> index_buffer;
  if (index_capacity > old_index_capacity) {
    index_buffer = CreateDeviceBuffer(vk::BufferUsageFlagBits::eIndexBuffer,
                                      int64_t{index_capacity} * index_size_);
    if (index_buffer == nullptr) {
      return false;
    }
  }

  // Buffers from an earlier call that OnRender() hasn't applied are replaced,
  // as these are bigger.
  absl::MutexLock lock(&growth_mutex_);
  if (vertex_buffer != nullptr && vertex_capacity > vertex_capacity_) {
    grown_vertex_buffer_ = std::move(vertex_buffer);
    vertex_capacity_ = vertex_capacity;
  }
  if (index_buffer != nullptr && index_capacity > index_capacity_) {
    grown_index_buffer_ = std::move(index_buffer);
    index_capacity_ = index_capacity;
  }
  return true;
}

uint64_t VulkanMeshPoolBuffers::GetReuseTicket() const {
  return static_cast<uint64_t>(backend_->GetFrame());
}

bool VulkanMeshPoolBuffers::CanReuse(uint64_t ticket) const {
  // A write queued in the ticket's frame may be copied in the next frame, so
  // that frame must be finished too. Copies are only recorded once the frame
  // two before has finished on the GPU, so nothing in flight then reads the
  // space.
  return !backend_->IsFrameInUse(static_cast<int>(ticket) + 1);
}

void VulkanMeshPoolBuffers::OnRender(vk::CommandBuffer commands,
                                     VulkanRenderState* state) {
  // The copies are taken before growth is applied, so any growth a copy
  // writes into is applied too. Only the copies queued before this started
  // are taken, so writers that keep queueing can't hold up the frame.
  int64_t remaining = static_cast<int64_t>(copies_.size_approx());
  while (remaining > 0) {
    const int64_t count = static_cast<int64_t>(copies_.try_dequeue_bulk(
        std::back_inserter(dequeued_copies_), static_cast<size_t>(remaining)));
    if (count == 0) {
      break;
    }
    remaining -= count;
  }
  ApplyGrowth(commands);

  // All vertices, then all indices, so copies from one staging block to one
  // buffer are adjacent, and the backend records them together.
  for (const Copy& vertex_copy : dequeued_copies_) {
    if (vertex_copy.vertex_size > 0) {
      state->buffer_updates.emplace_back(
          vertex_copy.staging_block->Get(), vertex_copy.staging_offset,
          vertex_buffer_->Get(), vertex_copy.vertex_offset,
          vertex_copy.vertex_size, vk::AccessFlagBits::eVertexAttributeRead);
    }
  }
  for (const Copy& index_copy : dequeued_copies_) {
    if (index_copy.index_size > 0) {
      state->buffer_updates.emplace_back(
          index_copy.staging_block->Get(),
          index_copy.staging_offset + index_copy.vertex_size,
          index_buffer_->Get(), index_copy.index_offset, index_copy.index_size,
          vk::AccessFlagBits::eIndexRead);
    }
  }

  // Staging blocks freed here are kept by the garbage collector until the
  // frame has finished copying from them.
  dequeued_copies_.clear();
}

void VulkanMeshPoolBuffers::ApplyGrowth(vk::CommandBuffer commands) {
  std::unique_ptr<VulkanBuffer> vertex_buffer;
  std::unique_ptr<VulkanBuffer> index_buffer;
  {
    absl::MutexLock lock(&growth_mutex_);
    vertex_buffer = std::move(grown_vertex_buffer_);
    index_buffer = std::move(grown_index_buffer_);
  }
  if (vertex_buffer == nullptr && index_buffer == nullptr) {
    return;
  }

  // Copies into the old buffers by frames still in flight finish before the
  // old buffers are read.
  commands.pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eTransfer, {},
      {vk::MemoryBarrier(vk::AccessFlagBits::eTransferWrite,
                         vk::AccessFlagBits::eTransferRead)},
      {}, {});

  // The old buffers are freed by the garbage collector, once no frame in
  // flight draws from them.
  if (vertex_buffer != nullptr) {
    commands.copyBuffer(vertex_buffer_->Get(), vertex_buffer->Get(),
                        {vk::BufferCopy(0, 0, vertex_buffer_->GetSize())});
    vertex_buffer_ = std::move(vertex_buffer);
  }
  if (index_buffer != nullptr) {
    commands.copyBuffer(index_buffer_->Get(), index_buffer->Get(),
                        {vk::BufferCopy(0, 0, index_buffer_->GetSize())});
    index_buffer_ = std::move(index_buffer);
  }

  // The old contents are in place before this frame's copies overwrite them,
  // or it draws them.
  commands.pipelineBarrier(
      vk::PipelineStageFlagBits::eTransfer,
      vk::PipelineStageFlagBits::eTransfer |
          vk::PipelineStageFlagBits::eVertexInput,
      {},
      {vk::MemoryBarrier(vk::AccessFlagBits::eTransferWrite,
                         vk::AccessFlagBits::eTransferWrite |
                             vk::AccessFlagBits::eVertexAttributeRead |
                             vk::AccessFlagBits::eIndexRead)},
      {}, {});
}

}  // namespace gb
