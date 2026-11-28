// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_binding_layout.h"

#include <utility>

#include "absl/memory/memory.h"

namespace gb {

std::unique_ptr<WebGpuBindingLayout> WebGpuBindingLayout::Create(
    WebGpuBackend* backend, absl::Span<const Binding> bindings) {
  std::shared_ptr<const WebGpuBindingSetLayout> view_layout;
  if (!backend->CreateInErrorScope("a binding layout", [&] {
        view_layout =
            WebGpuBindingSetLayout::Create(backend, BindingSet::kView, bindings);
      }) ||
      view_layout == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new WebGpuBindingLayout(backend, bindings, std::move(view_layout)));
}

std::unique_ptr<WebGpuBindingData> WebGpuBindingLayout::CreateViewBindingData() {
  return WebGpuBindingData::Create(backend_, nullptr, view_layout_);
}

}  // namespace gb
