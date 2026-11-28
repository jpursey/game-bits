// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_FRAME_WAIT_H_
#define GB_BASE_FRAME_WAIT_H_

namespace gb {

// Waits until the platform's next frame, where the platform paces frames: in
// a browser page, this returns to the browser until its next animation frame,
// which delivers the page's input, shows what was drawn, and paces frames to
// the display. Returns whether it waited, in which case the caller doesn't
// limit its frame rate as well.
//
// Elsewhere, including a program run under Node, this returns false at once.
//
// In a page, this must be called from code that main() runs, not from a
// callback the browser makes (such as SDL's event handlers), which can't wait.
bool WaitForNextFrame();

// Returns to the platform once between frames, without waiting for the
// display, where the platform needs it: in a browser page, so the page stays
// responsive while frames run as fast as they can (as a piloted run's do).
// Elsewhere, this does nothing.
void YieldBetweenFrames();

}  // namespace gb

#endif  // GB_BASE_FRAME_WAIT_H_
