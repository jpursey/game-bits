#include "gbits/game/game.h"

namespace gb {

void Game::Run(const std::vector<std::string_view>& args) {
  Init(args);
  // TODO
  CleanUp();
}

}  // namespace gb
