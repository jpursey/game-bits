#include "gbits/game/game.h"

#include "absl/time/clock.h"
#include "glog/logging.h"

namespace gb {

bool Game::Run(GameContract contract,
               const std::vector<std::string_view>& args) {
  if (!contract.IsValid()) {
    LOG(ERROR) << "Game context is not valid!";
    return false;
  }
  context_ = std::move(contract);
  context_.SetPtr<Game>(this);
  bool init_succeeded = Init(args);
  if (init_succeeded) {
    GameLoop();
  }
  CleanUp();
  return init_succeeded;
}

void Game::GameLoop() {
  // Determine minimum delta time based on the requested frame rate.
  int64_t max_fps = context_.GetValue<int>(kKeyMaxFps);
  absl::Duration min_delta_time;
  if (max_fps > 0) {
    min_delta_time = absl::Nanoseconds(1000000000LL / max_fps);
  }

  // Last time is used to determine when min_delta_time is satisfied.
  absl::Time last_time = absl::Now();

  // Cumulative time is the actual amount of time that has past.
  absl::Time cumulative_time = last_time;

  // Delta is how much time has passed since cumulative time.
  absl::Duration delta;
  do {
    delta = absl::Now() - cumulative_time;

    // To check against min_delta_time, we use a delta based on last time (which
    // is strictly the same or earlier than cumulative_time).
    absl::Duration test_delta = delta + (cumulative_time - last_time);

    if (min_delta_time == absl::ZeroDuration()) {
      last_time += delta;
    } else if (test_delta > min_delta_time) {
      // The game is running slower than the desired frame rate. If it is close
      // (within a millisecond), then we will try to absorb the time in the next
      // frame (it may be a one off issue). Otherwise, we just eat the cost in
      // this frame, and give the next frame more time. This is important when a
      // single frame may take many seconds (for instance loading a level).
      if (test_delta - min_delta_time < absl::Milliseconds(1)) {
        last_time += min_delta_time;
      } else {
        last_time += test_delta;
      }
    } else {
      // We have extra time on our hands. Be nice first, and yield time to the
      // system if we need to wait more than a millisecond.
      while (min_delta_time - test_delta > absl::Milliseconds(1)) {
        absl::SleepFor(min_delta_time - test_delta);
        test_delta = absl::Now() - last_time;
      }

      // Busy loop the rest of the time, if there is any.
      while (test_delta < min_delta_time) {
        test_delta = absl::Now() - last_time;
      }

      // Calculate the actual delta.
      delta = test_delta - (cumulative_time - last_time);

      // Advance last time by min_delta_time so we can maintain a reliable frame
      // rate. Otherwise we will drift slightly slower than the desired rate.
      last_time += min_delta_time;
    }
    cumulative_time += delta;
  } while (Update(delta));
}

}  // namespace gb
