// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_TEST_BINDING_LAYOUT_H_
#define GB_RENDER_TEST_BINDING_LAYOUT_H_

#include "gb/render/binding_layout.h"
#include "gb/render/render_types.h"

namespace gb {

class TestBindingLayout final : public BindingLayout {
 public:
  TestBindingLayout(absl::Span<const Binding> bindings)
      : BindingLayout(bindings) {}
};

}  // namespace gb

#endif  // GB_RENDER_TEST_BINDING_LAYOUT_H_
