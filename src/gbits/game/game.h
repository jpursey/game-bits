#ifndef GBITS_GAME_GAME_H_
#define GBITS_GAME_GAME_H_

#include <string_view>
#include <vector>

#include "absl/time/time.h"

#include "gbits/base/context.h"

namespace gb {

class Game {
 public:
  Game() = default;
  virtual ~Game() = default;

  gb::Context& Context() { return context_; }

  void Run(const std::vector<std::string_view>& args);

 protected:
  // Init is called at program start with the command line arguments (if any)
  // passed from main. It should return false if the game cannot continue. Note
  // that CleanUp() will get called no matter what value is returned.
  virtual bool Init(const std::vector<std::string_view>& args) {}

  // This is called right before the game exits.
  virtual void CleanUp() {}

private:
  gb::Context context_;
};

}  // namespace gb

#endif  // GBITS_GAME_GAME_H_
