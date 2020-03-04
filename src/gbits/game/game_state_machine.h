#ifndef GBITS_GAME_GAME_STATE_MACHINE_H_
#define GBITS_GAME_GAME_STATE_MACHINE_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/time/time.h"
#include "gbits/game/game_state.h"

namespace gb {

// This class manages a hierarchical state machine for use in games.
//
// Game states are uniquely defined by a subclass of GameState. The state
// machine instance manages a pool these states which are either active or
// inactive.
//
// The structure of the state hierarchy is defined by the ParentStates type
// defined in each GameState subclass. Only one branch (or partial branch) of
// the implied hierarchy may be active at any given time. In addition, only one
// instance of a state can be active at a time within the same branch.
//
class GameStateMachine final {
 public:
  GameStateMachine() = default;
  GameStateMachine(const GameStateMachine&) = delete;
  GameStateMachine& operator=(const GameStateMachine&) = delete;
  ~GameStateMachine() = default;

  // Registers a GameState derived class with the state machine. After a state
  // is registered, it can be used with the ChangeState() function.
  template <typename StateType>
  void Register() {
    DoRegister(ContextType<StateType>::Get()->Key(), StateType::Lifetime::kType,
               StateType::Contract::GetConstraints(),
               []() -> std::unique_ptr<GameState> {
                 return std::make_unique<StateType>();
               });
  }

  // Returns true if the specified state is currently active.
  bool IsActive(GameStateId state) const;
  template <typename StateType>
  bool IsActive() const {
    return IsActive(GetGameStateId<StateType>());
  }

  // Get the requested active state. If the state is not active, this will
  // return nullptr.
  GameState* GetActiveState(GameStateId state);
  template <typename StateType>
  GameState* GetActiveState() {
    return GetActiveState(GetGameStateId<StateType>);
  }

  // Returns the top state, or null if no states are active.
  GameState* GetTopState() { return top_state_; }

  // Changes the current state to the specified state.
  //
  // If 'parent' is kNoGameState, this will change the top most state, otherwise
  // 'parent' must be a state that already is active. If 'state' is
  // kNoGameState, then the child state of 'parent' will exit (leaving the
  // parent with no children). If 'state' is not kNoGameState, then it will
  // become the new child of 'parent'.
  //
  // This function does not make the state change immediately. It will happen
  // when the state machine is next updated (or if an update is in progress,
  // before Update() returns). If ChangeState is called multiple times before
  // the state can actually be changed, only the final change will happen
  // (changes are not queued).
  //
  // This function returns true if the requested state change is valid at the
  // time this was called. A state change is valid if all of the following are
  // true:
  //   - 'parent' is kNoGameState or is an active state.
  //   - All states that would be entered or exited as a result have met their
  //     context constraints.
  bool ChangeState(GameStateId parent, GameStateId state);
  template <typename StateType>
  bool ChangeState(GameStateId parent) {
    return ChangeState(parent, GetGameStateId<StateType>());
  }

  // Convenience method for ChangeState which always changes the top state.
  bool ChangeTopState(GameStateId state) {
    return ChangeState(kNoGameState, state);
  }
  template <typename StateType>
  bool ChangeTopState() {
    return ChangeState(kNoGameState, GetGameStateId<StateType>());
  }

  // Updates the state machine, performing any requested state changes and
  // calling update on all active states. States are updated from parent to
  // child. State changes may be requested at any point during Update(), and
  // they will be performed as soon possible.
  void Update(absl::Duration delta_time);

 private:
  struct GameStateInfo {
    GameStateLifetime::Type lifetime = GameStateLifetime::Type::kGlobal;
    std::vector<ContextConstraint> constraints;
    std::function<std::unique_ptr<GameState>()> factory;
    std::unique_ptr<GameState> instance;
    bool active = false;
  };

  void DoRegister(GameStateId key, GameStateLifetime::Type lifetime,
                  std::vector<ContextConstraint> constraints,
                  std::function<std::unique_ptr<GameState>()> factory);

  absl::flat_hash_map<GameStateId, GameStateInfo> states_;
  GameState* top_state_ = nullptr;
};

}  // namespace gb

#endif  // GBITS_GAME_GAME_STATE_MACHINE_H_
