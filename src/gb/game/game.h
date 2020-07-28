#ifndef GB_GAME_GAME_H_
#define GB_GAME_GAME_H_

#include <memory>
#include <string_view>
#include <vector>

#include "absl/time/time.h"
#include "gb/base/clock.h"
#include "gb/base/context.h"
#include "gb/base/validated_context.h"

namespace gb {

// The class provides a framework for the most basic game loop. It provides
// support for a fixed or variable frame rate game loop.
//
// This class is thread-safe.
class Game {
 public:
  // Maximum frame rate that the game will run at. This can be set to zero to
  // run at an unlimited frame rate. By default, games are limited to 60 FPS.
  static inline constexpr char* kKeyMaxFps = "max_fps";
  static inline constexpr int kDefaultMaxFps = 60;
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintMaxFps, kInOptional,
                                             int, kKeyMaxFps, kDefaultMaxFps);

  // Optional clock class that is used for doing all timing. Mainly this is
  // useful for tests where the time needs to be precisely controlled. If this
  // is not set, then the realtime clock will be used.
  static GB_CONTEXT_CONSTRAINT(kConstraintClock, kInOptional, Clock);

  // Pointer to this class. This is always set while the game is running.
  static GB_CONTEXT_CONSTRAINT(kConstraintGame, kScoped, Game);

  // Contract guaranteed by this class. Derived classes may provide additional
  // constraints.
  using GameContract =
      gb::ContextContract<kConstraintMaxFps, kConstraintClock, kConstraintGame>;

  // Default constructor and destructor.
  explicit Game() = default;
  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;
  virtual ~Game() = default;

  // Returns the context used by the game. This context is only valid after
  // Run() is called. Derived classes may override this if they have a different
  // context contract.
  virtual gb::ValidatedContext& Context() { return context_; }

  // Runs the game with the specified arguments and optional contract. Returns
  // true if the game exited normally, or false if an error occurred during
  // initialization.
  bool Run(GameContract contract, const std::vector<std::string_view>& args);
  bool Run(GameContract contract, int argc, char** argv) {
    return Run(std::move(contract),
               std::vector<std::string_view>(argv + 1, argv + argc));
  }
  bool Run(const std::vector<std::string_view>& args) {
    return Run(std::make_unique<gb::Context>(), args);
  }
  bool Run(int argc, char** argv) {
    return Run(std::make_unique<gb::Context>(), argc, argv);
  }

 protected:
  // Init is called at program start with the command line arguments (if any)
  // passed from main. It should return false if the game cannot continue. Note
  // that CleanUp() will get called no matter what value is returned.
  virtual bool Init(const std::vector<std::string_view>& args) { return true; }

  // Updates the game no faster than the max frame rate. Update should return
  // true to indicate the game should continue, and false if the game should
  // exit.
  virtual bool Update(absl::Duration delta_time) { return true; }

  // This is called right before the game exits.
  virtual void CleanUp() {}

 private:
  void GameLoop();

  Clock* clock_;
  ValidatedContext context_;
};

}  // namespace gb

#endif  // GB_GAME_GAME_H_
