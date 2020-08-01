// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_assert.h"

namespace gb {

namespace {
bool assert_enabled = false;
}  // namespace

void SetRenderAssertEnabled(bool enabled) { assert_enabled = enabled; }

bool IsRenderAssertEnabled() {
#if NDEBUG
  return false;
#else
  return assert_enabled;
#endif
}

}  // namespace gb
