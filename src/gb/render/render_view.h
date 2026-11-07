// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_VIEW_H_
#define GB_RENDER_RENDER_VIEW_H_

#include <memory>

#include "gb/render/binding_data.h"
#include "gb/render/binding_layout.h"
#include "gb/render/render_types.h"

namespace gb {

// A RenderView is a group of draws that share the same view binding data (such
// as a camera), drawn in the view's order relative to other views.
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
  virtual ~RenderView() = default;

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the binding layout this view is built on.
  //
  // The binding layout is registered with the RenderSystem. It defines the
  // bindings common to this view and every material type drawn in it.
  BindingLayout* GetBindingLayout() const { return binding_layout_; }

  // Returns the view order for this view.
  //
  // Views in an earlier order are drawn before views in a later order, such as
  // a UI view drawn over a 3D view. Views that have the same view order are
  // still drawn one after the other, but in an indeterminate order. View order
  // does not imply any sort of memory dependency, so for instance it is not
  // possible for a later ordered view to read the results of an earlier
  // ordered view.
  int GetOrder() const { return order_; }

  // Returns the view binding data for this view.
  //
  // This data is applied when rendering anything that uses this view.
  const BindingData* GetViewBindingData() const { return view_data_.get(); }
  BindingData* GetViewBindingData() { return view_data_.get(); }

 protected:
  RenderView(BindingLayout* binding_layout, int view_order,
             std::unique_ptr<BindingData> view_data)
      : binding_layout_(binding_layout),
        order_(view_order),
        view_data_(std::move(view_data)) {}

 private:
  BindingLayout* const binding_layout_;
  const int order_;
  std::unique_ptr<BindingData> view_data_;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_VIEW_H_
