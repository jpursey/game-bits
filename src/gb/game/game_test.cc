// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/game/game.h"

#include <string_view>
#include <vector>

#include "absl/time/time.h"
#include "gb/base/clock.h"
#include "gb/base/context.h"
#include "gb/base/fake_clock.h"
#include "gb/pilot/fake_pilot.h"
#include "gb/pilot/pilot.h"
#include "gb/test/log_error_guard.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;

class TestGame : public Game {
 public:
  explicit TestGame(FakeClock* clock = nullptr) : clock_(clock) {}
  ~TestGame() override = default;

  void SetInitResult(bool init_result) { init_result_ = init_result; }
  void SetUpdateCount(int update_count) { update_count_ = update_count; }
  void SetUpdateDelay(absl::Duration update_delay) {
    update_delay_ = update_delay;
  }
  void SetFirstUpdateDelay(absl::Duration first_update_delay) {
    first_update_delay_ = first_update_delay;
  }

  const std::vector<std::string_view>& GetInitArgs() const {
    return init_args_;
  }
  absl::Duration GetLastDeltaTime() const { return last_delta_time_; }
  absl::Duration GetTotalUpdateTime() const { return total_update_time_; }
  bool InitRun() const { return init_run_; }
  bool UpdateRun() const { return update_run_; }
  bool CleanupRun() const { return cleanup_run_; }

 protected:
  bool Init(const std::vector<std::string_view>& args) override {
    init_run_ = true;
    init_args_ = args;
    return init_result_;
  }

  bool Update(absl::Duration delta_time) override {
    update_run_ = true;
    last_delta_time_ = delta_time;
    total_update_time_ += delta_time;
    if (clock_ != nullptr) {
      clock_->AdvanceTime(update_delay_ + first_update_delay_);
      first_update_delay_ = absl::ZeroDuration();
    }
    return --update_count_ > 0;
  }

  void CleanUp() override { cleanup_run_ = true; }

 private:
  FakeClock* clock_ = nullptr;
  int update_count_ = 1;
  bool init_result_ = true;
  bool init_run_ = false;
  bool update_run_ = false;
  bool cleanup_run_ = false;
  absl::Duration update_delay_;
  absl::Duration first_update_delay_;
  absl::Duration last_delta_time_;
  absl::Duration total_update_time_;
  std::vector<std::string_view> init_args_;
};

// Runs games passed context_ on a fake clock, so they don't wait on real time.
class GameTest : public ::testing::Test {
 protected:
  GameTest() {
    clock_.SetAutoAdvance(absl::Microseconds(10));
    context_.SetPtr<Clock>(&clock_);
  }

  FakeClock clock_;
  Context context_;
};

TEST_F(GameTest, GameIsNotAbstract) { Game game; }

TEST_F(GameTest, RunNoContextNoArgs) {
  TestGame game;
  EXPECT_TRUE(game.Run({}));
  EXPECT_TRUE(game.InitRun());
  EXPECT_TRUE(game.UpdateRun());
  EXPECT_TRUE(game.CleanupRun());
  EXPECT_EQ(game.Context().GetPtr<Game>(), &game);
  EXPECT_EQ(game.Context().GetValue<int>(Game::kKeyMaxFps),
            Game::kDefaultMaxFps);
  EXPECT_GE(game.GetLastDeltaTime(),
            absl::Seconds(1.0 / static_cast<double>(Game::kDefaultMaxFps)) -
                absl::Microseconds(1));
  EXPECT_THAT(game.GetInitArgs(), IsEmpty());
}

TEST_F(GameTest, RunInvalidFps) {
  context_.SetValue<double>(Game::kKeyMaxFps, 1.0 / 60.0);
  TestGame game;
  EXPECT_FALSE(game.Run(&context_, {}));
  EXPECT_FALSE(game.InitRun());
  EXPECT_FALSE(game.UpdateRun());
  EXPECT_FALSE(game.CleanupRun());
}

TEST_F(GameTest, RunSpecifiedFps) {
  context_.SetValue<int>(Game::kKeyMaxFps, 30);
  TestGame game;
  EXPECT_TRUE(game.Run(&context_, {}));
  EXPECT_EQ(game.Context().GetValue<int>(Game::kKeyMaxFps), 30);
  EXPECT_GE(game.GetLastDeltaTime(),
            absl::Seconds(1.0 / 30.0) - absl::Microseconds(1));
}

TEST_F(GameTest, RunNoContextWithArgs) {
  TestGame game;
  std::vector<std::string_view> args = {"one", "two", "three"};
  EXPECT_TRUE(game.Run(args));
  EXPECT_EQ(game.GetInitArgs(), args);
}

TEST_F(GameTest, RunNoContextWithArgcArgv) {
  TestGame game;
  std::vector<std::string_view> args = {"one", "two", "three"};
  char* argv[] = {
      const_cast<char*>("executable-name"),
      const_cast<char*>(args[0].data()),
      const_cast<char*>(args[1].data()),
      const_cast<char*>(args[2].data()),
  };
  EXPECT_TRUE(game.Run(4, argv));
  EXPECT_EQ(game.GetInitArgs(), args);
}

TEST_F(GameTest, RunWithContextWithArgs) {
  context_.SetValue<int>(Game::kKeyMaxFps, 30);
  TestGame game;
  std::vector<std::string_view> args = {"one", "two", "three"};
  EXPECT_TRUE(game.Run(&context_, args));
  EXPECT_EQ(game.Context().GetValue<int>(Game::kKeyMaxFps), 30);
  EXPECT_EQ(game.GetInitArgs(), args);
}

TEST_F(GameTest, RunWithContextWithArgcArgv) {
  context_.SetValue<int>(Game::kKeyMaxFps, 30);
  TestGame game;
  std::vector<std::string_view> args = {"one", "two", "three"};
  char* argv[] = {
      const_cast<char*>("executable-name"),
      const_cast<char*>(args[0].data()),
      const_cast<char*>(args[1].data()),
      const_cast<char*>(args[2].data()),
  };
  EXPECT_TRUE(game.Run(&context_, 4, argv));
  EXPECT_EQ(game.Context().GetValue<int>(Game::kKeyMaxFps), 30);
  EXPECT_EQ(game.GetInitArgs(), args);
}

TEST_F(GameTest, RunWithEmptyArgv) {
  TestGame game;
  char* argv[] = {nullptr};
  EXPECT_TRUE(game.Run(&context_, 0, argv));
  EXPECT_THAT(game.GetInitArgs(), IsEmpty());
}

TEST_F(GameTest, RunWithInitFailure) {
  TestGame game;
  game.SetInitResult(false);
  EXPECT_FALSE(game.Run({}));
  EXPECT_TRUE(game.InitRun());
  EXPECT_FALSE(game.UpdateRun());
  EXPECT_TRUE(game.CleanupRun());
}

TEST_F(GameTest, GameTimesDoNotDrift) {
  // Pick two prime numbers that are never going to line up perfecting to 10
  // millisecond frame times.
  clock_.SetAutoAdvance(absl::Microseconds(3001));
  clock_.SetSleepOffset(absl::Microseconds(1009));
  const absl::Duration max_drift = absl::Milliseconds(10);
  context_.SetValue<int>(Game::kKeyMaxFps, 100);

  // Run for enough time to detect drift.
  TestGame game;
  const absl::Duration expected_duration = absl::Minutes(1);
  game.SetUpdateCount(6000);  // One minutes at 100 fps

  const absl::Time start = clock_.GetTime();
  EXPECT_TRUE(game.Run(&context_, {}));
  const absl::Duration time = clock_.GetTime() - start;
  EXPECT_GE(time, expected_duration);
  EXPECT_LE(time, expected_duration + max_drift);
  EXPECT_GE(game.GetTotalUpdateTime(), expected_duration);
  EXPECT_LE(game.GetTotalUpdateTime(), expected_duration + max_drift);
}

TEST_F(GameTest, GameFrameRateSlows) {
  const absl::Duration max_drift = absl::Milliseconds(10);
  context_.SetValue<int>(Game::kKeyMaxFps, 100);

  TestGame game(&clock_);
  const absl::Duration expected_duration =
      absl::Seconds(100) + absl::Milliseconds(10);
  const absl::Duration update_delay = absl::Seconds(1);
  game.SetUpdateDelay(update_delay);
  game.SetUpdateCount(100);

  const absl::Time start = clock_.GetTime();
  EXPECT_TRUE(game.Run(&context_, {}));
  const absl::Duration time = clock_.GetTime() - start;
  EXPECT_GE(time, expected_duration);
  EXPECT_LE(time, expected_duration + max_drift);
  EXPECT_GE(game.GetTotalUpdateTime(), expected_duration - update_delay);
  EXPECT_LE(game.GetTotalUpdateTime(),
            expected_duration - update_delay + max_drift);
}

TEST_F(GameTest, GameFrameRateRecoversAfterSlowFrame) {
  context_.SetValue<int>(Game::kKeyMaxFps, 100);

  // One long frame (such as loading a level), followed by frames that take
  // less time than the frame rate allows.
  TestGame game(&clock_);
  game.SetFirstUpdateDelay(absl::Seconds(1));
  game.SetUpdateDelay(absl::Milliseconds(2));
  game.SetUpdateCount(10);

  EXPECT_TRUE(game.Run(&context_, {}));
  EXPECT_GE(game.GetLastDeltaTime(), absl::Milliseconds(10));
  EXPECT_LE(game.GetLastDeltaTime(),
            absl::Milliseconds(10) + absl::Microseconds(100));
}

//------------------------------------------------------------------------------
// Piloted games
//------------------------------------------------------------------------------

// A clock that fails the test if it is used, as a piloted game never waits on
// real time.
class UnusedClock final : public Clock {
 public:
  absl::Time Now() override {
    ADD_FAILURE() << "Now() called";
    return absl::UnixEpoch();
  }
  void SleepFor(absl::Duration duration) override {
    ADD_FAILURE() << "SleepFor() called";
  }
};

// A game that records each update, and sets its pilot done after a number of
// updates.
class PilotTestGame final : public Game {
 public:
  // `pilot` must outlive this.
  PilotTestGame(FakePilot* pilot, int done_after_updates)
      : pilot_(pilot), done_after_updates_(done_after_updates) {}

  // Stops the game itself at an update, before the pilot is done.
  void SetStopAtUpdate(int update) { stop_at_update_ = update; }

  // What each update saw: its delta time, the pilot's frame, and how many
  // input events the frame had.
  struct UpdateInfo {
    bool operator==(const UpdateInfo&) const = default;

    absl::Duration delta_time;
    int frame = 0;
    int input_count = 0;
  };
  const std::vector<UpdateInfo>& GetUpdates() const { return updates_; }

  bool InitRun() const { return init_run_; }
  bool CleanupRun() const { return cleanup_run_; }

 protected:
  bool Init(const std::vector<std::string_view>& args) override {
    init_run_ = true;
    return true;
  }

  bool Update(absl::Duration delta_time) override {
    updates_.push_back(
        {.delta_time = delta_time,
         .frame = pilot_->GetFrame(),
         .input_count = static_cast<int>(pilot_->GetInput().size())});
    const int update = static_cast<int>(updates_.size());
    if (update == done_after_updates_) {
      pilot_->SetDone(true);
    }
    return update != stop_at_update_;
  }

  void CleanUp() override { cleanup_run_ = true; }

 private:
  FakePilot* const pilot_;
  const int done_after_updates_;
  int stop_at_update_ = 0;
  bool init_run_ = false;
  bool cleanup_run_ = false;
  std::vector<UpdateInfo> updates_;
};

class GamePilotTest : public ::testing::Test {
 protected:
  GamePilotTest() {
    pilot_.SetDeltaTime(absl::Milliseconds(10));
    context_.SetPtr<Clock>(&clock_);
    context_.SetPtr<Pilot>(&pilot_);
  }

  LogErrorGuard log_error_guard_;
  UnusedClock clock_;
  FakePilot pilot_;
  Context context_;
};

TEST_F(GamePilotTest, RunsAFrameForEachUpdateUntilThePilotIsDone) {
  pilot_.SetNextFrameInput({PilotTextEvent{.text = "a"}});
  PilotTestGame game(&pilot_, 3);
  EXPECT_TRUE(game.Run(&context_, {}));
  using UpdateInfo = PilotTestGame::UpdateInfo;
  EXPECT_THAT(game.GetUpdates(),
              ElementsAre(UpdateInfo{absl::Milliseconds(10), 0, 1},
                          UpdateInfo{absl::Milliseconds(10), 1, 0},
                          UpdateInfo{absl::Milliseconds(10), 2, 0}));
  EXPECT_EQ(pilot_.GetFrame(), 3);
  EXPECT_TRUE(game.CleanupRun());
}

TEST_F(GamePilotTest, RunsNoFramesIfThePilotIsAlreadyDone) {
  pilot_.SetDone(true);
  PilotTestGame game(&pilot_, 0);
  EXPECT_TRUE(game.Run(&context_, {}));
  EXPECT_TRUE(game.InitRun());
  EXPECT_THAT(game.GetUpdates(), IsEmpty());
  EXPECT_TRUE(game.CleanupRun());
}

TEST_F(GamePilotTest, RunsNoFramesIfInitFails) {
  class FailingInitGame final : public Game {
   protected:
    bool Init(const std::vector<std::string_view>& args) override {
      return false;
    }
  };
  FailingInitGame game;
  EXPECT_FALSE(game.Run(&context_, {}));
  EXPECT_EQ(pilot_.GetFrame(), 0);
}

TEST_F(GamePilotTest, ReturnsFalseIfThePilotFailed) {
  pilot_.SetFailed(true);
  PilotTestGame game(&pilot_, 2);
  EXPECT_FALSE(game.Run(&context_, {}));
  EXPECT_THAT(game.GetUpdates(), SizeIs(2));
  EXPECT_TRUE(game.CleanupRun());
}

TEST_F(GamePilotTest, ReturnsFalseIfTheGameStopsBeforeThePilotIsDone) {
  PilotTestGame game(&pilot_, 3);
  game.SetStopAtUpdate(2);
  EXPECT_FALSE(game.Run(&context_, {}));
  EXPECT_THAT(game.GetUpdates(), SizeIs(2));
  EXPECT_EQ(pilot_.GetFrame(), 2);
  EXPECT_TRUE(game.CleanupRun());
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(Field(&LogRecorder::Message::text,
                                HasSubstr("stopped before its pilot"))));
}

TEST_F(GamePilotTest, PassesIfTheGameStopsAsThePilotIsDone) {
  PilotTestGame game(&pilot_, 2);
  game.SetStopAtUpdate(2);
  EXPECT_TRUE(game.Run(&context_, {}));
  EXPECT_THAT(game.GetUpdates(), SizeIs(2));
}

}  // namespace
}  // namespace gb
