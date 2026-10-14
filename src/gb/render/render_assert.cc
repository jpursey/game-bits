// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_assert.h"

#include <atomic>

namespace gb {

namespace {
std::atomic<bool> g_assert_enabled = false;
}  // namespace

void SetRenderAssertEnabled(bool enabled) {
  g_assert_enabled.store(enabled, std::memory_order_relaxed);
}

bool IsRenderAssertEnabled() {
#ifdef NDEBUG
  return false;
#else
  return g_assert_enabled.load(std::memory_order_relaxed);
#endif
}

}  // namespace gb
