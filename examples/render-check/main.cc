// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "SDL_main.h"

// Game includes
#include "render_check.h"

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
  RenderCheck render_check;
  if (!render_check.Init()) {
    return 1;
  }
  render_check.Run();
  return 0;
}
