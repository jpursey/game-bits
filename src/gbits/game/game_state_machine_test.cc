#include "gbits/game/game_state_machine.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

struct TestStateInfo {
  GameState* current_state = nullptr;
  int construct_count = 0;
  int destruct_count = 0;
  int update_count = 0;
  int enter_count = 0;
  int exit_count = 0;
  int child_enter_count = 0;
  int child_exit_count = 0;
  absl::Duration update_time;
  GameStateId last_child_enter_id = kNoGameState;
  GameStateId last_child_exit_id = kNoGameState;
};

template <typename DerivedState>
class TestState : public GameState {
 public:
  static void Reset() { Info() = {}; }
  static TestStateInfo& Info() { 
    static TestStateInfo info = {};
    return info; 
  }

  explicit TestState() {
    Info().current_state = this;
    Info().construct_count += 1;
  }
  ~TestState() override {
    Info().current_state = nullptr;
    Info().destruct_count += 1;
  }

  ValidatedContext* GetContext() { return &Context(); }

 protected:
  void OnUpdate(absl::Duration delta_time) override {
    Info().update_count += 1;
    Info().update_time += delta_time;
  }
  void OnEnter() override { Info().enter_count += 1; }
  void OnExit() override { Info().exit_count += 1; }
  void OnChildEnter(GameStateId child) {
    Info().child_enter_count += 1;
    Info().last_child_enter_id = child;
  }
  void OnChildExit(GameStateId child) {
    Info().child_exit_count += 1;
    Info().last_child_exit_id = child;
  }

 private:
  static TestStateInfo info_;
};

struct DefaultState : TestState<DefaultState> {};

struct TopStateA : TestState<TopStateA> {
  using ParentStates = NoGameStates;
};

struct TopStateB : TestState<TopStateB> {
  using ParentStates = NoGameStates;
};

struct ChildStateA : TestState<ChildStateA> {
  using ParentStates = GameStates<TopStateA, TopStateB>;
};

struct ChildStateB : TestState<ChildStateB> {
  using ParentStates = GameStates<TopStateA, TopStateB>;
};

struct GlobalState : TestState<GlobalState> {
  using ParentStates = AllGameStates;
  using Lifetime = GlobalGameStateLifetime;
};

struct ActiveState : TestState<ActiveState> {
  using ParentStates = AllGameStates;
  using Lifetime = ActiveGameStateLifetime;
};

TEST(GameStateMachineTest, DefaultConstruct) {
  auto state_machine = GameStateMachine::Create();
  ASSERT_NE(state_machine, nullptr);
  EXPECT_EQ(state_machine->GetTopState(), nullptr);
}

TEST(GameStateMachineTest, RegisterDefaultLifetime) {
  DefaultState::Reset();
  auto state_machine = GameStateMachine::Create();

  state_machine->Register<DefaultState>();
  EXPECT_EQ(DefaultState::Info().construct_count, 1);
  EXPECT_EQ(DefaultState::Info().destruct_count, 0);

  state_machine.reset();
  EXPECT_EQ(DefaultState::Info().construct_count, 1);
  EXPECT_EQ(DefaultState::Info().destruct_count, 1);
}

TEST(GameStateMachineTest, RegisterGlobalLifetime) {
  GlobalState::Reset();
  auto state_machine = GameStateMachine::Create();

  state_machine->Register<GlobalState>();
  EXPECT_EQ(GlobalState::Info().construct_count, 1);
  EXPECT_EQ(GlobalState::Info().destruct_count, 0);

  state_machine.reset();
  EXPECT_EQ(GlobalState::Info().construct_count, 1);
  EXPECT_EQ(GlobalState::Info().destruct_count, 1);
}

TEST(GameStateMachineTest, RegisterActiveLifetime) {
  ActiveState::Reset();
  auto state_machine = GameStateMachine::Create();

  state_machine->Register<ActiveState>();
  EXPECT_EQ(ActiveState::Info().construct_count, 0);
  EXPECT_EQ(ActiveState::Info().destruct_count, 0);

  state_machine.reset();
  EXPECT_EQ(ActiveState::Info().construct_count, 0);
  EXPECT_EQ(ActiveState::Info().destruct_count, 0);
}

TEST(GameStateMachineTest, UpdateInactiveState) {
  DefaultState::Reset();
  auto state_machine = GameStateMachine::Create();
  state_machine->Register<DefaultState>();
  state_machine->Update(absl::Milliseconds(1));
  EXPECT_EQ(DefaultState::Info().update_count, 0);
}

TEST(GameStateMachineTest, ChangeTopStateNoUpdate) {
  DefaultState::Reset();
  auto state_machine = GameStateMachine::Create();
  state_machine->Register<DefaultState>();
  EXPECT_TRUE(state_machine->ChangeTopState<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
}

TEST(GameStateMachineTest, ChangeTopStateInvalidState) {
  DefaultState::Reset();
  auto state_machine = GameStateMachine::Create();
  EXPECT_FALSE(state_machine->ChangeTopState<DefaultState>());
  state_machine->Update(absl::Milliseconds(1));
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
}

TEST(GameStateMachineTest, ChangeTopState) {
  DefaultState::Reset();
  auto state_machine = GameStateMachine::Create();
  state_machine->Register<DefaultState>();
  EXPECT_TRUE(state_machine->ChangeTopState<DefaultState>());
  state_machine->Update(absl::Milliseconds(1));
  EXPECT_EQ(DefaultState::Info().update_count, 1);
  EXPECT_EQ(DefaultState::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(DefaultState::Info().enter_count, 1);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
}

TEST(GameStateMachineTest, ChangeTopStateTwice) {
  TopStateA::Reset();
  TopStateB::Reset();
  auto state_machine = GameStateMachine::Create();
  state_machine->Register<TopStateA>();
  state_machine->Register<TopStateB>();
  EXPECT_TRUE(state_machine->ChangeTopState<TopStateA>());
  EXPECT_TRUE(state_machine->ChangeTopState<TopStateB>());
  state_machine->Update(absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().update_count, 0);
  EXPECT_EQ(TopStateA::Info().enter_count, 0);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
}

TEST(GameStateMachineTest, ChangeToInvalidStateDoesNotStopPreviousChange) {
  TopStateA::Reset();
  TopStateB::Reset();
  auto state_machine = GameStateMachine::Create();
  state_machine->Register<TopStateA>();
  EXPECT_TRUE(state_machine->ChangeTopState<TopStateA>());
  EXPECT_FALSE(state_machine->ChangeTopState<TopStateB>());
  state_machine->Update(absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_EQ(TopStateB::Info().update_count, 0);
  EXPECT_EQ(TopStateB::Info().enter_count, 0);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
}

}  // namespace
}  // namespace gb
