#include "gbits/game/game_state_machine.h"

#include "gbits/test/thread_tester.h"
#include "glog/logging.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

struct TestStateInfo {
  GameState* current_state = nullptr;
  int construct_count = 0;
  int destruct_count = 0;
  int init_count = 0;
  int update_count = 0;
  int enter_count = 0;
  int exit_count = 0;
  int child_enter_count = 0;
  int child_exit_count = 0;
  absl::Duration update_time;
  GameStateId last_child_enter_id = kNoGameStateId;
  GameStateId last_child_exit_id = kNoGameStateId;
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
    EXPECT_EQ(Instance(), nullptr)
        << GetGameStateName<DerivedState>() << " has multiple instances";
    Instance() = this;
  }
  ~TestState() override {
    EXPECT_EQ(Instance(), this) << GetGameStateName<DerivedState>()
                                << " destructing mismatched instance";
    Instance() = nullptr;
    Info().current_state = nullptr;
    Info().destruct_count += 1;
    EXPECT_EQ(GetId(), GetGameStateId<DerivedState>());
    EXPECT_NE(GetStateMachine(), nullptr);
    EXPECT_EQ(GetParentId(), kNoGameStateId);
    EXPECT_EQ(GetParent(), nullptr);
    EXPECT_EQ(GetChildId(), kNoGameStateId);
    EXPECT_EQ(GetChild(), nullptr);
  }

  void QueueChangeState(GameStateTraceType event, GameStateId parent,
                        GameStateId state) {
    event_ = event;
    new_parent_ = parent;
    new_state_ = state;
  }

 protected:
  void OnInit() override {
    Info().init_count += 1;
    EXPECT_EQ(GetId(), GetGameStateId<DerivedState>());
    EXPECT_NE(GetStateMachine(), nullptr);
    EXPECT_EQ(GetParentId(), kNoGameStateId);
    EXPECT_EQ(GetParent(), nullptr);
    EXPECT_EQ(GetChildId(), kNoGameStateId);
    EXPECT_EQ(GetChild(), nullptr);
  }
  void OnUpdate(absl::Duration delta_time) override {
    Info().update_count += 1;
    Info().update_time += delta_time;
    if (event_ == GameStateTraceType::kOnUpdate) {
      GetStateMachine()->ChangeState(new_parent_, new_state_);
    }
  }
  void OnEnter() override {
    Info().enter_count += 1;
    if (event_ == GameStateTraceType::kOnEnter) {
      GetStateMachine()->ChangeState(new_parent_, new_state_);
    }
  }
  void OnExit() override {
    Info().exit_count += 1;
    if (event_ == GameStateTraceType::kOnExit) {
      GetStateMachine()->ChangeState(new_parent_, new_state_);
    }
  }
  void OnChildEnter(GameStateId child) {
    Info().child_enter_count += 1;
    Info().last_child_enter_id = child;
    if (event_ == GameStateTraceType::kOnChildEnter) {
      GetStateMachine()->ChangeState(new_parent_, new_state_);
    }
  }
  void OnChildExit(GameStateId child) {
    Info().child_exit_count += 1;
    Info().last_child_exit_id = child;
    if (event_ == GameStateTraceType::kOnChildExit) {
      GetStateMachine()->ChangeState(new_parent_, new_state_);
    }
  }

 private:
  GameStateTraceType event_ = GameStateTraceType::kUnknown;
  GameStateId new_parent_ = kNoGameStateId;
  GameStateId new_state_ = kNoGameStateId;
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
struct SiblingStateB;
struct SiblingStateA : TestState<SiblingStateA> {
  using SiblingStates = GameStates<SiblingStateB>;
};
struct SiblingStateB : TestState<SiblingStateB> {
  using SiblingStates = GameStates<SiblingStateA>;
};
struct GlobalState : TestState<GlobalState> {
  using Lifetime = GlobalGameStateLifetime;
};
struct ActiveState : TestState<ActiveState> {
  using Lifetime = ActiveGameStateLifetime;
};
struct AllParentsState : TestState<AllParentsState> {
  using ParentStates = AllGameStates;
};
struct InputContextState : TestState<InputContextState> {
  static GB_CONTEXT_CONSTRAINT(kRequiredString, kInRequired, std::string);
  static inline constexpr int kDefaultInt = 42;
  static GB_CONTEXT_CONSTRAINT_DEFAULT(kOptionalInt, kInOptional, int,
                                       kDefaultInt);
  using Contract = ContextContract<kRequiredString, kOptionalInt>;
  using Lifetime = ActiveGameStateLifetime;
};
struct OutputContextState : TestState<OutputContextState> {
  static GB_CONTEXT_CONSTRAINT(kRequiredString, kOutRequired, std::string);
  static inline constexpr int kDefaultInt = 24;
  static GB_CONTEXT_CONSTRAINT_DEFAULT(kOptionalInt, kOutOptional, int,
                                       kDefaultInt);
  static GB_CONTEXT_CONSTRAINT(kScopedState, kScoped, GameState);
  using Contract = ContextContract<kRequiredString, kOptionalInt, kScopedState>;
  using Lifetime = ActiveGameStateLifetime;
};

class GameStateMachineTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    // Force names for nicer error output and to test lookup by name.
    SetGameStateName<DefaultState>("DefaultState");
    SetGameStateName<TopStateA>("TopStateA");
    SetGameStateName<TopStateB>("TopStateB");
    SetGameStateName<ChildStateA>("ChildStateA");
    SetGameStateName<ChildStateB>("ChildStateB");
    SetGameStateName<SiblingStateA>("SiblingStateA");
    SetGameStateName<SiblingStateB>("SiblingStateB");
    SetGameStateName<GlobalState>("GlobalState");
    SetGameStateName<ActiveState>("ActiveState");
    SetGameStateName<AllParentsState>("AllParentsState");
    SetGameStateName<InputContextState>("InputContextState");
    SetGameStateName<OutputContextState>("OutputContextState");
  }

  GameStateMachineTest() {
    state_machine_ = GameStateMachine::Create(&context_);
    state_machine_->DisableLogging();
    state_machine_->SetTraceLevel(GameStateTraceLevel::kVerbose);
    state_machine_->SetTraceHandler([this](const GameStateTrace& trace) {
      if (trace.IsError()) {
        ++error_count_;
        last_error_ = trace;
      }
      traces_.push_back(trace);
    });
  }

  struct TraceMatchInfo {
    TraceMatchInfo() = default;
    TraceMatchInfo(GameStateTraceType type, GameStateId parent,
                   GameStateId state)
        : type(type), parent(parent), state(state) {}
    TraceMatchInfo(GameStateTraceType type, GameStateId state)
        : type(type), state(state) {}
    GameStateTraceType type = GameStateTraceType::kUnknown;
    GameStateId parent = kNoGameStateId;
    GameStateId state = kNoGameStateId;
  };

  void MatchTrace(std::vector<TraceMatchInfo> trace_matches) {
    EXPECT_EQ(trace_matches.size(), traces_.size());
    int size = static_cast<int>(std::min(trace_matches.size(), traces_.size()));
    for (int index = 0; index < size; ++index) {
      const auto& match = trace_matches[index];
      const auto& trace = traces_[index];
      EXPECT_EQ(ToString(trace.type), ToString(match.type))
          << std::to_string(index) << ": " << ToString(traces_[index]);
      if (trace.type != match.type) {
        return;
      }
      EXPECT_STREQ(GetGameStateName(trace.parent),
                   GetGameStateName(match.parent))
          << std::to_string(index) << ": " << ToString(traces_[index]);
      if (trace.parent != match.parent) {
        return;
      }
      EXPECT_STREQ(GetGameStateName(trace.state), GetGameStateName(match.state))
          << std::to_string(index) << ": " << ToString(traces_[index]);
      if (trace.state != match.state) {
        return;
      }
    }
  }

  int error_count_ = 0;
  GameStateTrace last_error_;
  std::vector<GameStateTrace> traces_;
  Context context_;
  std::unique_ptr<GameStateMachine> state_machine_;
};

TEST_F(GameStateMachineTest, DefaultConstruct) {
  EXPECT_EQ(state_machine_->GetTopState(), nullptr);
}

TEST_F(GameStateMachineTest, IsRegistered) {
  TopStateA::Reset();
  TopStateB::Reset();

  EXPECT_FALSE(state_machine_->IsRegistered<TopStateA>());
  EXPECT_FALSE(state_machine_->IsRegistered(gb::GetGameStateId<TopStateB>()));
  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  EXPECT_TRUE(state_machine_->IsRegistered<TopStateA>());
  EXPECT_TRUE(state_machine_->IsRegistered(gb::GetGameStateId<TopStateB>()));
}

TEST_F(GameStateMachineTest, GetRegisteredId) {
  TopStateA::Reset();
  TopStateB::Reset();

  EXPECT_EQ(state_machine_->GetRegisteredId("TopStateA"), kNoGameStateId);
  EXPECT_EQ(state_machine_->GetRegisteredId("TopStateB"), kNoGameStateId);
  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  EXPECT_EQ(state_machine_->GetRegisteredId("TopStateA"),
            GetGameStateId<TopStateA>());
  EXPECT_EQ(state_machine_->GetRegisteredId("TopStateB"),
            GetGameStateId<TopStateB>());
}

TEST_F(GameStateMachineTest, RegisterDefaultLifetime) {
  DefaultState::Reset();

  state_machine_->Register<DefaultState>();
  EXPECT_EQ(DefaultState::Info().construct_count, 1);
  EXPECT_EQ(DefaultState::Info().init_count,
            DefaultState::Info().construct_count);
  EXPECT_EQ(DefaultState::Info().destruct_count, 0);
  EXPECT_NE(state_machine_->GetState<DefaultState>(), nullptr);
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());

  state_machine_.reset();
  EXPECT_EQ(DefaultState::Info().construct_count, 1);
  EXPECT_EQ(DefaultState::Info().init_count,
            DefaultState::Info().construct_count);
  EXPECT_EQ(DefaultState::Info().destruct_count, 1);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(GameStateMachineTest, RegisterGlobalLifetime) {
  GlobalState::Reset();

  state_machine_->Register<GlobalState>();
  EXPECT_EQ(GlobalState::Info().construct_count, 1);
  EXPECT_EQ(GlobalState::Info().init_count,
            GlobalState::Info().construct_count);
  EXPECT_EQ(GlobalState::Info().destruct_count, 0);
  EXPECT_NE(state_machine_->GetState<GlobalState>(), nullptr);
  EXPECT_FALSE(state_machine_->IsActive<GlobalState>());

  state_machine_.reset();
  EXPECT_EQ(GlobalState::Info().construct_count, 1);
  EXPECT_EQ(GlobalState::Info().init_count,
            GlobalState::Info().construct_count);
  EXPECT_EQ(GlobalState::Info().destruct_count, 1);
  EXPECT_EQ(error_count_, 0);
}

TEST_F(GameStateMachineTest, RegisterActiveLifetime) {
  ActiveState::Reset();

  state_machine_->Register<ActiveState>();
  EXPECT_EQ(ActiveState::Info().construct_count, 0);
  EXPECT_EQ(ActiveState::Info().init_count,
            ActiveState::Info().construct_count);
  EXPECT_EQ(ActiveState::Info().destruct_count, 0);
  EXPECT_EQ(state_machine_->GetState<ActiveState>(), nullptr);
  EXPECT_FALSE(state_machine_->IsActive<ActiveState>());

  state_machine_.reset();
  EXPECT_EQ(ActiveState::Info().construct_count, 0);
  EXPECT_EQ(ActiveState::Info().init_count,
            ActiveState::Info().construct_count);
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
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<DefaultState>()));
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeTopStateInvalidState) {
  DefaultState::Reset();

  EXPECT_FALSE(state_machine_->ChangeState(kNoGameStateId,
                                           GetGameStateId<DefaultState>()));
  EXPECT_EQ(error_count_, 1);
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kInvalidChangeState, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeTopState) {
  DefaultState::Reset();

  state_machine_->Register<DefaultState>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<DefaultState>()));
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
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeTopStateInvalidSibling) {
  DefaultState::Reset();
  SiblingStateA::Reset();

  state_machine_->Register<DefaultState>();
  state_machine_->Register<SiblingStateA>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<SiblingStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->ChangeState(kNoGameStateId,
                                           GetGameStateId<DefaultState>()));
  EXPECT_TRUE(state_machine_->IsActive<SiblingStateA>());
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kInvalidChangeSibling,
       GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeTopStateValidSibling) {
  SiblingStateA::Reset();
  SiblingStateB::Reset();

  state_machine_->Register<SiblingStateA>();
  state_machine_->Register<SiblingStateB>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<SiblingStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<SiblingStateB>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<SiblingStateA>());
  EXPECT_TRUE(state_machine_->IsActive<SiblingStateB>());
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<SiblingStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<SiblingStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<SiblingStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<SiblingStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<SiblingStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeTopStateToStateWithSiblingRestrictions) {
  DefaultState::Reset();
  SiblingStateB::Reset();

  state_machine_->Register<DefaultState>();
  state_machine_->Register<SiblingStateB>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<DefaultState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<SiblingStateB>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  EXPECT_TRUE(state_machine_->IsActive<SiblingStateB>());
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<SiblingStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<SiblingStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<SiblingStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<SiblingStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeToAlreadyActiveState) {
  DefaultState::Reset();

  state_machine_->Register<DefaultState>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<DefaultState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->ChangeState(kNoGameStateId,
                                           GetGameStateId<DefaultState>()));
  EXPECT_TRUE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(state_machine_->GetTopState(),
            state_machine_->GetState<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 1);
  EXPECT_EQ(DefaultState::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(DefaultState::Info().enter_count, 1);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kInvalidChangeState, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeTopStateTwice) {
  TopStateA::Reset();
  TopStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateB>()));
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
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kAbortChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeToInvalidStateDoesNotStopPreviousChange) {
  TopStateA::Reset();
  TopStateB::Reset();

  state_machine_->Register<TopStateA>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  EXPECT_FALSE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateB>()));
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
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kInvalidChangeState, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
  });
}

TEST_F(GameStateMachineTest, ChangeBetweenTopStates) {
  TopStateA::Reset();
  TopStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateB>()));
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
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ExitTopState) {
  TopStateA::Reset();

  state_machine_->Register<TopStateA>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId, kNoGameStateId));
  state_machine_->Update(absl::Milliseconds(2));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(state_machine_->GetTopState(), nullptr);
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, kNoGameStateId},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, kNoGameStateId},
  });
}

TEST_F(GameStateMachineTest, ExitTopStateThatDoesNotExist) {
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId, kNoGameStateId));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_EQ(traces_.size(), 0);
}

TEST_F(GameStateMachineTest, EnterActiveLifetimeState) {
  ActiveState::Reset();

  state_machine_->Register<ActiveState>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<ActiveState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_NE(state_machine_->GetState<ActiveState>(), nullptr);
  EXPECT_EQ(state_machine_->GetState<ActiveState>(), ActiveState::Instance());
  EXPECT_EQ(ActiveState::Info().construct_count, 1);
  EXPECT_EQ(ActiveState::Info().init_count,
            ActiveState::Info().construct_count);
}

TEST_F(GameStateMachineTest, ExitActiveLifetimeState) {
  ActiveState::Reset();

  state_machine_->Register<ActiveState>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<ActiveState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId, kNoGameStateId));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_EQ(state_machine_->GetState<ActiveState>(), nullptr);
  EXPECT_EQ(ActiveState::Instance(), nullptr);
  EXPECT_EQ(ActiveState::Info().destruct_count, 1);
}

TEST_F(GameStateMachineTest, ChangeToUndefinedParent) {
  AllParentsState::Reset();

  state_machine_->Register<AllParentsState>();
  EXPECT_FALSE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                           GetGameStateId<AllParentsState>()));
  EXPECT_EQ(error_count_, 1);
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<AllParentsState>());
  MatchTrace({
      {GameStateTraceType::kInvalidChangeParent, GetGameStateId<TopStateA>(),
       GetGameStateId<AllParentsState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeToInactiveParent) {
  TopStateA::Reset();
  AllParentsState::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<AllParentsState>();
  EXPECT_FALSE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                           GetGameStateId<AllParentsState>()));
  EXPECT_EQ(error_count_, 1);
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_FALSE(state_machine_->IsActive<AllParentsState>());
  MatchTrace({
      {GameStateTraceType::kInvalidChangeParent, GetGameStateId<TopStateA>(),
       GetGameStateId<AllParentsState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeTopOnlyStateToParent) {
  DefaultState::Reset();
  TopStateA::Reset();

  state_machine_->Register<DefaultState>();
  state_machine_->Register<TopStateA>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<DefaultState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->ChangeState(GetGameStateId<DefaultState>(),
                                           GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<DefaultState>());
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kInvalidChangeParent, GetGameStateId<DefaultState>(),
       GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateToInvalidParent) {
  DefaultState::Reset();
  ChildStateA::Reset();

  state_machine_->Register<DefaultState>();
  state_machine_->Register<ChildStateA>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<DefaultState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->ChangeState(GetGameStateId<DefaultState>(),
                                           GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<DefaultState>());
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kInvalidChangeParent, GetGameStateId<DefaultState>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, AddChildState) {
  TopStateA::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 2);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(2));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 1);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
  });
}

TEST_F(GameStateMachineTest, ExitChildState) {
  TopStateA::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(
      state_machine_->ChangeState(GetGameStateId<TopStateA>(), kNoGameStateId));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 3);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(3));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 1);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       kNoGameStateId},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       kNoGameStateId},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
  });
}

TEST_F(GameStateMachineTest, ChangeChildStates) {
  TopStateA::Reset();
  ChildStateA::Reset();
  ChildStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<ChildStateB>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateB>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 3);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(3));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 2);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 1);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<ChildStateB>());
  EXPECT_EQ(ChildStateB::Info().update_count, 1);
  EXPECT_EQ(ChildStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(ChildStateB::Info().enter_count, 1);
  EXPECT_EQ(ChildStateB::Info().exit_count, 0);
  EXPECT_EQ(ChildStateB::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateB::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateB>()},
  });
}

TEST_F(GameStateMachineTest, ExitChildStateWithChildren) {
  TopStateA::Reset();
  ChildStateA::Reset();
  AllParentsState::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<AllParentsState>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<ChildStateA>(),
                                          GetGameStateId<AllParentsState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(
      state_machine_->ChangeState(GetGameStateId<TopStateA>(), kNoGameStateId));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 4);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(4));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 2);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::Milliseconds(2));
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<AllParentsState>());
  EXPECT_EQ(AllParentsState::Info().update_count, 1);
  EXPECT_EQ(AllParentsState::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(AllParentsState::Info().enter_count, 1);
  EXPECT_EQ(AllParentsState::Info().exit_count, 1);
  EXPECT_EQ(AllParentsState::Info().child_enter_count, 0);
  EXPECT_EQ(AllParentsState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<ChildStateA>(),
       GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<ChildStateA>(),
       GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<ChildStateA>(),
       GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       kNoGameStateId},
      {GameStateTraceType::kOnExit, GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<ChildStateA>(),
       GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       kNoGameStateId},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
  });
}

TEST_F(GameStateMachineTest, ChangeChildStatesWithChildren) {
  TopStateA::Reset();
  ChildStateA::Reset();
  ChildStateB::Reset();
  AllParentsState::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<ChildStateB>();
  state_machine_->Register<AllParentsState>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<ChildStateA>(),
                                          GetGameStateId<AllParentsState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateB>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 4);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(4));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 0);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 2);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 2);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::Milliseconds(2));
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<AllParentsState>());
  EXPECT_EQ(AllParentsState::Info().update_count, 1);
  EXPECT_EQ(AllParentsState::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(AllParentsState::Info().enter_count, 1);
  EXPECT_EQ(AllParentsState::Info().exit_count, 1);
  EXPECT_EQ(AllParentsState::Info().child_enter_count, 0);
  EXPECT_EQ(AllParentsState::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<ChildStateB>());
  EXPECT_EQ(ChildStateB::Info().update_count, 1);
  EXPECT_EQ(ChildStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(ChildStateB::Info().enter_count, 1);
  EXPECT_EQ(ChildStateB::Info().exit_count, 0);
  EXPECT_EQ(ChildStateB::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateB::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<ChildStateA>(),
       GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<ChildStateA>(),
       GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<ChildStateA>(),
       GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<ChildStateA>(),
       GetGameStateId<AllParentsState>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateWithChildrenToTopState) {
  TopStateA::Reset();
  TopStateB::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->Register<ChildStateA>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateB>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 2);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(2));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 1);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, kNoGameStateId,
       GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, kNoGameStateId,
       GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateDuringUpdate) {
  TopStateA::Reset();
  TopStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->GetState<TopStateA>()->QueueChangeState(
      GameStateTraceType::kOnUpdate, kNoGameStateId,
      GetGameStateId<TopStateB>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateDuringParentUpdate) {
  TopStateA::Reset();
  TopStateB::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->Register<ChildStateA>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                              GetGameStateId<ChildStateA>());
  state_machine_->GetState<TopStateA>()->QueueChangeState(
      GameStateTraceType::kOnUpdate, kNoGameStateId,
      GetGameStateId<TopStateB>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 2);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(2));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 0);
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateDuringOnEnter) {
  TopStateA::Reset();
  TopStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->GetState<TopStateA>()->QueueChangeState(
      GameStateTraceType::kOnEnter, kNoGameStateId,
      GetGameStateId<TopStateB>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 0);
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kAbortChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateDuringChildOnEnter) {
  TopStateA::Reset();
  TopStateB::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->Register<ChildStateA>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                              GetGameStateId<ChildStateA>());
  state_machine_->GetState<ChildStateA>()->QueueChangeState(
      GameStateTraceType::kOnEnter, kNoGameStateId,
      GetGameStateId<TopStateB>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 0);
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kAbortChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateDuringOnExit) {
  TopStateA::Reset();
  TopStateB::Reset();
  DefaultState::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->Register<DefaultState>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->GetState<TopStateA>()->QueueChangeState(
      GameStateTraceType::kOnExit, kNoGameStateId, GetGameStateId<TopStateB>());
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<DefaultState>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kAbortChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeToChildStateDuringOnExit) {
  TopStateA::Reset();
  ChildStateA::Reset();
  DefaultState::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<DefaultState>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->GetState<TopStateA>()->QueueChangeState(
      GameStateTraceType::kOnExit, GetGameStateId<TopStateA>(),
      GetGameStateId<ChildStateA>());
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<DefaultState>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 0);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 0);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::ZeroDuration());
  EXPECT_EQ(ChildStateA::Info().enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().exit_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 1);
  EXPECT_EQ(DefaultState::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(DefaultState::Info().enter_count, 1);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kInvalidChangeParent, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateDuringChildOnExit) {
  TopStateA::Reset();
  TopStateB::Reset();
  ChildStateA::Reset();
  DefaultState::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<DefaultState>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                              GetGameStateId<ChildStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->GetState<ChildStateA>()->QueueChangeState(
      GameStateTraceType::kOnExit, kNoGameStateId, GetGameStateId<TopStateB>());
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<DefaultState>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 2);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(2));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 1);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kAbortChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateDuringOnChildEnter) {
  TopStateA::Reset();
  TopStateB::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->Register<ChildStateA>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                              GetGameStateId<ChildStateA>());
  state_machine_->GetState<TopStateA>()->QueueChangeState(
      GameStateTraceType::kOnChildEnter, kNoGameStateId,
      GetGameStateId<TopStateB>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 1);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 0);
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kAbortChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, ChangeStateDuringOnChildExit) {
  TopStateA::Reset();
  TopStateB::Reset();
  ChildStateA::Reset();
  DefaultState::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<DefaultState>();
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                              GetGameStateId<ChildStateA>());
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_->GetState<TopStateA>()->QueueChangeState(
      GameStateTraceType::kOnChildExit, kNoGameStateId,
      GetGameStateId<TopStateB>());
  state_machine_->ChangeState(kNoGameStateId, GetGameStateId<DefaultState>());
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_EQ(TopStateA::Info().update_count, 2);
  EXPECT_EQ(TopStateA::Info().update_time, absl::Milliseconds(2));
  EXPECT_EQ(TopStateA::Info().enter_count, 1);
  EXPECT_EQ(TopStateA::Info().exit_count, 1);
  EXPECT_EQ(TopStateA::Info().child_enter_count, 1);
  EXPECT_EQ(TopStateA::Info().child_exit_count, 1);
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(ChildStateA::Info().update_count, 1);
  EXPECT_EQ(ChildStateA::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(ChildStateA::Info().enter_count, 1);
  EXPECT_EQ(ChildStateA::Info().exit_count, 1);
  EXPECT_EQ(ChildStateA::Info().child_enter_count, 0);
  EXPECT_EQ(ChildStateA::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<TopStateB>());
  EXPECT_EQ(TopStateB::Info().update_count, 1);
  EXPECT_EQ(TopStateB::Info().update_time, absl::Milliseconds(1));
  EXPECT_EQ(TopStateB::Info().enter_count, 1);
  EXPECT_EQ(TopStateB::Info().exit_count, 0);
  EXPECT_EQ(TopStateB::Info().child_enter_count, 0);
  EXPECT_EQ(TopStateB::Info().child_exit_count, 0);
  EXPECT_FALSE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().update_count, 0);
  EXPECT_EQ(DefaultState::Info().enter_count, 0);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kAbortChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateB>()},
  });
}

TEST_F(GameStateMachineTest, EnterContextValidationFails) {
  InputContextState::Reset();

  state_machine_->Register<InputContextState>();
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<InputContextState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_EQ(state_machine_->GetTopState(), nullptr);
  EXPECT_FALSE(state_machine_->IsActive<InputContextState>());
  EXPECT_EQ(InputContextState::Info().construct_count, 0);
  EXPECT_EQ(InputContextState::Info().init_count,
            InputContextState::Info().construct_count);
  EXPECT_EQ(InputContextState::Info().destruct_count, 0);
  EXPECT_EQ(InputContextState::Info().update_count, 0);
  EXPECT_EQ(InputContextState::Info().enter_count, 0);
  EXPECT_EQ(InputContextState::Info().exit_count, 0);
  EXPECT_EQ(InputContextState::Info().child_enter_count, 0);
  EXPECT_EQ(InputContextState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<InputContextState>()},
      {GameStateTraceType::kConstraintFailure,
       GetGameStateId<InputContextState>()},
      {GameStateTraceType::kAbortChange, GetGameStateId<InputContextState>()},
  });
}

TEST_F(GameStateMachineTest, EnterContextValidationSucceeds) {
  InputContextState::Reset();

  state_machine_->Register<InputContextState>();
  context_.SetValue<std::string>("foo");
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<InputContextState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_NE(state_machine_->GetTopState(), nullptr);
  EXPECT_EQ(state_machine_->GetTopState(), InputContextState::Instance());
  EXPECT_TRUE(state_machine_->IsActive<InputContextState>());
  EXPECT_EQ(InputContextState::Info().construct_count, 1);
  EXPECT_EQ(InputContextState::Info().init_count,
            InputContextState::Info().construct_count);
  EXPECT_EQ(InputContextState::Info().destruct_count, 0);
  EXPECT_EQ(InputContextState::Info().update_count, 1);
  EXPECT_EQ(InputContextState::Info().enter_count, 1);
  EXPECT_EQ(InputContextState::Info().exit_count, 0);
  EXPECT_EQ(InputContextState::Info().child_enter_count, 0);
  EXPECT_EQ(InputContextState::Info().child_exit_count, 0);
  EXPECT_EQ(context_.GetValue<int>(), InputContextState::kDefaultInt);
  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<InputContextState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<InputContextState>()},
      {GameStateTraceType::kCompleteChange,
       GetGameStateId<InputContextState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<InputContextState>()},
  });
}

TEST_F(GameStateMachineTest, ExitContextValidationFails) {
  OutputContextState::Reset();
  DefaultState::Reset();

  state_machine_->Register<OutputContextState>();
  state_machine_->Register<DefaultState>();
  EXPECT_TRUE(state_machine_->ChangeState(
      kNoGameStateId, GetGameStateId<OutputContextState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<DefaultState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_NE(state_machine_->GetTopState(), nullptr);
  EXPECT_EQ(state_machine_->GetTopState(), DefaultState::Instance());
  EXPECT_FALSE(state_machine_->IsActive<OutputContextState>());
  EXPECT_EQ(OutputContextState::Info().construct_count, 1);
  EXPECT_EQ(OutputContextState::Info().init_count,
            OutputContextState::Info().construct_count);
  EXPECT_EQ(OutputContextState::Info().destruct_count, 1);
  EXPECT_EQ(OutputContextState::Info().update_count, 1);
  EXPECT_EQ(OutputContextState::Info().enter_count, 1);
  EXPECT_EQ(OutputContextState::Info().exit_count, 1);
  EXPECT_EQ(OutputContextState::Info().child_enter_count, 0);
  EXPECT_EQ(OutputContextState::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().construct_count, 1);
  EXPECT_EQ(DefaultState::Info().init_count,
            DefaultState::Info().construct_count);
  EXPECT_EQ(DefaultState::Info().destruct_count, 0);
  EXPECT_EQ(DefaultState::Info().update_count, 1);
  EXPECT_EQ(DefaultState::Info().enter_count, 1);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  MatchTrace({
      {GameStateTraceType::kRequestChange,
       GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kCompleteChange,
       GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnExit, GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kConstraintFailure,
       GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, ExitContextValidationSucceeds) {
  OutputContextState::Reset();
  DefaultState::Reset();

  state_machine_->Register<OutputContextState>();
  state_machine_->Register<DefaultState>();
  EXPECT_TRUE(state_machine_->ChangeState(
      kNoGameStateId, GetGameStateId<OutputContextState>()));
  state_machine_->Update(absl::Milliseconds(1));
  context_.SetValue<std::string>("foo");
  context_.SetPtr<GameState>(OutputContextState::Instance());
  EXPECT_TRUE(state_machine_->ChangeState(kNoGameStateId,
                                          GetGameStateId<DefaultState>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_NE(state_machine_->GetTopState(), nullptr);
  EXPECT_EQ(state_machine_->GetTopState(), DefaultState::Instance());
  EXPECT_FALSE(state_machine_->IsActive<OutputContextState>());
  EXPECT_EQ(OutputContextState::Info().construct_count, 1);
  EXPECT_EQ(OutputContextState::Info().init_count,
            OutputContextState::Info().construct_count);
  EXPECT_EQ(OutputContextState::Info().destruct_count, 1);
  EXPECT_EQ(OutputContextState::Info().update_count, 1);
  EXPECT_EQ(OutputContextState::Info().enter_count, 1);
  EXPECT_EQ(OutputContextState::Info().exit_count, 1);
  EXPECT_EQ(OutputContextState::Info().child_enter_count, 0);
  EXPECT_EQ(OutputContextState::Info().child_exit_count, 0);
  EXPECT_TRUE(state_machine_->IsActive<DefaultState>());
  EXPECT_EQ(DefaultState::Info().construct_count, 1);
  EXPECT_EQ(DefaultState::Info().init_count,
            DefaultState::Info().construct_count);
  EXPECT_EQ(DefaultState::Info().destruct_count, 0);
  EXPECT_EQ(DefaultState::Info().update_count, 1);
  EXPECT_EQ(DefaultState::Info().enter_count, 1);
  EXPECT_EQ(DefaultState::Info().exit_count, 0);
  EXPECT_EQ(DefaultState::Info().child_enter_count, 0);
  EXPECT_EQ(DefaultState::Info().child_exit_count, 0);
  EXPECT_EQ(context_.GetValue<int>(), OutputContextState::kDefaultInt);
  EXPECT_EQ(context_.GetPtr<GameState>(), nullptr);
  MatchTrace({
      {GameStateTraceType::kRequestChange,
       GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kCompleteChange,
       GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnExit, GetGameStateId<OutputContextState>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<DefaultState>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<DefaultState>()},
  });
}

TEST_F(GameStateMachineTest, GameStateAttributesAreCorrect) {
  TopStateA::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));

  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_TRUE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(TopStateA::Instance()->GetId(), GetGameStateId<TopStateA>());
  EXPECT_EQ(TopStateA::Instance()->GetParentId(), kNoGameStateId);
  EXPECT_EQ(TopStateA::Instance()->GetParent(), nullptr);
  EXPECT_EQ(TopStateA::Instance()->GetChildId(), GetGameStateId<ChildStateA>());
  EXPECT_EQ(TopStateA::Instance()->GetChild(), ChildStateA::Instance());
  EXPECT_EQ(TopStateA::Instance()->GetStateMachine(), state_machine_.get());
  EXPECT_EQ(ChildStateA::Instance()->GetId(), GetGameStateId<ChildStateA>());
  EXPECT_EQ(ChildStateA::Instance()->GetParentId(),
            GetGameStateId<TopStateA>());
  EXPECT_EQ(ChildStateA::Instance()->GetParent(), TopStateA::Instance());
  EXPECT_EQ(ChildStateA::Instance()->GetChildId(), kNoGameStateId);
  EXPECT_EQ(ChildStateA::Instance()->GetChild(), nullptr);
  EXPECT_EQ(ChildStateA::Instance()->GetStateMachine(), state_machine_.get());

  state_machine_->ChangeState(kNoGameStateId, kNoGameStateId);
  state_machine_->Update(absl::Milliseconds(1));

  EXPECT_FALSE(state_machine_->IsActive<TopStateA>());
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_EQ(TopStateA::Instance()->GetId(), GetGameStateId<TopStateA>());
  EXPECT_EQ(TopStateA::Instance()->GetParentId(), kNoGameStateId);
  EXPECT_EQ(TopStateA::Instance()->GetParent(), nullptr);
  EXPECT_EQ(TopStateA::Instance()->GetChildId(), kNoGameStateId);
  EXPECT_EQ(TopStateA::Instance()->GetChild(), nullptr);
  EXPECT_EQ(TopStateA::Instance()->GetStateMachine(), state_machine_.get());
  EXPECT_EQ(ChildStateA::Instance()->GetId(), GetGameStateId<ChildStateA>());
  EXPECT_EQ(ChildStateA::Instance()->GetParentId(), kNoGameStateId);
  EXPECT_EQ(ChildStateA::Instance()->GetParent(), nullptr);
  EXPECT_EQ(ChildStateA::Instance()->GetChildId(), kNoGameStateId);
  EXPECT_EQ(ChildStateA::Instance()->GetChild(), nullptr);
  EXPECT_EQ(ChildStateA::Instance()->GetStateMachine(), state_machine_.get());
}

TEST_F(GameStateMachineTest, GameStateChangeChildState) {
  TopStateA::Reset();
  ChildStateA::Reset();
  ChildStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<ChildStateB>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  TopStateA::Instance()->ChangeChildState<ChildStateB>();
  state_machine_->Update(absl::Milliseconds(1));

  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_TRUE(state_machine_->IsActive<ChildStateB>());

  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateB>()},
  });
}

TEST_F(GameStateMachineTest, GameStateChangeState) {
  TopStateA::Reset();
  ChildStateA::Reset();
  ChildStateB::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<ChildStateB>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  ChildStateA::Instance()->ChangeState<ChildStateB>();
  state_machine_->Update(absl::Milliseconds(1));

  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());
  EXPECT_TRUE(state_machine_->IsActive<ChildStateB>());

  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateB>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateB>()},
  });
}

TEST_F(GameStateMachineTest, GameStateChangeExitState) {
  TopStateA::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  ChildStateA::Instance()->ExitState();
  state_machine_->Update(absl::Milliseconds(1));

  EXPECT_TRUE(state_machine_->IsActive<TopStateA>());
  EXPECT_FALSE(state_machine_->IsActive<ChildStateA>());

  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       kNoGameStateId},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       kNoGameStateId},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
  });
}

TEST_F(GameStateMachineTest, StatesShutDownCleanlyAtStateMachineDestruction) {
  TopStateA::Reset();
  ChildStateA::Reset();

  state_machine_->Register<TopStateA>();
  state_machine_->Register<ChildStateA>();
  EXPECT_TRUE(
      state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  EXPECT_TRUE(state_machine_->ChangeState(GetGameStateId<TopStateA>(),
                                          GetGameStateId<ChildStateA>()));
  state_machine_->Update(absl::Milliseconds(1));
  state_machine_.reset();

  MatchTrace({
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kRequestChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildEnter, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnEnter, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kCompleteChange, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kOnUpdate, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kRequestChange, kNoGameStateId},
      {GameStateTraceType::kOnExit, GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnChildExit, GetGameStateId<TopStateA>(),
       GetGameStateId<ChildStateA>()},
      {GameStateTraceType::kOnExit, GetGameStateId<TopStateA>()},
      {GameStateTraceType::kCompleteChange, kNoGameStateId},
  });
  EXPECT_EQ(TopStateA::Info().destruct_count, 1);
  EXPECT_EQ(ChildStateA::Info().destruct_count, 1);
}

TEST_F(GameStateMachineTest, ThreadAbuse) {
  state_machine_->Register<TopStateA>();
  state_machine_->Register<TopStateB>();
  state_machine_->Register<DefaultState>();
  state_machine_->Register<ChildStateA>();
  state_machine_->Register<ChildStateB>();

  ThreadTester tester;
  tester.RunLoop(1, "change-1", [this]() {
    state_machine_->ChangeState(kNoGameStateId, GetGameStateId<DefaultState>());
    state_machine_->Update(absl::Milliseconds(1));
    state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateA>());
    state_machine_->Update(absl::Milliseconds(1));
    state_machine_->ChangeState(kNoGameStateId, GetGameStateId<TopStateB>());
    state_machine_->Update(absl::Milliseconds(1));
    return true;
  });
  tester.RunLoop(1, "change-2", [this]() {
    auto top_state = state_machine_->GetTopState();
    if (top_state != nullptr && state_machine_->IsActive(top_state->GetId())) {
      top_state->ChangeChildState<DefaultState>();
      state_machine_->Update(absl::Milliseconds(1));
      top_state->ChangeChildState<ChildStateA>();
      state_machine_->Update(absl::Milliseconds(1));
      top_state->ChangeChildState<ChildStateB>();
      state_machine_->Update(absl::Milliseconds(1));
      top_state->ExitState();
      state_machine_->Update(absl::Milliseconds(1));
    }
    return true;
  });
  tester.RunLoop(1, "state-info", [this]() {
    auto info = state_machine_->GetState<DefaultState>()->GetInfo();
    EXPECT_NE(info, nullptr);
    if (info == nullptr) {
      return false;
    }
    EXPECT_EQ(info->GetStateMachine(), state_machine_.get());
    EXPECT_EQ(info->GetId(), GetGameStateId<DefaultState>());
    info->IsActive();
    info->GetParentId();
    info->GetParent();
    info->GetChildId();
    info->GetChild();
    return true;
  });
  GameStateTrace last_trace, other_trace;
  GameStateTraceLevel trace_level = GameStateTraceLevel::kNone;
  tester.RunLoop(
      1, "trace-handler", [this, &last_trace, &other_trace, &trace_level]() {
        state_machine_->SetTraceHandler(
            [&last_trace](const GameStateTrace& trace) { last_trace = trace; });
        state_machine_->AddTraceHandler(
            [&other_trace](const GameStateTrace& trace) {
              other_trace = trace;
            });
        switch (trace_level) {
          case GameStateTraceLevel::kNone:
            trace_level = GameStateTraceLevel::kError;
            break;
          case GameStateTraceLevel::kError:
            trace_level = GameStateTraceLevel::kInfo;
            break;
          case GameStateTraceLevel::kInfo:
            trace_level = GameStateTraceLevel::kVerbose;
            break;
          case GameStateTraceLevel::kVerbose:
            trace_level = GameStateTraceLevel::kNone;
            break;
        }
        return true;
      });
  absl::SleepFor(absl::Seconds(1));
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  state_machine_->SetTraceHandler([](const GameStateTrace&) {});
}

}  // namespace
}  // namespace gb
