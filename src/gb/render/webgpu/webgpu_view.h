// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_WEBGPU_WEBGPU_VIEW_H_
#define GB_RENDER_WEBGPU_WEBGPU_VIEW_H_

#include <memory>

#include "gb/render/render_view.h"
#include "gb/render/webgpu/webgpu_binding_data.h"
#include "gb/render/webgpu/webgpu_binding_layout.h"

namespace gb {

// WebGPU implementation of a render view.
//
// This class is thread-compatible.
class WebGpuView final : public RenderView {
 public:
  // Creates a view drawn in `pass`, or in the frame if it is null. Returns
  // null, having logged why, if its binding data can't be created.
  static std::unique_ptr<WebGpuView> Create(RenderPass* pass,
                                            WebGpuBindingLayout* binding_layout,
                                            int view_order);

  WebGpuBindingData* GetWebGpuViewBindingData() {
    return static_cast<WebGpuBindingData*>(GetViewBindingData());
  }

 private:
  WebGpuView(RenderPass* pass, WebGpuBindingLayout* binding_layout,
             int view_order, std::unique_ptr<WebGpuBindingData> view_data)
      : RenderView(pass, binding_layout, view_order, std::move(view_data)) {}
};

}  // namespace gb

#endif  // GB_RENDER_WEBGPU_WEBGPU_VIEW_H_
