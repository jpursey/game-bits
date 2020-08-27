// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/vulkan/vulkan_scene_type.h"

#include <vector>

#include "absl/memory/memory.h"

namespace gb {

std::unique_ptr<VulkanSceneType> VulkanSceneType::Create(
    VulkanInternal, VulkanBackend* backend,
    absl::Span<const Binding> bindings) {
  std::vector<Binding> scene_bindings;
  for (const Binding& binding : bindings) {
    if (binding.set == BindingSet::kScene) {
      scene_bindings.push_back(binding);
    }
  }
  auto scene_data_factory = VulkanBindingDataFactory::Create(
      {}, backend, kMaxScenesPerGroup, scene_bindings);
  if (scene_data_factory == nullptr) {
    return nullptr;
  }

  return absl::WrapUnique(
      new VulkanSceneType(bindings, std::move(scene_data_factory)));
}

}  // namespace gb
