// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_BINDING_LAYOUT_H_
#define GB_RENDER_WEBGPU_WEBGPU_BINDING_LAYOUT_H_

#include <memory>

#include "absl/types/span.h"
#include "gb/render/binding.h"
#include "gb/render/binding_layout.h"
#include "gb/render/webgpu/webgpu_backend.h"
#include "gb/render/webgpu/webgpu_binding_data.h"

namespace gb {

// WebGPU implementation of a binding layout, which holds the layout of its
// views' binding data.
//
// This class is thread-compatible.
class WebGpuBindingLayout final : public BindingLayout {
 public:
  // Creates the binding layout. Returns null, having logged why, if its view
  // bindings can't be bound. `backend` must outlive it.
  static std::unique_ptr<WebGpuBindingLayout> Create(
      WebGpuBackend* backend, absl::Span<const Binding> bindings);

  const WebGpuBindingDataFactory* GetViewFactory() const {
    return view_factory_.get();
  }

  // Creates the binding data of a view, or returns null having logged why.
  std::unique_ptr<WebGpuBindingData> CreateViewBindingData();

 private:
  WebGpuBindingLayout(absl::Span<const Binding> bindings,
                      std::shared_ptr<WebGpuBindingDataFactory> view_factory)
      : BindingLayout(bindings), view_factory_(std::move(view_factory)) {}

  const std::shared_ptr<WebGpuBindingDataFactory> view_factory_;
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_BINDING_LAYOUT_H_
