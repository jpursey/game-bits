// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_view.h"

#include "absl/memory/memory.h"

namespace gb {

std::unique_ptr<VulkanView> VulkanView::Create(
    VulkanInternal, VulkanBindingLayout* binding_layout, int view_order) {
  auto view_data = binding_layout->CreateViewBindingData();
  if (view_data == nullptr) {
    return nullptr;
  }
  return absl::WrapUnique(
      new VulkanView(binding_layout, view_order, std::move(view_data)));
}

}  // namespace gb
