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
  FLAGS_alsologtostderr = 1;
  google::InitGoogleLogging(argv[0]);
  google::EnableLogCleaner(1);

  BlockWorld game;
  return game.Run(argc, argv) ? 0 : 1;
}
