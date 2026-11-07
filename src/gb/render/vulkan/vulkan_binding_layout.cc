// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_binding_layout.h"

#include <vector>

#include "absl/memory/memory.h"

namespace gb {

std::unique_ptr<VulkanBindingLayout> VulkanBindingLayout::Create(
    VulkanInternal, VulkanBackend* backend,
    absl::Span<const Binding> bindings) {
  std::vector<Binding> view_bindings;
  for (const Binding& binding : bindings) {
    if (binding.set == BindingSet::kView) {
      view_bindings.push_back(binding);
    }
  }
  auto view_data_factory = VulkanBindingDataFactory::Create(
      {}, backend, kMaxViewsPerGroup, view_bindings);
  if (view_data_factory == nullptr) {
    return nullptr;
  }

  return absl::WrapUnique(
      new VulkanBindingLayout(bindings, std::move(view_data_factory)));
}

}  // namespace gb
