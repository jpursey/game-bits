// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/test_mesh_pool_buffers.h"

#include <cstring>

#include "gb/render/test_render_backend.h"

namespace gb {

TestMeshPoolBuffers::TestMeshPoolBuffers(Config* config,
                                         TestRenderBackend* backend,
                                         int vertex_size, IndexType index_type,
                                         int vertex_capacity,
                                         int index_capacity)
    : config_(config),
      backend_(backend),
      vertex_size_(vertex_size),
      index_type_(index_type),
      index_size_(GetIndexSize(index_type)),
      vertices_(int64_t{vertex_capacity} * vertex_size, 0xFF),
      indices_(int64_t{index_capacity} * index_size_, 0xFF) {
  backend_->AddMeshPoolBuffers(this);
}

TestMeshPoolBuffers::~TestMeshPoolBuffers() {
  backend_->RemoveMeshPoolBuffers(this);
}

int TestMeshPoolBuffers::GetVertexCapacity() const {
  absl::MutexLock lock(&mutex_);
  return static_cast<int>(vertices_.size() / vertex_size_);
}

int TestMeshPoolBuffers::GetIndexCapacity() const {
  absl::MutexLock lock(&mutex_);
  return static_cast<int>(indices_.size() / index_size_);
}

int TestMeshPoolBuffers::GetInvalidCallCount() const {
  absl::MutexLock lock(&mutex_);
  return invalid_call_count_;
}

void TestMeshPoolBuffers::CopyWrites() {
  absl::MutexLock lock(&mutex_);
  for (const Write& write : writes_) {
    std::vector<uint8_t>& bytes = write.is_vertices ? vertices_ : indices_;
    std::memcpy(bytes.data() + write.offset, write.bytes.data(),
                write.bytes.size());
  }
  writes_.clear();
}

bool TestMeshPoolBuffers::WriteMesh(int vertex_offset, const void* vertices,
                                    int vertex_count, int index_offset,
                                    const void* indices, int index_count) {
  absl::MutexLock lock(&mutex_);
  if (config_->fail_write) {
    return false;
  }
  if (vertices == nullptr || indices == nullptr ||
      !IsInRange(vertices_, vertex_size_, vertex_offset, vertex_count) ||
      !IsInRange(indices_, index_size_, index_offset, index_count)) {
    invalid_call_count_ += 1;
    return false;
  }
  const uint8_t* vertex_bytes = static_cast<const uint8_t*>(vertices);
  const uint8_t* index_bytes = static_cast<const uint8_t*>(indices);
  writes_.push_back(
      {true, int64_t{vertex_offset} * vertex_size_,
       std::vector<uint8_t>(
           vertex_bytes, vertex_bytes + int64_t{vertex_count} * vertex_size_)});
  writes_.push_back(
      {false, int64_t{index_offset} * index_size_,
       std::vector<uint8_t>(index_bytes,
                            index_bytes + int64_t{index_count} * index_size_)});
  return true;
}

bool TestMeshPoolBuffers::Grow(int vertex_capacity, int index_capacity) {
  absl::MutexLock lock(&mutex_);
  if (config_->fail_grow) {
    return false;
  }
  const int64_t vertex_bytes = int64_t{vertex_capacity} * vertex_size_;
  const int64_t index_bytes = int64_t{index_capacity} * index_size_;
  if (vertex_bytes < static_cast<int64_t>(vertices_.size()) ||
      index_bytes < static_cast<int64_t>(indices_.size())) {
    invalid_call_count_ += 1;
    return false;
  }
  vertices_.resize(vertex_bytes, 0xFF);
  indices_.resize(index_bytes, 0xFF);
  return true;
}

uint64_t TestMeshPoolBuffers::GetReuseTicket() const {
  return static_cast<uint64_t>(backend_->GetFrame());
}

bool TestMeshPoolBuffers::CanReuse(uint64_t ticket) const {
  return static_cast<uint64_t>(backend_->GetFrame()) >
         ticket + static_cast<uint64_t>(config_->frames_before_reuse);
}

void TestMeshPoolBuffers::CopyBytes(const std::vector<uint8_t>& bytes,
                                    int value_size, int offset, int count,
                                    void* out) {
  CHECK(IsInRange(bytes, value_size, offset, count));
  std::memcpy(out, bytes.data() + int64_t{offset} * value_size,
              int64_t{count} * value_size);
}

bool TestMeshPoolBuffers::IsInRange(const std::vector<uint8_t>& bytes,
                                    int value_size, int offset, int count) {
  return offset >= 0 && count >= 0 &&
         (int64_t{offset} + count) * value_size <=
             static_cast<int64_t>(bytes.size());
}

}  // namespace gb
