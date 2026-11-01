// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_assert.h"

#include <atomic>

#include "absl/log/log.h"

namespace gb {

namespace {
std::atomic<bool> g_assert_enabled = false;
}  // namespace

void SetRenderAssertEnabled(bool enabled) {
  g_assert_enabled.store(enabled, std::memory_order_relaxed);
}

bool IsRenderAssertEnabled() {
  return kRenderAssertsCompiled &&
         g_assert_enabled.load(std::memory_order_relaxed);
}

namespace internal {

void RenderAssertFailed(const char* condition, const char* file, int line) {
  LOG(FATAL).AtLocation(file, line) << "Render assert failed: " << condition;
}

}  // namespace internal

}  // namespace gb
