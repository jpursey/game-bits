// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/game/game.h"

#include "absl/log/log.h"
#include "absl/time/clock.h"

namespace gb {

bool Game::Run(GameContract contract,
               const std::vector<std::string_view>& args) {
  if (!contract.IsValid()) {
    LOG(ERROR) << "Game context is not valid!";
    return false;
  }
  context_ = std::move(contract);
  context_.SetPtr<Game>(this);
  clock_ = context_.GetPtr<Clock>();
  if (clock_ == nullptr) {
    clock_ = RealtimeClock::GetClock();
  }
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

  absl::Time last_time = clock_->Now();
  absl::Time next_time = last_time + min_delta_time;
  absl::Duration delta_time;
  do {
    absl::Time now = clock_->Now();
    delta_time = now - last_time;
    absl::Duration time_remaining = next_time - now;
    if (time_remaining < absl::ZeroDuration()) {
      // The game is running slower than the desired frame rate (or the frame
      // rate is unlocked). If it is close (within a millisecond), then we will
      // try to absorb the time in the next frame (it may be a one off issue).
      // Otherwise, we just eat the cost in this frame, and give the next frame
      // more time. This is important when a single frame may take many seconds
      // (for instance loading a level).
      if (time_remaining < -absl::Milliseconds(1)) {
        next_time = now;
      } else {
        next_time += min_delta_time;
      }
    } else {
      // We have extra time on our hands. Be nice first, and yield time to the
      // system if we need to wait more than a millisecond.
      while (time_remaining > absl::Milliseconds(1)) {
        clock_->SleepFor(time_remaining - absl::Milliseconds(1));
        now = clock_->Now();
        time_remaining = next_time - now;
      }

      // Busy loop the rest of the time, if there is any.
      while (time_remaining > absl::ZeroDuration()) {
        now = clock_->Now();
        time_remaining = next_time - now;
      }

      // Calculate the actual delta.
      delta_time = now - last_time;

      // Advance last time by min_delta_time so we can maintain a reliable frame
      // rate. Otherwise we will drift slightly slower than the desired rate.
      next_time += min_delta_time;
    }
    last_time = now;
  } while (Update(delta_time));
}

}  // namespace gb
