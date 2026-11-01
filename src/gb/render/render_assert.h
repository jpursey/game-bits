// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_ASSERT_H_
#define GB_RENDER_RENDER_ASSERT_H_

#include "gb/render/render_types.h"

namespace gb {

// True if RENDER_ASSERT is compiled in, which it is only in debug builds.
#ifdef NDEBUG
inline constexpr bool kRenderAssertsCompiled = false;
#else
inline constexpr bool kRenderAssertsCompiled = true;
#endif

// If this is a debug build, this will set the render assert to enabled.
// Otherwise, it will have no effect.
void SetRenderAssertEnabled(bool enabled);

// Returns true if render asserts are enabled.
//
// This is always false if kRenderAssertsCompiled is false.
bool IsRenderAssertEnabled();

namespace internal {

// Called by RENDER_ASSERT when its condition fails. This logs the failure as a
// fatal error.
[[noreturn]] void RenderAssertFailed(const char* condition, const char* file,
                                     int line);

}  // namespace internal

// Logs a fatal error naming the condition if render asserts are enabled and
// the condition is false.
//
// Tests check that misuse is caught with death tests, after enabling render
// asserts (and skipping if kRenderAssertsCompiled is false):
//
//   SetRenderAssertEnabled(true);
//   EXPECT_DEATH(draw_list.SetMesh(nullptr), "mesh != nullptr");
#ifdef NDEBUG
#define RENDER_ASSERT(condition) ((void)0)
#else  // NDEBUG
#define RENDER_ASSERT(condition)                                        \
  do {                                                                  \
    if (gb::IsRenderAssertEnabled() && !(condition)) {                  \
      gb::internal::RenderAssertFailed(#condition, __FILE__, __LINE__); \
    }                                                                   \
  } while (false)
#endif  // NDEBUG

}  // namespace gb

#endif  // GB_RENDER_RENDER_ASSERT_H_
