// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/game/game.h"

#include "absl/log/log.h"
#include "gb/base/frame_wait.h"

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
  pilot_ = context_.GetPtr<Pilot>();
  bool succeeded = Init(args);
  if (succeeded) {
    if (pilot_ != nullptr) {
      succeeded = PilotLoop();
    } else {
      GameLoop();
    }
  }
  CleanUp();
  return succeeded;
}

bool Game::PilotLoop() {
  bool running = true;
  while (running && !pilot_->IsDone()) {
    pilot_->BeginFrame();
    running = Update(pilot_->GetDeltaTime());
    pilot_->EndFrame();
    YieldBetweenFrames();
  }
  if (!pilot_->IsDone()) {
    LOG(ERROR) << "The game stopped before its pilot was done.";
    return false;
  }
  return !pilot_->HasFailed();
}

void Game::GameLoop() {
  // Determine minimum delta time based on the requested frame rate.
  const int max_fps = context_.GetValue<int>(kKeyMaxFps);
  absl::Duration min_delta_time;
  if (max_fps > 0) {
    min_delta_time = absl::Seconds(1) / max_fps;
  }

  absl::Time last_time = clock_->Now();
  absl::Time next_time = last_time + min_delta_time;
  absl::Duration delta_time;
  do {
    // Where the platform paces frames (in a browser page), the display paces
    // them, and the loop doesn't limit them as well.
    const bool paced = WaitForNextFrame();
    absl::Time now = clock_->Now();
    absl::Duration time_remaining = next_time - now;
    if (time_remaining < absl::ZeroDuration()) {
      // The game is running slower than the desired frame rate (or the frame
      // rate is unlocked). If it is close (within a millisecond), then we will
      // try to absorb the time in the next frame (it may be a one off issue).
      // Otherwise, we just eat the cost in this frame, and give the next frame
      // a full frame from now. This is important when a single frame may take
      // many seconds (for instance loading a level).
      if (time_remaining < -absl::Milliseconds(1)) {
        next_time = now;
      }
    } else if (!paced) {
      // We have extra time on our hands, so wait until the frame is due.
      clock_->SleepUntil(next_time);
      now = clock_->Now();
    }
    delta_time = now - last_time;

    // Advance the next time by min_delta_time (rather than from now) so we can
    // maintain a reliable frame rate. Otherwise we will drift slightly slower
    // than the desired rate.
    next_time += min_delta_time;
    last_time = now;
  } while (Update(delta_time));
}

}  // namespace gb
