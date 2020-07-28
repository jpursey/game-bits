#ifndef GB_GAME_GAME_STATE_H_
#define GB_GAME_GAME_STATE_H_

#include "absl/time/time.h"
#include "gb/base/validated_context.h"

namespace gb {

class GameStateInfo;
class GameStateMachine;

//------------------------------------------------------------------------------
// GameStateId
//------------------------------------------------------------------------------

// A GameStateId is a unique used to identify a GameState. It can be determined
// from a GameState derived class type via GetGameStateId().
using GameStateId = TypeKey*;

// Retrieves the GameStateId for the specified state type. It is sufficient for
// the StateType to only be forward declared (the full class definition is not
// required).
template <typename StateType>
GameStateId GetGameStateId() {
  return TypeKey::Get<StateType>();
}

// Constant that represents the GameStateId for no state.
inline constexpr GameStateId kNoGameStateId = nullptr;

// Returns the state name for the specified state. This always returns a valid
// value (even if kNoGameStateId is passed in).
const char* GetGameStateName(GameStateId id);

template <typename StateType>
const char* GetGameStateName() {
  return GetGameStateName(GetGameStateId<StateType>());
}

// Sets the state name for the specified state. The 'name' passed in must remain
// valid as long as it is used for the game state name. It is invalid to set the
// name for kNoGameStateId.
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
    return {GetGameStateId<StateTypes>()...};
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
    kGlobal,  // The state is constructed at regististration and destructed at
              // state machine destruction.
    kActive,  // The state is constructed and destructed when the state is
              // entered and exited respectively.
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
// functions. The derived class must conform to the following:
//   - It should declare its context contract by defining a Contract alias
//     of type ContextContract<> in their public section. Without this, the
//     Context() method is not useful.
//   - It should declare a ParentStates alias to limit where it is valid as a
//     child state in the hierarchy.
//   - It should declare a SiblingStates alias to limit which sibling states can
//     be switched to from this state.
//   - It should declare a Lifetime alias if the state has specific lifetime
//     requirements.
//   - It must have a default constructor taking no parameters. This is called
//     by the GameStateMachine to instantiate the class.
//   - No GameState member functions are callable until *after* construction
//     completes. If access is needed during initialization of the instance, the
//     derived class can override OnInit() to do further one-time
//     initialization. Calling GameState member functions prior to OnInit() will
//     result in an access violation.
//
// This class is thread-safe. However, caution must still be applied around
// destruction, as the state instance is owned by the GameStateMachine and may
// be deleted during a state machine update (depending on specified Lifetime
// attribute).
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

  // SiblingStates define which states (in addition to the root) this state can
  // switch to under the same parent. By default a game state may switch to any
  // other state. Other options are NoGameStates, or list specific states with
  // GameStates<A, B, C, ...>.
  using SiblingStates = AllGameStates;

  // Lifetime defines when the class will be created and deleted. By default, a
  // game state has a global lifetime, which means it will be constructed once
  // at state machine registration and deleted when the state machine is
  // deleted. The other option is ActiveGameStateLifetime which means the state
  // will be constructed immediately before OnEnter is called and destructed
  // after OnExit returns. Derived classes can define the Lifetime alias to
  // override the default behavior.
  using Lifetime = GlobalGameStateLifetime;

  // As noted above, these attributes are only available after construction
  // completes. Override OnInit(), if further one-time initialization is needed
  // that requires these attributes.
  GameStateInfo* GetInfo() const { return info_; }
  GameStateId GetId() const;
  GameStateMachine* GetStateMachine() const;

  // The following attributes are only set if the state is active (during
  // OnEnter and OnExit and any time in between these two calls). Notably these
  // will return kNoGameStateId or null in the constructor and destructor.
  GameStateId GetParentId() const;
  GameState* GetParent() const;
  GameStateId GetChildId() const;
  GameState* GetChild() const;

  // Changes the child for this state. See GameStateMachine::ChangeState() for
  // details on state change handling. This state must be a valid parent as
  // controlled by the childs ParentStates attribute.
  bool ChangeChildState(GameStateId state);
  template <typename StateType>
  bool ChangeChildState() {
    return ChangeChildState(GetGameStateId<StateType>());
  }

  // Exits this state and switches to the specified state under the same parent.
  // See GameStateMachine::ChangeState() for details on state change handling.
  // The new state must be a valid sibling as controlled by this states
  // SiblingStates attribute.
  bool ChangeState(GameStateId state);
  template <typename StateType>
  bool ChangeState() {
    return ChangeState(GetGameStateId<StateType>());
  }

  // Exits this state. See GameStateMachine::ChangeState() for details on state
  // change handling.
  bool ExitState();

 protected:
  GameState() = default;

  // Returns the validated context, whose contract was defined by Contract in
  // the registered type. This context is *always* valid when the state is
  // entered (OnEnter is called), and is *always* invalid when the state is not
  // active. It is *only* safe to access the context under the following
  // circumstances:
  //   - While executing any On*() callback from the state machine (of any
  //     state, not just this state).
  //   - When the associated GameStateMachine's Update function is not running.
  const ValidatedContext& Context() const { return context_; }
  ValidatedContext& Context() { return context_; }

  // Get called once after the state's ID and state machine are initialized.
  //
  // This occurs immediately after the state is constructed (when that occurs
  // depends on the state's Lifetime attribute). While it is valid to request
  // state changes inside OnInit, it is generally not recommended as not all
  // states may be registered yet, so the state transition may fail.
  virtual void OnInit() {}

  // Gets called once every frame if the state is active.
  //
  // States are always updated before their child state (if they have one). If a
  // parent state switches states (or child states), then any existing child
  // state will not get its update, and instead the new state will be switched
  // to and updated this frame.
  virtual void OnUpdate(absl::Duration delta_time) {}

  // Called when the state is entered.
  //
  // A state will only be entered iff its context contract is satisfied.
  //
  // After OnEnter returns, OnUpdate will be called this frame unless the state
  // is changed before then. If the state is changed within this callback, then
  // the state will be exited and the new state entered on this frame (after
  // OnEnter returns).
  virtual void OnEnter() {}

  // Called when the state is exited.
  //
  // If the Lifetime attribute is ActiveGameStateLifetime, it will be destructed
  // immediately after OnExit returns.
  //
  // If the state is changed within this callback, then that state will be
  // switched to instead of whatever state change caused OnExit in the first
  // place. The parent state (if there is one), may still change the state in
  // OnChildExit.
  virtual void OnExit() {}

  // Called immediately before a child state is entered.
  //
  // The specified child state will always be entered when this call returns (it
  // will receives its OnEnter call). If the state is changed during
  // OnChildEnter, then the child specified here will be entered and then
  // immediately exited (and OnChildExit will be called on this state). Then the
  // new state will be switched to.
  virtual void OnChildEnter(GameStateId child) {}

  // Called immediately after a child state has exited.
  //
  // If the child state's Lifetime attribute is ActiveGameStateLifetime, then it
  // will already be deleted when this is called. If the state is changed within
  // this callback, then that state will be switched to instead of whatever
  // state change is currently in progress.
  virtual void OnChildExit(GameStateId child) {}

 private:
  friend class GameStateMachine;

  // These are managed directly by GameStateMachine.
  GameStateInfo* info_ = nullptr;
  ValidatedContext context_;
};

}  // namespace gb

#endif  // GB_GAME_GAME_STATE_H_
