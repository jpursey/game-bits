#include <string>
#include <vector>

#include "SDL_main.h"
#include "block_world.h"

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
  FLAGS_alsologtostderr = 1;
  google::InitGoogleLogging(argv[0]);
  google::EnableLogCleaner(1);

  BlockWorld game;
  return game.Run(argc, argv) ? 0 : 1;
}
