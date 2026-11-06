// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_MESH_POOL_BUFFERS_H_
#define GB_RENDER_VULKAN_VULKAN_MESH_POOL_BUFFERS_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "concurrentqueue.h"
#include "gb/render/mesh_pool_buffers.h"
#include "gb/render/render_types.h"
#include "gb/render/vulkan/vulkan_buffer.h"
#include "gb/render/vulkan/vulkan_render_state.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of MeshPoolBuffers.
//
// The vertices and indices are in device-local buffers. A write is copied into
// a host-visible staging block on the writer's thread, and queued. When a
// frame ends, the backend calls OnRender(), which records the copies from the
// staging blocks, and any growth, before the frame is drawn.
//
// The MeshPoolBuffers functions are thread-safe. The others are only called
// by the backend, while it ends a frame.
class VulkanMeshPoolBuffers final : public MeshPoolBuffers {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Returns null if the buffers could not be created. `backend` must outlive
  // this.
  static std::unique_ptr<VulkanMeshPoolBuffers> Create(
      VulkanInternal, VulkanBackend* backend, int vertex_size,
      IndexType index_type, int vertex_capacity, int index_capacity);
  ~VulkanMeshPoolBuffers() override;

  //----------------------------------------------------------------------------
  // Rendering
  //----------------------------------------------------------------------------

  // The buffers to draw from, after OnRender() has been called for the frame.
  vk::Buffer GetVertexBuffer() const { return vertex_buffer_->Get(); }
  vk::Buffer GetIndexBuffer() const { return index_buffer_->Get(); }
  vk::IndexType GetIndexType() const { return index_type_; }

  // Records growth into `commands`, and adds the copies queued since the last
  // call to `state`, for the backend to record before drawing.
  void OnRender(vk::CommandBuffer commands, VulkanRenderState* state);

  //----------------------------------------------------------------------------
  // MeshPoolBuffers overrides
  //----------------------------------------------------------------------------

  bool WriteMesh(int vertex_offset, const void* vertices, int vertex_count,
                 int index_offset, const void* indices,
                 int index_count) override;
  bool Grow(int vertex_capacity, int index_capacity) override;
  uint64_t GetReuseTicket() const override;
  bool CanReuse(uint64_t ticket) const override;

 private:
  // The size of a staging block. A bigger write gets a block of its own.
  static constexpr int64_t kStagingBlockSize = 4 * 1024 * 1024;

  // A write waiting to be copied. The staging block holds its vertices, then
  // its indices, from `staging_offset`. Offsets and sizes are in bytes.
  struct Copy {
    std::shared_ptr<VulkanBuffer> staging_block;
    int64_t staging_offset;
    int64_t vertex_offset;
    int64_t vertex_size;
    int64_t index_offset;
    int64_t index_size;
  };

  VulkanMeshPoolBuffers(VulkanBackend* backend, int vertex_size,
                        IndexType index_type, int vertex_capacity,
                        int index_capacity);

  // Creates a vertex or index buffer, by `usage`, of `size` bytes.
  std::unique_ptr<VulkanBuffer> CreateDeviceBuffer(vk::BufferUsageFlags usage,
                                                   int64_t size);
  std::shared_ptr<VulkanBuffer> CreateStagingBlock(int64_t size);

  // Reserves `size` bytes in a staging block, setting the block and offset in
  // `copy`.
  bool ReserveStaging(int64_t size, Copy* copy);

  // Reserves `size` bytes in the current staging block, if it has room.
  bool TakeStaging(int64_t size, Copy* copy)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(staging_mutex_);

  // Replaces the buffers with any that Grow() created, copying the contents.
  void ApplyGrowth(vk::CommandBuffer commands);

  VulkanBackend* const backend_;
  const int vertex_size_;
  const int index_size_;
  const vk::IndexType index_type_;

  // Only the backend uses these, while it ends a frame. The dequeued copies
  // are kept between frames only to keep their capacity.
  std::unique_ptr<VulkanBuffer> vertex_buffer_;
  std::unique_ptr<VulkanBuffer> index_buffer_;
  std::vector<Copy> dequeued_copies_;

  // Writes waiting to be copied.
  moodycamel::ConcurrentQueue<Copy> copies_;

  // The block writes are copied into, and the bytes of it already used.
  absl::Mutex staging_mutex_;
  std::shared_ptr<VulkanBuffer> staging_block_ ABSL_GUARDED_BY(staging_mutex_);
  int64_t staging_used_ ABSL_GUARDED_BY(staging_mutex_) = 0;

  // The capacities of the last call to Grow(), and the buffers it created that
  // OnRender() hasn't applied yet, or null.
  absl::Mutex growth_mutex_;
  int vertex_capacity_ ABSL_GUARDED_BY(growth_mutex_);
  int index_capacity_ ABSL_GUARDED_BY(growth_mutex_);
  std::unique_ptr<VulkanBuffer> grown_vertex_buffer_
      ABSL_GUARDED_BY(growth_mutex_);
  std::unique_ptr<VulkanBuffer> grown_index_buffer_
      ABSL_GUARDED_BY(growth_mutex_);
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_MESH_POOL_BUFFERS_H_
