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
  std::shared_ptr<WebGpuBindingDataFactory> view_factory;
  if (!backend->CreateInErrorScope("a binding layout", [&] {
        view_factory =
            WebGpuBindingDataFactory::Create(backend, BindingSet::kView, bindings);
      }) ||
      view_factory == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new WebGpuBindingLayout(bindings, std::move(view_factory)));
}

std::unique_ptr<WebGpuBindingData> WebGpuBindingLayout::CreateViewBindingData() {
  return view_factory_->NewBindingData(nullptr);
}

}  // namespace gb
