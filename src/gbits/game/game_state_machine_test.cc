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
  static GameState*& Instance() {
    static GameState* instance = nullptr;
    return instance;
  }

  explicit TestState() {
    Info().current_state = this;
    Info().construct_count += 1;
    EXPECT_EQ(Instance(), nullptr) << GetGameStateName<DerivedState>()
                                   << " has multiple instances";
    Instance() = this;
  }
  ~TestState() override {
    EXPECT_EQ(Instance(), this) << GetGameStateName<DerivedState>()
                                << " destructing mismatched instance";
    Instance() = nullptr;
    Info().current_state = nullptr;
    Info().destruct_count += 1;
  }

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

class GameStateMachineTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    // Force names for nicer output.
    SetGameStateName<DefaultState>("DefaultState");
    SetGameStateName<TopStateA>("TopStateA");
    SetGameStateName<TopStateB>("TopStateB");
    SetGameStateName<ChildStateA>("ChildStateA");
    SetGameStateName<ChildStateB>("ChildStateB");
    SetGameStateName<GlobalState>("GlobalState");
    SetGameStateName<ActiveState>("ActiveState");
  }

  GameStateMachineTest() {
    state_machine_ = GameStateMachine::Create();
    state_machine_->SetTraceLevel(GameStateTraceLevel::kVerbose);
    state_machine_->SetTraceHandler([this](const GameStateTrace& trace) {
      if (trace.IsError()) {
        ++error_count_;
        last_error_ = trace;
      }
      traces_.push_back(trace);
    });
  }

  void ExpectTraceMatches(int index, GameStateTraceType type,
                          GameStateId parent, GameStateId state) {
    if (index > static_cast<int>(traces_.size())) {
      EXPECT_LT(index, static_cast<int>(traces_.size()));
      return;
    }
    const auto& trace = traces_[index];
    EXPECT_EQ(ToString(trace.type), ToString(type))
        << std::to_string(index) << ": " << ToString(traces_[index]);
    if (trace.type != type) {
      return;
    }
    EXPECT_EQ(GetGameStateName(trace.parent), GetGameStateName(parent))
        << std::to_string(index) << ": " << ToString(traces_[index]);
    if (trace.parent != parent) {
      return;
    }
    EXPECT_EQ(GetGameStateName(trace.state), GetGameStateName(state))
        << std::to_string(index) << ": " << ToString(traces_[index]);
  }
  void ExpectTraceMatches(int index, GameStateTraceType type,
                          GameStateId state) {
    ExpectTraceMatches(index, type, kNoGameState, state);
  }

  int error_count_ = 0;
  GameStateTrace last_error_;
  std::vector<GameStateTrace> traces_;
  std::unique_ptr<GameStateMachine> state_machine_;
};

TEST_F(GameStateMachineTest, DefaultConstruct) {
  EXPECT_EQ(state_machine_->GetTopState(), nullptr);
}

TEST_F(GameStateMachineTest, RegisterDefaultLifetime) {
  DefaultState::Reset();

  state_machine_->Register<DefaultState>();
  EXPECT_EQ(DefaultState::Info().construct_count, 1);
  EXPECT_EQ(DefaultState::Info().destruct_count, 0);
  EXPECT_NE(state_machine_->GetState<DefaultState>(), nullptr);
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());

  state_machine_.reset();
  EXPECT_EQ(DefaultState::Info().construct_count, 1);
  EXPECT_EQ(DefaultState::Info().destruct_count, 1);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(GameStateMachineTest, RegisterGlobalLifetime) {
  GlobalState::Reset();

  state_machine_->Register<GlobalState>();
  EXPECT_EQ(GlobalState::Info().construct_count, 1);
  EXPECT_EQ(GlobalState::Info().destruct_count, 0);
  EXPECT_NE(state_machine_->GetState<GlobalState>(), nullptr);
  EXPECT_FALSE(state_machine_->IsActive<GlobalState>());

  state_machine_.reset();
  EXPECT_EQ(GlobalState::Info().construct_count, 1);
  EXPECT_EQ(GlobalState::Info().destruct_count, 1);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(GameStateMachineTest, RegisterActiveLifetime) {
  ActiveState::Reset();

  state_machine_->Register<ActiveState>();
  EXPECT_EQ(ActiveState::Info().construct_count, 0);
  EXPECT_EQ(ActiveState::Info().destruct_count, 0);
  EXPECT_EQ(state_machine_->GetState<ActiveState>(), nullptr);
  EXPECT_FALSE(state_machine_->IsActive<ActiveState>());

  state_machine_.reset();
  EXPECT_EQ(ActiveState::Info().construct_count, 0);
  EXPECT_EQ(ActiveState::Info().destruct_count, 0);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(GameStateMachineTest, UpdateInactiveState) {
  DefaultState::Reset();

  state_machine_->Register<DefaultState>();
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(GameStateMachineTest, ChangeTopStateNoUpdate) {
  DefaultState::Reset();

  state_machine_->Register<DefaultState>();
  EXPECT_TRUE(state_machine_->ChangeTopState<DefaultState>());
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  EXPECT_EQ(traces_.size(), 1);
  ExpectTraceMatches(0, GameStateTraceType::kRequestChange,
                     GetGameStateId<DefaultState>());
}

TEST_F(GameStateMachineTest, ChangeTopStateInvalidState) {
  DefaultState::Reset();

  EXPECT_FALSE(state_machine_->ChangeTopState<DefaultState>());
  EXPECT_EQ(error_count_, 1);
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  EXPECT_EQ(traces_.size(), 1);
  ExpectTraceMatches(0, GameStateTraceType::kInvalidState,
                     GetGameStateId<DefaultState>());
}

TEST_F(GameStateMachineTest, ChangeTopState) {
  DefaultState::Reset();

  state_machine_->Register<DefaultState>();
  EXPECT_TRUE(state_machine_->ChangeTopState<DefaultState>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(state_machine_->GetTopState(),
            state_machine_->GetState<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 1);
  EXPECT_EQ(DefaultState::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(DefaultState::Info().enter_count, 1);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  EXPECT_EQ(traces_.size(), 3);
  ExpectTraceMatches(0, GameStateTraceType::kRequestChange,
                     GetGameStateId<DefaultState>());
  ExpectTraceMatches(1, GameStateTraceType::kOnEnter,
                     GetGameStateId<DefaultState>());
  ExpectTraceMatches(2, GameStateTraceType::kOnUpdate,
                     GetGameStateId<DefaultState>());
}

TEST_F(GameStateMachineTest, ChangeTopStateTwice) {
  TopStateA::Reset();
  TopStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  EXPECT_TRUE(state_machine_->ChangeTopState<TopStateA>());
  EXPECT_TRUE(state_machine_->ChangeTopState<TopStateB>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 0);
  EXPECT_EQ(TopStateA::Info().enter_count, 0);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(state_machine_->GetTopState(),
            state_machine_->GetState<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  EXPECT_EQ(traces_.size(), 5);
  ExpectTraceMatches(0, GameStateTraceType::kRequestChange,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(1, GameStateTraceType::kAbortChange,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(2, GameStateTraceType::kRequestChange,
                     GetGameStateId<TopStateB>());
  ExpectTraceMatches(3, GameStateTraceType::kOnEnter,
                     GetGameStateId<TopStateB>());
  ExpectTraceMatches(4, GameStateTraceType::kOnUpdate,
                     GetGameStateId<TopStateB>());
}

TEST_F(GameStateMachineTest, ChangeToInvalidStateDoesNotStopPreviousChange) {
  TopStateA::Reset();
  TopStateB::Reset();

  state_machine_->Register<TopStateA>();
  EXPECT_TRUE(state_machine_->ChangeTopState<TopStateA>());
  EXPECT_FALSE(state_machine_->ChangeTopState<TopStateB>());
  EXPECT_EQ(error_count_, 1);
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(state_machine_->GetTopState(),
            state_machine_->GetState<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_FALSE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 0);
  EXPECT_EQ(TopStateB::Info().enter_count, 0);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  EXPECT_EQ(traces_.size(), 4);
  ExpectTraceMatches(0, GameStateTraceType::kRequestChange,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(1, GameStateTraceType::kInvalidState,
                     GetGameStateId<TopStateB>());
  ExpectTraceMatches(2, GameStateTraceType::kOnEnter,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(3, GameStateTraceType::kOnUpdate,
                     GetGameStateId<TopStateA>());
}

TEST_F(GameStateMachineTest, ChangeBetweenTopStates) {
  TopStateA::Reset();
  TopStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  EXPECT_TRUE(state_machine_->ChangeTopState<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeTopState<TopStateB>());
  state_machine_->Update(absl::Milliseconds(2));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(state_machine_->GetTopState(),
            state_machine_->GetState<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(2));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  EXPECT_EQ(traces_.size(), 7);
  ExpectTraceMatches(0, GameStateTraceType::kRequestChange,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(1, GameStateTraceType::kOnEnter,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(2, GameStateTraceType::kOnUpdate,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(3, GameStateTraceType::kRequestChange,
                     GetGameStateId<TopStateB>());
  ExpectTraceMatches(4, GameStateTraceType::kOnExit,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(5, GameStateTraceType::kOnEnter,
                     GetGameStateId<TopStateB>());
  ExpectTraceMatches(6, GameStateTraceType::kOnUpdate,
                     GetGameStateId<TopStateB>());
}

TEST_F(GameStateMachineTest, ExitTopState) {
  TopStateA::Reset();

  state_machine_->Register<TopStateA>();
  EXPECT_TRUE(state_machine_->ChangeTopState<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeTopState(kNoGameState));
  state_machine_->Update(absl::Milliseconds(2));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(state_machine_->GetTopState(), nullptr);
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_EQ(traces_.size(), 5);
  ExpectTraceMatches(0, GameStateTraceType::kRequestChange,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(1, GameStateTraceType::kOnEnter,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(2, GameStateTraceType::kOnUpdate,
                     GetGameStateId<TopStateA>());
  ExpectTraceMatches(3, GameStateTraceType::kRequestChange, kNoGameState);
  ExpectTraceMatches(4, GameStateTraceType::kOnExit,
                     GetGameStateId<TopStateA>());
}

TEST_F(GameStateMachineTest, ExitTopStateThatDoesNotExist) {
  EXPECT_TRUE(state_machine_->ChangeTopState(kNoGameState));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_EQ(traces_.size(), 1);
  ExpectTraceMatches(0, GameStateTraceType::kRequestChange, kNoGameState);
}

}  // namespace
}  // namespace gb
