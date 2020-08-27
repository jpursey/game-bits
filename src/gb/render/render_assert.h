// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_ASSERT_H_
#define GB_RENDER_RENDER_ASSERT_H_

#include <cassert>

namespace gb {

// If this is a debug build, this will set the render assert to enabled.
// Otherwise, it will have no effect.
void SetRenderAssertEnabled(bool enabled);

// Returns true if render asserts are enabled.
bool IsRenderAssertEnabled();

#ifdef NDEBUG
#define RENDER_ASSERT(condition) ((void)0)
#else  // NDEBUG
#define RENDER_ASSERT(condition)                    \
  do {                                              \
    if (gb::IsRenderAssertEnabled()) assert(condition); \
  } while (false)
#endif  // NDEBUG

}  // namespace gb

#endif  // GB_RENDER_RENDER_ASSERT_H_
