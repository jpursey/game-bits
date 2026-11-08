// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_VIEW_H_
#define GB_RENDER_VULKAN_VULKAN_VIEW_H_

#include "gb/render/render_view.h"
#include "gb/render/vulkan/vulkan_binding_layout.h"
#include "gb/render/vulkan/vulkan_types.h"

namespace gb {

// Vulkan implementation of RenderView.
//
// This class is thread-safe.
class VulkanView : public RenderView {
 public:
  // The pass is null for a view drawn in the frame.
  static std::unique_ptr<VulkanView> Create(VulkanInternal, RenderPass* pass,
                                            VulkanBindingLayout* binding_layout,
                                            int view_order);

 private:
  VulkanView(RenderPass* pass, VulkanBindingLayout* binding_layout,
             int view_order, std::unique_ptr<VulkanBindingData> view_data)
      : RenderView(pass, binding_layout, view_order, std::move(view_data)) {}
};

}  // namespace gb

#endif  // GB_RENDER_VULKAN_VULKAN_VIEW_H_
