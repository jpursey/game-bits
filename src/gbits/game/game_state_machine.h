#ifndef GBITS_GAME_GAME_STATE_MACHINE_H_
#define GBITS_GAME_GAME_STATE_MACHINE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "gbits/game/game_state.h"

namespace gb {

class GameStateMachine;

//------------------------------------------------------------------------------
// GameStateTrace
//------------------------------------------------------------------------------

// Defines the trace level for trace output from the state machine. Levels are
// cumulative, so a higher level will always include traces of a lower level.
enum class GameStateTraceLevel {
  kNone,     // No trace output at all.
  kError,    // Only error trace output. This is the default.
  kInfo,     // Error and info trace output. Info output only occurs during
             // state transitions.
  kVerbose,  // All trace output, very spammy. This includes trace output every
             // frame.
};

// Type of trace.
enum class GameStateTraceType : int {
  kUnknown,  // Initial value for a default constructed GameStateTrace.

  // Error trace
  kInvalidChangeState,    // The new state is not registered or is already
                          // active.
  kInvalidChangeParent,   // The parent state is not registered, not active, or
                          // not allowed.
  kInvalidChangeSibling,  // The sibling state is not registered or is not
                          // allowed.
  kConstraintFailure,     // The context constraints were not met.

  // Info trace
  kRequestChange,   // Change state requested.
  kAbortChange,     // Abort state change (the prior request has not yet
                    // completed when the next request was made).
  kCompleteChange,  // State change is completed (the prior request completed as
                    // initially requested).
  kOnEnter,         // State is about to be entered (immediately before
                    // GameState::OnEnter).
  kOnExit,          // State is about to be exited (immediately before
                    // GameState::OnExit).
  kOnChildEnter,    // Child state was entered (immediately before
                    // GameState::OnChildEnter).
  kOnChildExit,     // Child state was exited (immediately before
                    // GameState::OnChildExit).

  // Verbose trace
  kOnUpdate,  // Child state is being updated (immediately before
              // GameState::OnUpdate).
};

// Trace record for game states.
//
// Trace records are received by registering a trace handler with a
// GameStateMachine instance (via SetTraceHandler or AddTraceHandler).
struct GameStateTrace {
  GameStateTrace() = default;
  GameStateTrace(GameStateTraceType type, GameStateId parent, GameStateId state,
                 std::string_view method, std::string message)
      : type(type),
        parent(parent),
        state(state),
        method(method),
        message(message) {}

  // Returns true if this trace represents an error level trace only.
  bool IsError() const {
    return type >= GameStateTraceType::kInvalidChangeState &&
           type <= GameStateTraceType::kConstraintFailure;
  }

  // Returns true if this trace represents an info level trace only.
  bool IsInfo() const {
    return type >= GameStateTraceType::kRequestChange &&
           type <= GameStateTraceType::kOnChildExit;
  }

  // Returns true if this trace represents a verbose level trace only.
  bool IsVerbose() const { return type >= GameStateTraceType::kOnUpdate; }

  // The type of record this trace represents.
  GameStateTraceType type = GameStateTraceType::kUnknown;

  // Parent state for the trace. This is set only for kInvalidChangeState,
  // kInvalidChangeParent, kRequestChange, kAbortChange, kCompleteChange,
  // kOnChildEnter, and kOnChildExit. For all other trace types it is always
  // kNoGameStateId.
  GameStateId parent = kNoGameStateId;

  // State for the trace. This is always set.
  GameStateId state = kNoGameStateId;

  // GameStateMachine public method that the trace occurred in.
  std::string_view method;

  // Human readable message with additional information about the trace event.
  std::string message;
};

// Handler definition of the callback that receives a GameStateTrace.
using GameStateTraceHandler = std::function<void(const GameStateTrace&)>;

// Helpers to stringify traces.
std::string ToString(GameStateTraceType trace_type);
std::string ToString(const GameStateTrace& trace);

//------------------------------------------------------------------------------
// GameStateInfo
//------------------------------------------------------------------------------

// Internal class used by GameStateMachine to track game states.
class GameStateInfo {
 public:
  GameStateInfo() = default;

 private:
  friend class GameState;
  friend class GameStateMachine;
  friend GameStateId GetGameStateId(GameStateInfo* info);

  // Set at registration.
  GameStateId id = kNoGameStateId;
  GameStateLifetime::Type lifetime = GameStateLifetime::Type::kGlobal;
  GameStateList::Type valid_parents_type = GameStateList::kNone;
  std::vector<GameStateId> valid_parents;
  GameStateList::Type valid_siblings_type = GameStateList::kNone;
  std::vector<GameStateId> valid_siblings;
  std::vector<ContextConstraint> constraints;
  std::function<std::unique_ptr<GameState>()> factory;

  // Working state.
  std::unique_ptr<GameState> instance;
  bool active = false;
  GameStateInfo* parent = nullptr;
  GameStateInfo* child = nullptr;
  int64_t update_id = 0;
};

inline GameStateId GetGameStateId(GameStateInfo* info) {
  return info != nullptr ? info->id : kNoGameStateId;
}

//------------------------------------------------------------------------------
// GameStateMachine
//------------------------------------------------------------------------------

// This class manages a hierarchical state machine for use in games.
//
// Game states are uniquely defined by a subclass of GameState. The state
// machine instance manages a pool these states which are either active or
// inactive.
//
// The structure of the state hierarchy is defined by the ParentStates and
// SiblingStates attribute types defined in each GameState subclass. Only one
// branch (or partial branch) of the implied hierarchy may be active at any
// given time. In addition, only one instance of a state can be active at a time
// within the same branch.
//
// State transitions can be requested at any time (generally within one of the
// active state's callbacks). However, the the actual state transition always
// happens directly with in the GameStateMachine::Update call (never while
// executing any GameState callback).
//
// This class is thread-safe except as noted.
class GameStateMachine final {
 public:
  GameStateMachine(const GameStateMachine&) = delete;
  GameStateMachine& operator=(const GameStateMachine&) = delete;
  ~GameStateMachine();

  // Context contract for the state machine itself.
  //
  // GameStateMachine instances do not require anything in its context, it is
  // just used to pass to GameState classes.
  using Contract = ContextContract<>;

  // Creates a new GameStateMachine.
  //
  // An optional context may be passed to GameStateMachine, if it is shared by
  // other parts of the game. The context passed in will also be passed to all
  // active game states.
  static std::unique_ptr<GameStateMachine> Create(
      Contract contract = std::make_unique<Context>());

  // Sets the trace level for the state machine. The default trace level is
  // kError.
  //
  // This function is thread-compatible. External synchronization is required if
  // this function could be called while this or any other member function is
  // executing in a different thread.
  void SetTraceLevel(GameStateTraceLevel trace_level);

  // Set the trace handler for this state machine.
  //
  // This completely replaces any prior handler, including the default hander
  // (which logs the trace). To add an additional handler, call AddTraceHandler
  // instead. See GameStateTrace for more information on trace output.
  //
  // This GameStateMachine instance must not be called from within the
  // registered handler, or deadlock will occur.
  //
  // This function is thread-compatible. External synchronization is required if
  // this function could be called while this or any other member function is
  // executing in a different thread.
  void SetTraceHandler(GameStateTraceHandler handler);

  // Adds an additional trace handler for this state machine.
  //
  // This leaves any existing handlers in place, and adds this handler in
  // addition. See GameStateTrace for more information on trace output.
  //
  // This GameStateMachine instance must not be called from within the
  // registered handler, or deadlock will occur.
  //
  // This function is thread-compatible. External synchronization is required if
  // this function could be called while this or any other member function is
  // executing in a different thread.
  void AddTraceHandler(GameStateTraceHandler handler);

  // Registers a GameState derived class with the state machine.
  //
  // After a state is registered, it can be used with the ChangeState()
  // function. A state may not be registered more than once (a warning will be
  // logged and the duplicate registration is ignored).
  //
  // If a state's Lifetime attribute is GlobalGameStateLifetime (the default),
  // then the state will be constructed during registration.
  template <typename StateType>
  void Register() {
    DoRegister(
        ContextType::Get<StateType>()->Key(), StateType::Lifetime::kType,
        StateType::ParentStates::kType, StateType::ParentStates::GetIds(),
        StateType::SiblingStates::kType, StateType::SiblingStates::GetIds(),
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

  // Get the requested state instance. If the state is not global or active,
  // this will return nullptr.
  GameState* GetState(GameStateId state);

  template <typename StateType>
  StateType* GetState() {
    return static_cast<StateType*>(GetState(GetGameStateId<StateType>()));
  }

  // Returns the top state, or null if no states are active.
  GameState* GetTopState() {
    absl::MutexLock lock(&mutex_);
    return top_state_ != nullptr ? top_state_->instance.get() : nullptr;
  }

  // Requests to change the current state to the specified state.
  //
  // If 'parent' is kNoGameStateId, this will change the top most state,
  // otherwise 'parent' must be a state that already is active. If 'state' is
  // kNoGameStateId, then the child state of 'parent' will exit (leaving the
  // parent with no children). If 'state' is not kNoGameStateId, then it will
  // become the new child of 'parent'.
  //
  // This function does not make the state change immediately. It will happen
  // when the state machine is next updated (or if an update is in progress,
  // before Update() returns). If ChangeState is called multiple times before
  // the state can actually be changed, only the final change will happen
  // (changes are not queued). State changes never take place while any state
  // callback of this state machine's states is executing.
  //
  // This function returns true if the requested state change is valid at the
  // time this was called. A state change is valid if all of the following are
  // true:
  //   - 'parent' is kNoGameStateId or is an active state.
  //   - 'parent' is one of the allowed parent states for 'state'
  //   - 'state' is not currently active.
  //   - 'state' is a valid sibling state to the current child of 'parent', or
  //     'parent' has no child state.
  //
  // This function does *not* pre-validate context constraints, as constraints
  // may be met (or broken) as a side effect of the state transition process
  // itself. However, no state will actually be entered unless its input context
  // constraints are met. If a state context constraint failure occurs (exit or
  // enter).
  bool ChangeState(GameStateId parent, GameStateId state);

  // Updates the state machine, performing any requested state changes and
  // calling update on all active states.
  //
  // States are updated from parent to child. State changes may be requested at
  // any point during Update (even -- and commonly -- within state callbacks),
  // and they will be performed as soon possible.
  //
  // Update is not reentrant. If it is called while it is still executing, a
  // warning is logged and the Update call will be ignored.
  void Update(absl::Duration delta_time);

 private:
  using States =
      absl::flat_hash_map<GameStateId, std::unique_ptr<GameStateInfo>>;

  explicit GameStateMachine(ValidatedContext context);

  // Dumps the trace to glog.
  void LogTrace(const GameStateTrace& trace);

  // Returns human-readable debug strings for the requested state paths.
  std::string GetStatePath(GameStateId parent, GameStateId state)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  std::string GetCurrentStatePath() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Helper to return the GameStateInfo* for the specified ID. This will return
  // null if the ID is kNoGameStateId, or is not registered with the state
  // machine.
  const GameStateInfo* GetStateInfo(GameStateId id) const
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  GameStateInfo* GetStateInfo(GameStateId id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Constructs the GameState instance for the specified state_info.
  void CreateInstance(GameStateInfo* state_info);

  // Implementation for the Register<> public function.
  void DoRegister(GameStateId id, GameStateLifetime::Type lifetime,
                  GameStateList::Type valid_parents_type,
                  std::vector<GameStateId> valid_parents,
                  GameStateList::Type valid_siblings_type,
                  std::vector<GameStateId> valid_siblings,
                  std::vector<ContextConstraint> constraints,
                  std::function<std::unique_ptr<GameState>()> factory)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Performs the actual update.
  void DoUpdate(absl::Duration delta_time)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(update_mutex_);

  // Performs any state transitions previously requested via ChangeState. If a
  // ChangeState is called during this funciton, it will return early without
  // completing the previous transition.
  void ProcessTransition() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex update_mutex_;  // Reentrance guard for Update method.
  mutable absl::Mutex mutex_;         // Mutex for execution state.

  // Context used for this state machine and all game states. This is not
  // guarded by any mutex as it is only ever changed at state machine
  // construction.
  ValidatedContext context_;

  // Trace information.
  GameStateTraceLevel trace_level_ = GameStateTraceLevel::kError;
  GameStateTraceHandler trace_handler_;

  // Map of all registered states.
  States states_ GUARDED_BY(mutex_);

  // The current top state that is active.
  GameStateInfo* top_state_ GUARDED_BY(mutex_) = nullptr;

  // Current pending transition as specified by ChangeState and reset by
  // ProcessTransition.
  bool transition_ GUARDED_BY(mutex_) = false;
  GameStateInfo* transition_parent_ GUARDED_BY(mutex_) = nullptr;
  GameStateInfo* transition_state_ GUARDED_BY(mutex_) = nullptr;
};

}  // namespace gb

#endif  // GBITS_GAME_GAME_STATE_MACHINE_H_
