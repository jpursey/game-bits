// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_BINDING_DATA_H_
#define GB_RENDER_WEBGPU_WEBGPU_BINDING_DATA_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "gb/render/binding.h"
#include "gb/render/binding_data.h"
#include "gb/render/render_types.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

//==============================================================================
// WebGpuBindingDataFactory
//==============================================================================

class WebGpuBindingData;

// Makes the binding data of one binding set: its bind group layout, and the
// uniform buffers its constants are in. Each binding data takes a slot in one
// of them, which holds all of its constants bindings, each at an offset of its
// own. A factory is shared by what makes its binding data (a binding layout or
// a pipeline) and each binding data, so either may outlive the other.
//
// This class is thread-safe.
class WebGpuBindingDataFactory final
    : public std::enable_shared_from_this<WebGpuBindingDataFactory> {
 public:
  // One binding index.
  struct Binding {
    BindingType binding_type = BindingType::kNone;
    const RenderDataType* data_type = nullptr;

    // Where a constants binding's values are in a binding data's slot, and
    // their size as bound.
    int64_t offset = 0;
    int64_t size = 0;
  };

  // Creates the factory for the bindings in `bindings` that are in `set`.
  // Returns null, having logged why, if a binding can't be bound yet (only
  // constants can), or its index is beyond kMaxWebGpuBindingIndex.
  //
  // `backend` must outlive this.
  static std::shared_ptr<WebGpuBindingDataFactory> Create(
      WebGpuBackend* backend, BindingSet set,
      absl::Span<const gb::Binding> bindings);

  WebGpuBindingDataFactory(const WebGpuBindingDataFactory&) = delete;
  WebGpuBindingDataFactory& operator=(const WebGpuBindingDataFactory&) =
      delete;
  ~WebGpuBindingDataFactory();

  BindingSet GetSet() const { return set_; }
  const wgpu::BindGroupLayout& GetBindGroupLayout() const { return layout_; }

  // By binding index, kNone where the set has no binding.
  absl::Span<const Binding> GetBindings() const { return bindings_; }

  // The size of a binding data's slot, or 0 if the set has no constants.
  int64_t GetSlotSize() const { return slot_size_; }

  // Creates binding data for `pipeline` (null for a view's). Returns null,
  // having logged why, if its buffer or bind group can't be created.
  std::unique_ptr<WebGpuBindingData> NewBindingData(RenderPipeline* pipeline);

 private:
  friend class WebGpuBindingData;

  // A slot in one of the uniform buffers.
  struct Slot {
    int buffer_index = 0;
    int slot_index = 0;
  };

  WebGpuBindingDataFactory(WebGpuBackend* backend, BindingSet set,
                           std::vector<Binding> bindings, int64_t slot_size,
                           wgpu::BindGroupLayout layout);

  // Returns a free slot, adding a buffer if there is none, or null if the
  // buffer can't be created.
  std::optional<Slot> AllocateSlot() ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the slot to be reused. Its binding data must no longer be drawn.
  void FreeSlot(const Slot& slot) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the buffer a slot is in.
  wgpu::Buffer GetBuffer(const Slot& slot) const ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns the slot's offset in its buffer.
  int64_t GetSlotOffset(const Slot& slot) const {
    return slot.slot_index * slot_size_;
  }

  WebGpuBackend* const backend_;
  const BindingSet set_;
  const std::vector<Binding> bindings_;
  const int64_t slot_size_;
  const int slots_per_buffer_;
  const wgpu::BindGroupLayout layout_;

  mutable absl::Mutex mutex_;
  std::vector<wgpu::Buffer> buffers_ ABSL_GUARDED_BY(mutex_);
  std::vector<Slot> free_slots_ ABSL_GUARDED_BY(mutex_);
};

//==============================================================================
// WebGpuBindingData
//==============================================================================

// WebGPU implementation of binding data: one bind group, and a slot in one of
// its factory's uniform buffers that holds every constants binding. Constants
// are written to a CPU copy and marked, and the changes are written into the
// slot as the next frame ends, so changing them never changes the bind group.
//
// This class is thread-compatible.
class WebGpuBindingData final : public BindingData, public WebGpuPendingWrite {
 public:
  ~WebGpuBindingData() override;

  const WebGpuBindingDataFactory* GetFactory() const { return factory_.get(); }
  const wgpu::BindGroup& GetBindGroup() const { return bind_group_; }

  void WritePending(const wgpu::Queue& queue,
                    const wgpu::CommandEncoder& encoder) override;

 protected:
  bool Validate(int index, TypeKey* type) const override;
  bool ValidateDataType(int index, const RenderDataType* type) const override;
  void DoSet(int index, const void* value) override;
  void DoGet(int index, void* value) const override;
  void DoGetDependencies(ResourceDependencyList* dependencies) const override;

 private:
  friend class WebGpuBindingDataFactory;

  // A range of bytes in the slot.
  struct Range {
    int64_t begin = 0;
    int64_t end = 0;
  };

  WebGpuBindingData(WebGpuBackend* backend, RenderPipeline* pipeline,
                    std::shared_ptr<WebGpuBindingDataFactory> factory,
                    std::optional<WebGpuBindingDataFactory::Slot> slot,
                    wgpu::Buffer buffer, wgpu::BindGroup bind_group);

  const std::shared_ptr<WebGpuBindingDataFactory> factory_;
  const std::optional<WebGpuBindingDataFactory::Slot> slot_;
  const wgpu::Buffer buffer_;
  const int64_t slot_offset_;
  const wgpu::BindGroup bind_group_;

  // The slot's CPU copy, and the bytes of it changed since the GPU was last
  // written.
  std::vector<uint8_t> data_;
  std::optional<Range> pending_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_BINDING_DATA_H_
