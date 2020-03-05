#ifndef GBITS_GAME_GAME_STATE_MACHINE_H_
#define GBITS_GAME_GAME_STATE_MACHINE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
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
  kInfo,     // Error and info trace output.
  kVerbose,  // Verbose trace output, very spammy.
};

// Type of trace.
enum class GameStateTraceType : int {
  kUnknown,  // Initial value for a default constructed trace.

  // Error trace
  kInvalidState,       // The new state is not registered or is already active.
  kInvalidParent,      // The parent state is not registered or is not active.
  kConstraintFailure,  // The context constraints were not met.

  // Info trace
  kRequestChange,  // Change state requested.
  kAbortChange,    // Abort state change.
  kOnEnter,        // State is about to be entered.
  kOnExit,         // State is about to be exited.
  kOnChildEnter,   // Child state was entered.
  kOnChildExit,    // Child state was exited.

  // Verbose trace
  kOnUpdate,  // Child state is being updated.
};

// Trace record for game states.
struct GameStateTrace {
  GameStateTrace() = default;
  GameStateTrace(GameStateTraceType type, GameStateId parent, GameStateId state,
                 std::string_view method, std::string message)
      : type(type),
        parent(parent),
        state(state),
        method(method),
        message(message) {}

  bool IsError() const {
    return type > GameStateTraceType::kUnknown &&
           type <= GameStateTraceType::kConstraintFailure;
  }
  bool IsVerbose() const { return type >= GameStateTraceType::kOnUpdate; }

  GameStateTraceType type = GameStateTraceType::kUnknown;
  GameStateId parent = kNoGameState;
  GameStateId state = kNoGameState;
  std::string_view method;
  std::string message;
};

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
  GameStateId id = kNoGameState;
  GameStateLifetime::Type lifetime = GameStateLifetime::Type::kGlobal;
  GameStateList::Type valid_parents_type = GameStateList::kNone;
  std::vector<GameStateId> valid_parents;
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
  return info != nullptr ? info->id : kNoGameState;
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
// The structure of the state hierarchy is defined by the ParentStates type
// defined in each GameState subclass. Only one branch (or partial branch) of
// the implied hierarchy may be active at any given time. In addition, only one
// instance of a state can be active at a time within the same branch.
//
class GameStateMachine final {
 public:
  GameStateMachine(const GameStateMachine&) = delete;
  GameStateMachine& operator=(const GameStateMachine&) = delete;
  ~GameStateMachine() = default;

  // Context contract for the state machine itself.
  using Contract = ContextContract<>;

  // Creates a new GameStateMachine.
  static std::unique_ptr<GameStateMachine> Create(
      Contract contract = std::make_unique<Context>());

  // Sets the trace level for the state machine. The default trace level is
  // kError.
  void SetTraceLevel(GameStateTraceLevel trace_level) {
    trace_level_ = trace_level;
  }

  // Set the trace handler for this state machine.
  //
  // This completely replaces any prior handler, including the default hander
  // (which logs the trace). To add an additional handler, call AddTraceHandler
  // instead. See GameStateTrace for more information on trace output.
  void SetTraceHandler(GameStateTraceHandler handler) {
    trace_handler_ = std::move(handler);
  }

  // Adds an additional trace handler for this state machine.
  //
  // This leaves any existing handlers in place, and adds this handler in
  // addition. See GameStateTrace for more information on trace output.
  void AddTraceHandler(GameStateTraceHandler handler) {
    auto new_handler = [handler_1 = std::move(trace_handler_),
                        handler_2 =
                            std::move(handler)](const GameStateTrace& trace) {
      handler_1(trace);
      handler_2(trace);
    };
    trace_handler_ = new_handler;
  }

  // Registers a GameState derived class with the state machine. After a state
  // is registered, it can be used with the ChangeState() function.
  template <typename StateType>
  void Register() {
    DoRegister(ContextType::Get<StateType>()->Key(), StateType::Lifetime::kType,
               StateType::ParentStates::kType,
               StateType::ParentStates::GetIds(),
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
    return top_state_ != nullptr ? top_state_->instance.get() : nullptr;
  }

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
  //   - 'parent' is one of the allowed parent states for 'state'
  //   - 'state' is not currently active.
  //
  // This function does *not* pre-validate context constraints, as constraints
  // may be met (or broken) as a side effect of the state transition process
  // itself. However, no state will actually be entered unless its context input
  // constraints are met. If a state context constraint failure occurs (exit or
  // enter), then an error will be logged and any registered error handler will
  // be logged.
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
  explicit GameStateMachine(ValidatedContext context);

  void LogTrace(const GameStateTrace& trace);
  std::string GetStatePath(GameStateId parent, GameStateId state);
  std::string GetCurrentStatePath();
  const GameStateInfo* GetStateInfo(GameStateId id) const;
  GameStateInfo* GetStateInfo(GameStateId id);
  void CreateInstance(GameStateInfo* state_info);
  void DoRegister(GameStateId id, GameStateLifetime::Type lifetime,
                  GameStateList::Type valid_parents_type,
                  std::vector<GameStateId> valid_parents,
                  std::vector<ContextConstraint> constraints,
                  std::function<std::unique_ptr<GameState>()> factory);
  void ProcessTransition();

  ValidatedContext context_;
  GameStateTraceLevel trace_level_ = GameStateTraceLevel::kError;
  GameStateTraceHandler trace_handler_;
  absl::flat_hash_map<GameStateId, GameStateInfo> states_;
  GameStateInfo* top_state_ = nullptr;
  bool transition_ = false;
  GameStateInfo* transition_parent_ = nullptr;
  GameStateInfo* transition_state_ = nullptr;
};

}  // namespace gb

#endif  // GBITS_GAME_GAME_STATE_MACHINE_H_
