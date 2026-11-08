// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_VIEW_H_
#define GB_RENDER_RENDER_VIEW_H_

#include <memory>

#include "gb/render/binding_data.h"
#include "gb/render/binding_layout.h"
#include "gb/render/render_pass.h"
#include "gb/render/render_types.h"

namespace gb {

// A RenderView is a group of draws that share the same view binding data (such
// as a camera), drawn in the view's order relative to other views in the same
// pass, or in the frame.
//
// Every RenderSystem draw is in a view. A view is built on a binding layout,
// and draws only material types built on the same one. Several views may share
// a binding layout, each with view binding data of its own, such as a world
// and a reflection of it drawn with different cameras.
//
// This is a view in the sense game engines use: a view of draws. It is not a
// window onto data, as ImageView and MeshView are.
//
// This class and all derived classes must be thread-compatible.
class RenderView {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  RenderView(const RenderView&) = delete;
  RenderView(RenderView&&) = delete;
  RenderView& operator=(const RenderView&) = delete;
  RenderView& operator=(RenderView&&) = delete;
  virtual ~RenderView() {
    if (pass_ != nullptr) {
      pass_->RemoveView();
    }
  }

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the pass this view is drawn in, or null if it is drawn in the
  // frame.
  RenderPass* GetPass() const { return pass_; }

  // Returns the binding layout this view is built on.
  //
  // The binding layout is registered with the RenderSystem. It defines the
  // bindings common to this view and every material type drawn in it.
  BindingLayout* GetBindingLayout() const { return binding_layout_; }

  // Returns the view order for this view.
  //
  // Within a pass (or the frame), views in an earlier order are drawn before
  // views in a later order, such as a UI view drawn over a 3D view. Views that
  // have the same view order are still drawn one after the other, but in an
  // indeterminate order. View order does not imply any sort of memory
  // dependency, so a later ordered view can't read the results of an earlier
  // ordered view: to read what was drawn, draw it in an earlier pass.
  int GetOrder() const { return order_; }

  // Returns the view binding data for this view.
  //
  // This data is applied when rendering anything that uses this view.
  const BindingData* GetViewBindingData() const { return view_data_.get(); }
  BindingData* GetViewBindingData() { return view_data_.get(); }

 protected:
  // The pass is null for a view drawn in the frame, and must outlive this.
  RenderView(RenderPass* pass, BindingLayout* binding_layout, int view_order,
             std::unique_ptr<BindingData> view_data)
      : pass_(pass),
        binding_layout_(binding_layout),
        order_(view_order),
        view_data_(std::move(view_data)) {
    if (pass_ != nullptr) {
      pass_->AddView();
    }
  }

 private:
  RenderPass* const pass_;
  BindingLayout* const binding_layout_;
  const int order_;
  std::unique_ptr<BindingData> view_data_;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_VIEW_H_
