#ifndef GBITS_GAME_GAME_STATE_H_
#define GBITS_GAME_GAME_STATE_H_

#include "absl/time/time.h"
#include "gbits/base/validated_context.h"

namespace gb {

class GameStateInfo;
class GameStateMachine;

//------------------------------------------------------------------------------
// GameStateId
//------------------------------------------------------------------------------

// A GameStateId is a unique used to identify a GameState. It can be determined
// from a GameState derived class type via GetGameStateId().
using GameStateId = ContextKey*;

// Retrieves the GameStateId for the specified state type. It is sufficient for
// the StateType to only be forward declared (the full class definition is not
// required).
template <typename StateType>
GameStateId GetGameStateId() {
  return ContextKey::Get<StateType>();
}

// Constant that represents the GameStateId for no state.
inline constexpr GameStateId kNoGameState = nullptr;

// Returns the state name for the specified state. This always returns a valid
// value (even if kNoGameState is passed in).
const char* GetGameStateName(GameStateId id);

template <typename StateType>
const char* GetGameStateName() {
  return GetGameStateName(GetGameStateId<StateType>());
}

// Sets the state name for the specified state. The 'name' passed in must remain
// valid as long as it is used for the game state name. It is invalid to set the
// name for kNoGameState.
void SetGameStateName(GameStateId id, const char* name);

template <typename StateType>
void SetGameStateName(const char* name) {
  return SetGameStateName(GetGameStateId<StateType>(), name);
}

//------------------------------------------------------------------------------
// GameStateList
//------------------------------------------------------------------------------

// A GameStateList defines a set of GameStateIds as a type. To define a list of
// states, use one of the derived types: NoGameStates, AllGameStates, or
// GameStates<StateType...>.
struct GameStateList {
  enum Type {
    kNone,      // The list contains no game states.
    kAll,       // The list implicitly includes all game states.
    kExplicit,  // The list explicitly includes specific game states retrievable
                // via GetIds().
  };
};

// GameStateList that represents no states.
struct NoGameStates : GameStateList {
  static inline constexpr Type kType = kNone;
  static std::vector<GameStateId> GetIds() { return {}; }
};

// GameStateList that implicitly represents all states.
struct AllGameStates : GameStateList {
  static inline constexpr Type kType = kAll;
  static std::vector<GameStateId> GetIds() { return {}; }
};

// GameStateList that explicitly specifies game states.
template <typename... StateTypes>
struct GameStates : GameStateList {
  static inline constexpr Type kType = kExplicit;
  static std::vector<GameStateId> GetIds() {
    return {GetGameStateId(StateTypes)...};
  }
};

//------------------------------------------------------------------------------
// GameStateLifetime
//------------------------------------------------------------------------------

// A GameStateLifetime class determines when a GameState will be destructed and
// destructed within a GameStateMachine. To define a lifetime for a state, use
// one of the derived types: GlobalGameStateLifetime or ActiveGameStateLifetime.
struct GameStateLifetime {
  enum Type {
    kGlobal,
    kActive,
  };
};

// This lifetime specifies the state will be constructed at registration with a
// GameStateMachine, and destructed when the corresponding state machine is
// destroyed.
struct GlobalGameStateLifetime : GameStateLifetime {
  static inline constexpr Type kType = kGlobal;
};

// This lifetime specifies the state will be constructed immediately before
// OnEnter is called and destructed after OnExit returns.
struct ActiveGameStateLifetime : GameStateLifetime {
  static inline constexpr Type kType = kActive;
};

//------------------------------------------------------------------------------
// GameState
//------------------------------------------------------------------------------

// Represents a game state in the game.
//
// Each game state should derive from GameState and override the relevant
// functions. The derived class must conform to the following requirements:
//   - It should declare its context contract by defining a Contract alias
//     of type ContextContract<> in their public section. Without this, the
//     Context() method is not useful.
//   - It should declare a ParentStates alias if it is intended to be used as a
//     child state in the hierarchy.
//   - It should declare a Lifetime alias if the state has specific lifetime
//     requirements.
//   - It must have a default constructor (no parameters). This is called by the
//     GameStateMachine to instantiate the class.
class GameState {
 public:
  GameState(const GameState&) = delete;
  GameState& operator=(const GameState&) = delete;
  virtual ~GameState() = default;

  // The default contract is empty. Derived classes should define their own
  // 'Contract' type in order to specify their context constraints.
  using Contract = ContextContract<>;

  // ParentStates define which states (in addition to the root) this state must
  // have as a parent. By default a game state may exist anywhere in the
  // hierarchy. Other options are NoGameStates, or list specific states with
  // GameStates<A, B, C, ...>.
  using ParentStates = AllGameStates;

  // Lifetime defines when the class will be created and deleted. By default, a
  // game state has a global lifetime, which means it will be constructed once
  // at state machine registration and deleted when the state machine is
  // deleted. The other option is ActiveGameStateLifetime which means the state
  // will be constructed immediately before OnEnter is called and destructed
  // after OnExit returns. Derived classes can define the Lifetime alias to
  // override the default behavior.
  using Lifetime = GlobalGameStateLifetime;

  // The following attributes are only set if the state is active (during
  // OnEnter and OnExit and any time in between these two calls). Notably these
  // will return null in the constructor and destructor.
  GameStateId GetId() const;
  GameStateId GetParentId() const;
  GameState* GetParent() const;
  GameStateId GetChildId() const;
  GameState* GetChild() const;

  // Changes the child for this state. See GameStateMachine::ChangeState() for
  // details on state change handling.
  bool ChangeChildState(GameStateId state);
  template <typename StateType>
  bool ChangeChildState() {
    return ChangeChild(GetGameStateId<StateType>());
  }

  // Exits this state and switches to the specified state. See
  // GameStateMachine::ChangeState() for details on state change handling.
  bool ChangeState(GameStateId state);

  // Changes the top state for the state machine. See
  // GameStateMachine::ChangeState() for details on state change handling.
  bool ChangeTopState(GameStateId state);

  // Exits this state. See GameStateMachine::ChangeState() for details on state
  // change handling.
  bool ExitState();

 protected:
  GameState() = default;

  // Returns the validated context, whose contract was defined by Contract in
  // the registered type.
  const ValidatedContext& Context() const { return context_; }
  ValidatedContext& Context() { return context_; }

  // Gets called once every frame if the state is active.
  virtual void OnUpdate(absl::Duration delta_time) {}

  // Called when the state is entered. A state will only be entered iff its
  // contract is satisfied.
  virtual void OnEnter() {}

  // Called when the state is exited. If the state is not persistent, it will be
  // deleted after OnExit returns. This is called at most once for each time
  // OnEnter is called.
  virtual void OnExit() {}

  // Called immediately before a child state is entered. If the state is changed
  // during this call, it can result in original child state never being
  // entered.
  virtual void OnChildEnter(GameStateId child) {}

  // Called immediately after a child state has exited.
  virtual void OnChildExit(GameStateId child) {}

 private:
  friend class GameStateMachine;

  // These are managed directly by GameStateMachine.
  GameStateInfo* info_ = nullptr;
  GameStateMachine* machine_ = nullptr;
  ValidatedContext context_;
};

}  // namespace gb

#endif  // GBITS_GAME_GAME_STATE_H_
