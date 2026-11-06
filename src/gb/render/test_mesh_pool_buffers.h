// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_MESH_POOL_BUFFERS_H_
#define GB_RENDER_TEST_MESH_POOL_BUFFERS_H_

#include <cstdint>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "gb/render/mesh_pool_buffers.h"
#include "gb/render/render_types.h"

namespace gb {

class TestRenderBackend;

// Implementation of MeshPoolBuffers for use in tests.
//
// Writes are queued, and copied in order when the backend's frame ends, so
// tests see what a frame would draw. Growth is immediate. Unwritten bytes are
// 0xFF.
class TestMeshPoolBuffers final : public MeshPoolBuffers {
 public:
  struct Config {
    bool fail_write = false;
    bool fail_grow = false;

    // CanReuse() is true once the frame a ticket was taken in, and this many
    // frames after it, have ended. The Vulkan backend's equivalent is two: one
    // for its second frame in flight, and one for writes that are copied a
    // frame late.
    int frames_before_reuse = 2;
  };

  // `backend` copies the queued writes as each frame ends, and must outlive
  // this.
  TestMeshPoolBuffers(Config* config, TestRenderBackend* backend,
                      int vertex_size, IndexType index_type,
                      int vertex_capacity, int index_capacity);
  ~TestMeshPoolBuffers() override;

  int GetVertexSize() const { return vertex_size_; }
  IndexType GetIndexType() const { return index_type_; }
  int GetVertexCapacity() const;
  int GetIndexCapacity() const;
  int GetInvalidCallCount() const;

  // Returns `count` vertices or indices from `offset` as of the last frame
  // ended, which must be within the capacity and of the buffer's types.
  template <typename Vertex>
  std::vector<Vertex> GetVertices(int offset, int count) const;
  template <typename Index>
  std::vector<Index> GetIndices(int offset, int count) const;

  // Copies the writes queued since the last call. Called by TestRenderBackend
  // when its frame ends.
  void CopyWrites();

  // Implementation of MeshPoolBuffers.
  bool WriteMesh(int vertex_offset, const void* vertices, int vertex_count,
                 int index_offset, const void* indices,
                 int index_count) override;
  bool Grow(int vertex_capacity, int index_capacity) override;
  uint64_t GetReuseTicket() const override;
  bool CanReuse(uint64_t ticket) const override;

 private:
  // A queued write of `bytes` from byte `offset` in the vertices or indices.
  struct Write {
    bool is_vertices;
    int64_t offset;
    std::vector<uint8_t> bytes;
  };

  // Copies `count` values of `value_size` bytes from `offset` in `bytes` to
  // `out`.
  static void CopyBytes(const std::vector<uint8_t>& bytes, int value_size,
                        int offset, int count, void* out);

  // Returns true if `count` values from `offset` are within `bytes`.
  static bool IsInRange(const std::vector<uint8_t>& bytes, int value_size,
                        int offset, int count);

  Config* const config_;
  TestRenderBackend* const backend_;
  const int vertex_size_;
  const IndexType index_type_;
  const int index_size_;

  mutable absl::Mutex mutex_;
  std::vector<uint8_t> vertices_ ABSL_GUARDED_BY(mutex_);
  std::vector<uint8_t> indices_ ABSL_GUARDED_BY(mutex_);
  std::vector<Write> writes_ ABSL_GUARDED_BY(mutex_);
  int invalid_call_count_ ABSL_GUARDED_BY(mutex_) = 0;
};

template <typename Vertex>
std::vector<Vertex> TestMeshPoolBuffers::GetVertices(int offset,
                                                     int count) const {
  CHECK_EQ(static_cast<int>(sizeof(Vertex)), vertex_size_);
  std::vector<Vertex> vertices(count);
  absl::MutexLock lock(&mutex_);
  CopyBytes(vertices_, vertex_size_, offset, count, vertices.data());
  return vertices;
}

template <typename Index>
std::vector<Index> TestMeshPoolBuffers::GetIndices(int offset,
                                                   int count) const {
  CHECK_EQ(static_cast<int>(sizeof(Index)), index_size_);
  std::vector<Index> indices(count);
  absl::MutexLock lock(&mutex_);
  CopyBytes(indices_, index_size_, offset, count, indices.data());
  return indices;
}

}  // namespace gb

#endif  // GB_RENDER_TEST_MESH_POOL_BUFFERS_H_
