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
      clock_->AdvanceTime(update_delay_);
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
  absl::Duration last_delta_time_;
  absl::Duration total_update_time_;
  std::vector<std::string_view> init_args_;
};

TEST(GameTest, GameIsNotAbstract) { Game game; }

TEST(GameTest, RunNoContextNoArgs) {
  TestGame game;
  EXPECT_TRUE(game.Run({}));
  EXPECT_TRUE(game.InitRun());
  EXPECT_TRUE(game.UpdateRun());
  EXPECT_TRUE(game.CleanupRun());
  EXPECT_EQ(game.Context().GetPtr<Game>(), &game);
  EXPECT_EQ(game.Context().GetValue<int>(Game::kKeyMaxFps),
            Game::kDefaultMaxFps);
  EXPECT_GE(game.GetLastDeltaTime(),
            absl::Seconds(1.0 / static_cast<double>(Game::kDefaultMaxFps)));
  EXPECT_THAT(game.GetInitArgs(), IsEmpty());
}

TEST(GameTest, RunInvalidFps) {
  Context context;
  context.SetValue<double>(Game::kKeyMaxFps, 1.0 / 60.0);
  TestGame game;
  EXPECT_FALSE(game.Run(&context, {}));
  EXPECT_FALSE(game.InitRun());
  EXPECT_FALSE(game.UpdateRun());
  EXPECT_FALSE(game.CleanupRun());
}

TEST(GameTest, RunSpecifiedFps) {
  Context context;
  context.SetValue<int>(Game::kKeyMaxFps, 30);
  TestGame game;
  EXPECT_TRUE(game.Run(&context, {}));
  EXPECT_EQ(game.Context().GetValue<int>(Game::kKeyMaxFps), 30);
  EXPECT_GE(game.GetLastDeltaTime(), absl::Seconds(1.0 / 30.0));
}

TEST(GameTest, RunNoContextWithArgs) {
  TestGame game;
  std::vector<std::string_view> args = {"one", "two", "three"};
  EXPECT_TRUE(game.Run(args));
  EXPECT_EQ(game.GetInitArgs(), args);
}

TEST(GameTest, RunNoContextWithArgcArgv) {
  TestGame game;
  std::vector<std::string_view> args = {"one", "two", "three"};
  char* argv[] = {
      "executable-name",
      const_cast<char*>(args[0].data()),
      const_cast<char*>(args[1].data()),
      const_cast<char*>(args[2].data()),
  };
  EXPECT_TRUE(game.Run(4, argv));
  EXPECT_EQ(game.GetInitArgs(), args);
}

TEST(GameTest, RunWithContextWithArgs) {
  Context context;
  context.SetValue<int>(Game::kKeyMaxFps, 30);
  TestGame game;
  std::vector<std::string_view> args = {"one", "two", "three"};
  EXPECT_TRUE(game.Run(&context, args));
  EXPECT_EQ(game.Context().GetValue<int>(Game::kKeyMaxFps), 30);
  EXPECT_EQ(game.GetInitArgs(), args);
}

TEST(GameTest, RunWithContextWithArgcArgv) {
  Context context;
  context.SetValue<int>(Game::kKeyMaxFps, 30);
  TestGame game;
  std::vector<std::string_view> args = {"one", "two", "three"};
  char* argv[] = {
      "executable-name",
      const_cast<char*>(args[0].data()),
      const_cast<char*>(args[1].data()),
      const_cast<char*>(args[2].data()),
  };
  EXPECT_TRUE(game.Run(&context, 4, argv));
  EXPECT_EQ(game.Context().GetValue<int>(Game::kKeyMaxFps), 30);
  EXPECT_EQ(game.GetInitArgs(), args);
}

TEST(GameTest, RunWithInitFailure) {
  TestGame game;
  game.SetInitResult(false);
  EXPECT_FALSE(game.Run({}));
  EXPECT_TRUE(game.InitRun());
  EXPECT_FALSE(game.UpdateRun());
  EXPECT_TRUE(game.CleanupRun());
}

TEST(GameTest, GameTimesDoNotDrift) {
  // Pick two prime numbers that are never going to line up perfecting to 10
  // millisecond frame times.
  FakeClock clock;
  clock.SetAutoAdvance(absl::Microseconds(3001));
  clock.SetSleepOffset(absl::Microseconds(1009));
  const absl::Duration max_drift = absl::Milliseconds(10);

  Context context;
  context.SetPtr<Clock>(&clock);
  context.SetValue<int>(Game::kKeyMaxFps, 100);

  // Run for enough time to detect drift.
  TestGame game;
  const absl::Duration expected_duration = absl::Minutes(1);
  game.SetUpdateCount(6000);  // One minutes at 100 fps

  const absl::Time start = clock.GetTime();
  EXPECT_TRUE(game.Run(&context, {}));
  const absl::Duration time = clock.GetTime() - start;
  EXPECT_GE(time, expected_duration);
  EXPECT_LE(time, expected_duration + max_drift);
  EXPECT_GE(game.GetTotalUpdateTime(), expected_duration);
  EXPECT_LE(game.GetTotalUpdateTime(), expected_duration + max_drift);
}

TEST(GameTest, GameFrameRateSlows) {
  FakeClock clock;
  clock.SetAutoAdvance(absl::Microseconds(10));
  const absl::Duration max_drift = absl::Milliseconds(10);

  Context context;
  context.SetPtr<Clock>(&clock);
  context.SetValue<int>(Game::kKeyMaxFps, 100);

  TestGame game(&clock);
  const absl::Duration expected_duration =
      absl::Seconds(100) + absl::Milliseconds(10);
  const absl::Duration update_delay = absl::Seconds(1);
  game.SetUpdateDelay(update_delay);
  game.SetUpdateCount(100);

  const absl::Time start = clock.GetTime();
  EXPECT_TRUE(game.Run(&context, {}));
  const absl::Duration time = clock.GetTime() - start;
  EXPECT_GE(time, expected_duration);
  EXPECT_LE(time, expected_duration + max_drift);
  EXPECT_GE(game.GetTotalUpdateTime(), expected_duration - update_delay);
  EXPECT_LE(game.GetTotalUpdateTime(),
            expected_duration - update_delay + max_drift);
}

}  // namespace
}  // namespace gb
