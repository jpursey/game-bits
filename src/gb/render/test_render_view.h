// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_RENDER_VIEW_H_
#define GB_RENDER_TEST_RENDER_VIEW_H_

#include "gb/render/render_types.h"
#include "gb/render/render_view.h"
#include "gb/render/test_binding_data.h"

namespace gb {

class TestRenderView final : public RenderView {
 public:
  TestRenderView(BindingLayout* binding_layout, int view_order)
      : RenderView(
            binding_layout, view_order,
            std::make_unique<TestBindingData>(nullptr, BindingSet::kView,
                                              binding_layout->GetBindings())) {}
};

}  // namespace gb

#endif  // GB_RENDER_TEST_RENDER_VIEW_H_
