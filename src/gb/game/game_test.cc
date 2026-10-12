// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/game/game.h"

#include "gb/base/fake_clock.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::IsEmpty;

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

}  // namespace
}  // namespace gb
