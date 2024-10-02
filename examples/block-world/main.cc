// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <string>
#include <vector>

#include "SDL_main.h"

// Game includes
#include "block_world.h"

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
  BlockWorld game;
  return game.Run(argc, argv) ? 0 : 1;
}
