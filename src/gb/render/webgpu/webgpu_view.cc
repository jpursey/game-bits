// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/webgpu/webgpu_view.h"

#include <utility>

#include "absl/memory/memory.h"

namespace gb {

std::unique_ptr<WebGpuView> WebGpuView::Create(
    RenderPass* pass, WebGpuBindingLayout* binding_layout, int view_order) {
  auto view_data = binding_layout->CreateViewBindingData();
  if (view_data == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new WebGpuView(pass, binding_layout, view_order, std::move(view_data)));
}

}  // namespace gb
