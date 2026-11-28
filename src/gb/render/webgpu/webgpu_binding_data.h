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

#include "absl/types/span.h"
#include "gb/render/binding.h"
#include "gb/render/binding_data.h"
#include "gb/render/render_types.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "webgpu/webgpu_cpp.h"

namespace gb {

//==============================================================================
// WebGpuBindingSetLayout
//==============================================================================

// What a binding set's binding data holds: its bindings, and the bind group
// layout of each binding data's bind group. It is shared by what makes the
// binding data (a binding layout or a pipeline) and each binding data, so
// either may outlive the other.
struct WebGpuBindingSetLayout {
  // Creates the layout of the bindings in `bindings` that are in `set`.
  // Returns null, having logged why, if a binding can't be bound yet (only
  // constants can), or its index is beyond kMaxWebGpuBindingIndex.
  //
  // `backend` is only used while this is created.
  static std::shared_ptr<const WebGpuBindingSetLayout> Create(
      WebGpuBackend* backend, BindingSet set,
      absl::Span<const Binding> bindings);

  // One binding index.
  struct Slot {
    BindingType binding_type = BindingType::kNone;
    const RenderDataType* data_type = nullptr;

    // Where a constants binding's values are in the uniform buffer, and their
    // size as bound.
    int64_t offset = 0;
    int64_t size = 0;
  };

  BindingSet set = BindingSet::kView;

  // By binding index, kNone where the set has no binding.
  std::vector<Slot> slots;

  // The size of each binding data's uniform buffer, or 0 for none.
  int64_t buffer_size = 0;

  wgpu::BindGroupLayout layout;
};

//==============================================================================
// WebGpuBindingData
//==============================================================================

// WebGPU implementation of binding data: one bind group, and a uniform buffer
// that holds every constants binding, each at an offset of its own. Constants
// are written to a CPU copy and marked, and the changes are written as the
// next frame ends, so changing them never changes the bind group.
//
// This class is thread-compatible.
class WebGpuBindingData final : public BindingData, public WebGpuPendingWrite {
 public:
  // Creates binding data for `pipeline` (null for a view's) with the layout.
  // Returns null, having logged why, if its bind group or buffer can't be
  // created. `backend` must outlive it.
  static std::unique_ptr<WebGpuBindingData> Create(
      WebGpuBackend* backend, RenderPipeline* pipeline,
      std::shared_ptr<const WebGpuBindingSetLayout> layout);

  ~WebGpuBindingData() override;

  const WebGpuBindingSetLayout* GetLayout() const { return layout_.get(); }
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
  // A range of bytes in the uniform buffer.
  struct Range {
    int64_t begin = 0;
    int64_t end = 0;
  };

  WebGpuBindingData(WebGpuBackend* backend, RenderPipeline* pipeline,
                    std::shared_ptr<const WebGpuBindingSetLayout> layout,
                    wgpu::Buffer buffer, wgpu::BindGroup bind_group);

  const std::shared_ptr<const WebGpuBindingSetLayout> layout_;
  wgpu::Buffer buffer_;
  const wgpu::BindGroup bind_group_;

  // The uniform buffer's CPU copy, and the bytes of it changed since the GPU
  // was last written.
  std::vector<uint8_t> data_;
  std::optional<Range> pending_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_BINDING_DATA_H_
