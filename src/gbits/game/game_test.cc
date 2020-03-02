#include "gbits/game/game.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::IsEmpty;

class TestGame : public Game {
 public:
  TestGame() = default;
  ~TestGame() override = default;

  void SetInitResult(bool init_result) { init_result_ = init_result; }

  const std::vector<std::string_view>& GetInitArgs() const {
    return init_args_;
  }
  absl::Duration GetLastDeltaTime() const { return last_delta_time_; }
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
    return false;
  }

  void CleanUp() override { cleanup_run_ = true; }

 private:
  bool init_result_ = true;
  bool init_run_ = false;
  bool update_run_ = false;
  bool cleanup_run_ = false;
  absl::Duration last_delta_time_;
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

}  // namespace
}  // namespace gb
