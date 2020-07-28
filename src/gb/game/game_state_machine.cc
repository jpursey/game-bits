// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/game/game_state_machine.h"

#include <algorithm>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "glog/logging.h"

namespace gb {

std::string ToString(GameStateTraceType trace_type) {
  switch (trace_type) {
    case GameStateTraceType::kUnknown:
      return "Unknown";
    case GameStateTraceType::kInvalidChangeState:
      return "InvalidChangeState";
    case GameStateTraceType::kInvalidChangeParent:
      return "InvalidChangeParent";
    case GameStateTraceType::kInvalidChangeSibling:
      return "InvalidChangeSibling";
    case GameStateTraceType::kConstraintFailure:
      return "ConstraintFailure";
    case GameStateTraceType::kRequestChange:
      return "RequestChange";
    case GameStateTraceType::kAbortChange:
      return "AbortChange";
    case GameStateTraceType::kCompleteChange:
      return "CompleteChange";
    case GameStateTraceType::kOnEnter:
      return "OnEnter";
    case GameStateTraceType::kOnExit:
      return "OnExit";
    case GameStateTraceType::kOnChildEnter:
      return "OnChildEnter";
    case GameStateTraceType::kOnChildExit:
      return "OnChildExit";
    case GameStateTraceType::kOnUpdate:
      return "OnUpdate";
  }
  return absl::StrCat("GameStateTraceType(", static_cast<int>(trace_type), ")");
}

std::string ToString(const GameStateTrace& trace) {
  std::string result = absl::StrCat("[GameState] ", trace.method, ": ",
                                    ToString(trace.type), "(");
  if (trace.parent != kNoGameStateId) {
    result = absl::StrCat(result, "p=", GetGameStateName(trace.parent), ",");
  }
  result = absl::StrCat(result, "s=", GetGameStateName(trace.state), ")");
  if (!trace.message.empty()) {
    result = absl::StrCat(result, " ", trace.message);
  }
  return result;
}

void GameStateMachine::SetTraceLevel(GameStateTraceLevel trace_level) {
  absl::MutexLock lock(&mutex_);
  trace_level_ = trace_level;
}

void GameStateMachine::SetTraceHandler(GameStateTraceHandler handler) {
  absl::MutexLock lock(&mutex_);
  trace_handler_ = std::move(handler);
}

void GameStateMachine::AddTraceHandler(GameStateTraceHandler handler) {
  absl::MutexLock lock(&mutex_);
  auto new_handler = [handler_1 = std::move(trace_handler_),
                      handler_2 =
                          std::move(handler)](const GameStateTrace& trace) {
    handler_1(trace);
    handler_2(trace);
  };
  trace_handler_ = std::move(new_handler);
}

std::unique_ptr<GameStateMachine> GameStateMachine::Create(Contract contract) {
  if (!contract.IsValid()) {
    LOG(ERROR) << "GameStateMachine::Create: Invalid context";
    return nullptr;
  }
  return std::unique_ptr<GameStateMachine>(
      new GameStateMachine(std::move(contract)));
}

GameStateMachine::GameStateMachine(ValidatedContext context)
    : context_(std::move(context)) {
  absl::MutexLock lock(&mutex_);
  trace_handler_ = [this](const GameStateTrace& trace) { LogTrace(trace); };
}

GameStateMachine::~GameStateMachine() {
  // Make sure we are not in the middle of an Update(), that would be dire!
  CHECK(update_mutex_.TryLock())
      << "GameStateMachine is being destructed while Update is still running.";

  // Make sure there is no top level state
  ChangeState(kNoGameStateId, kNoGameStateId);
  DoUpdate(absl::ZeroDuration());

  // Clear internal state.
  States old_states;
  {
    // Lock order here is deliberate.
    absl::MutexLock lock(&mutex_);
    CHECK(!transition_)
        << "Transition was queued while GameStateMachine was being destructed.";
    old_states = std::move(states_);
  }

  // Now iterate over each state and delete any active instances. This ensures
  // that GameStateInfo still exists during the GameState destructor.
  for (auto& state : old_states) {
    if (state.second->instance.get() != nullptr) {
      state.second->instance.reset();
    }
  }
  old_states.clear();

  update_mutex_.Unlock();
}

void GameStateMachine::LogTrace(const GameStateTrace& trace) {
  if (trace.IsError()) {
    LOG_IF(ERROR, enable_logging_) << ToString(trace);
  } else {
    LOG_IF(INFO, enable_logging_) << ToString(trace);
  }
}

const GameStateInfo* GameStateMachine::GetStateInfo(GameStateId id) const {
  auto it = states_.find(id);
  if (it == states_.end()) {
    return nullptr;
  }
  return it->second.get();
}

GameStateInfo* GameStateMachine::GetStateInfo(GameStateId id) {
  auto it = states_.find(id);
  if (it == states_.end()) {
    return nullptr;
  }
  return it->second.get();
}

GameStateId GameStateMachine::GetRegisteredId(std::string_view name) const {
  absl::MutexLock lock(&mutex_);
  for (const auto& it : states_) {
    if (GetGameStateName(it.first) == name) {
      return it.first;
    }
  }
  return kNoGameStateId;
}

bool GameStateMachine::IsRegistered(GameStateId state) const {
  absl::MutexLock lock(&mutex_);
  return GetStateInfo(state) != nullptr;
}

bool GameStateMachine::IsActive(GameStateId state) const {
  absl::MutexLock lock(&mutex_);
  auto state_info = GetStateInfo(state);
  return state_info != nullptr ? state_info->active : false;
}

GameState* GameStateMachine::GetState(GameStateId state) const {
  absl::MutexLock lock(&mutex_);
  auto state_info = GetStateInfo(state);
  if (state_info == nullptr) {
    return nullptr;
  }
  return state_info->instance.get();
}

bool GameStateMachine::ChangeState(GameStateId parent, GameStateId state) {
  absl::MutexLock lock(&mutex_);

  // If a transition is in progress, make sure it is different.
  if (transition_) {
    if (parent == GetGameStateId(transition_parent_) &&
        state == GetGameStateId(transition_state_)) {
      return true;
    }
  }

  // Validate the parent.
  GameStateInfo* parent_info = nullptr;
  if (parent != kNoGameStateId) {
    parent_info = GetStateInfo(parent);
    if (parent_info == nullptr) {
      if (trace_level_ >= GameStateTraceLevel::kError) {
        trace_handler_({GameStateTraceType::kInvalidChangeParent, parent, state,
                        "ChangeState", "Parent state is not registered"});
      }
      return false;
    }
    if (!parent_info->active) {
      if (trace_level_ >= GameStateTraceLevel::kError) {
        trace_handler_({GameStateTraceType::kInvalidChangeParent, parent, state,
                        "ChangeState", "Parent state is not active"});
      }
      return false;
    }
  }

  // Validate the new state.
  GameStateInfo* state_info = nullptr;
  if (state != kNoGameStateId) {
    state_info = GetStateInfo(state);
    if (state_info == nullptr) {
      if (trace_level_ >= GameStateTraceLevel::kError) {
        trace_handler_({GameStateTraceType::kInvalidChangeState, parent, state,
                        "ChangeState", "new state is not registered"});
      }
      return false;
    }
    if (state_info->active) {
      if (trace_level_ >= GameStateTraceLevel::kError) {
        trace_handler_({GameStateTraceType::kInvalidChangeState, parent, state,
                        "ChangeState", "new state is already active"});
      }
      return false;
    }
  }

  // Make sure that it is actually a change.
  if (!transition_) {
    if (parent == kNoGameStateId) {
      if (top_state_ == state_info) {
        return true;
      }
    } else if (parent_info->child == state_info) {
      return true;
    }
  }

  // Validate the new state can be the next state.
  GameStateInfo* sibling_info =
      (parent_info == nullptr ? top_state_ : parent_info->child);
  if (state_info != nullptr && sibling_info != nullptr &&
      sibling_info->valid_siblings_type != GameStateList::kAll) {
    if (std::find(sibling_info->valid_siblings.begin(),
                  sibling_info->valid_siblings.end(),
                  state) == sibling_info->valid_siblings.end()) {
      if (trace_level_ >= GameStateTraceLevel::kError) {
        trace_handler_({GameStateTraceType::kInvalidChangeSibling, parent,
                        state, "ChangeState",
                        "Sibling state is not valid for new state"});
      }
      return false;
    }
  }

  // Validate the new state can be parented as requested.
  if (parent != kNoGameStateId && state != kNoGameStateId &&
      state_info->valid_parents_type != GameStateList::kAll) {
    if (std::find(state_info->valid_parents.begin(),
                  state_info->valid_parents.end(),
                  parent) == state_info->valid_parents.end()) {
      if (trace_level_ >= GameStateTraceLevel::kError) {
        trace_handler_({GameStateTraceType::kInvalidChangeParent, parent, state,
                        "ChangeState",
                        "Parent state is not valid for new state"});
      }
      return false;
    }
  }

  if (trace_level_ >= GameStateTraceLevel::kInfo) {
    if (transition_) {
      trace_handler_({GameStateTraceType::kAbortChange,
                      GetGameStateId(transition_parent_),
                      GetGameStateId(transition_state_), "ChangeState",
                      "abort due to new request"});
    }
    trace_handler_({GameStateTraceType::kRequestChange, parent, state,
                    "ChangeState",
                    absl::StrCat("current=", GetCurrentStatePath())});
  }
  transition_ = true;
  transition_parent_ = parent_info;
  transition_state_ = state_info;
  return true;
}

void GameStateMachine::Update(absl::Duration delta_time) {
  if (!update_mutex_.TryLock()) {
    LOG_IF(WARNING, enable_logging_)
        << "Update called recursively, ignoring request.";
    return;
  }
  DoUpdate(delta_time);
  update_mutex_.Unlock();
}

void GameStateMachine::DoUpdate(absl::Duration delta_time) {
  static int64_t update_id = 0;
  ++update_id;

  absl::MutexLock lock(&mutex_);
  bool needs_update = false;
  do {
    needs_update = false;

    // Process transitions.
    while (transition_) {
      ProcessTransition();
    }

    // Update states from top most to child.
    GameStateInfo* state_info = top_state_;
    while (state_info != nullptr) {
      if (state_info->update_id != update_id) {
        if (trace_level_ >= GameStateTraceLevel::kVerbose) {
          trace_handler_(
              {GameStateTraceType::kOnUpdate, kNoGameStateId,
               GetGameStateId(state_info), "Update",
               absl::StrCat("path=", GetStatePath(GetGameStateId(state_info),
                                                  kNoGameStateId))});
        }
        mutex_.Unlock();
        state_info->instance->OnUpdate(delta_time);
        mutex_.Lock();
      }
      if (transition_) {
        needs_update = true;
        break;
      }
      state_info = state_info->child;
    }
  } while (needs_update);
}

void GameStateMachine::ProcessTransition() {
  // Locking is tricky here, as this function makes callbacks to game states
  // which must be able to update the state machine in known ways (everything
  // but call Update -- which is guarded separately). As such, we do manual
  // locking and unlocking around these callbacks.

  // Cache current request.
  GameStateInfo* const parent_info = transition_parent_;
  GameStateInfo* const new_state_info = transition_state_;

  // Find states that need to exit.
  GameStateInfo* exit_info = top_state_;
  if (exit_info != nullptr) {
    while (exit_info->child != nullptr) {
      exit_info = exit_info->child;
    }
  }

  // Exit all the states, aborting if a new transition is initiated.
  while (exit_info != parent_info) {
    if (trace_level_ >= GameStateTraceLevel::kInfo) {
      trace_handler_(
          {GameStateTraceType::kOnExit, kNoGameStateId,
           GetGameStateId(exit_info), "Update",
           absl::StrCat("path=", GetStatePath(exit_info->id, kNoGameStateId))});
    }

    // Clear all the state. If anything happens related to the instance now, it
    // should be treated as exited.
    GameStateInfo* exit_parent = exit_info->parent;
    exit_info->active = false;
    exit_info->parent = nullptr;
    if (exit_parent != nullptr) {
      exit_parent->child = nullptr;
    } else {
      top_state_ = nullptr;
    }
    exit_info->update_id = 0;

    mutex_.Unlock();
    exit_info->instance->OnExit();
    if (!exit_info->instance->context_.Assign(ValidatedContext{})) {
      if (trace_level_ >= GameStateTraceLevel::kError) {
        trace_handler_({GameStateTraceType::kConstraintFailure, kNoGameStateId,
                        GetGameStateId(exit_info), "Update",
                        "exit context could not complete"});
      }
    }
    mutex_.Lock();

    if (exit_info->lifetime == GameStateLifetime::Type::kActive) {
      mutex_.Unlock();
      exit_info->instance.reset();
      mutex_.Lock();
    }

    // Now notify the parent that the child exited.
    if (exit_parent != nullptr) {
      if (trace_level_ >= GameStateTraceLevel::kInfo) {
        trace_handler_(
            {GameStateTraceType::kOnChildExit, GetGameStateId(exit_parent),
             GetGameStateId(exit_info), "Update",
             absl::StrCat("path=", GetStatePath(GetGameStateId(exit_info),
                                                kNoGameStateId))});
      }
      mutex_.Unlock();
      exit_parent->instance->OnChildExit(exit_info->id);
      mutex_.Lock();
    }

    // If a new transition was queued, start over.
    if (transition_parent_ != parent_info ||
        transition_state_ != new_state_info) {
      return;
    }

    exit_info = exit_parent;
  }

  // Is there a new state?
  if (new_state_info == nullptr) {
    if (trace_level_ >= GameStateTraceLevel::kInfo) {
      trace_handler_({GameStateTraceType::kCompleteChange,
                      GetGameStateId(parent_info),
                      GetGameStateId(new_state_info), "Update",
                      absl::StrCat("path=", GetCurrentStatePath())});
    }
    transition_ = false;
    transition_parent_ = nullptr;
    transition_state_ = nullptr;
    return;
  }

  // Validate the context for the new state.
  mutex_.Unlock();
  ValidatedContext new_context(context_, new_state_info->constraints);
  mutex_.Lock();
  if (!new_context.IsValid()) {
    if (trace_level_ >= GameStateTraceLevel::kError) {
      trace_handler_({GameStateTraceType::kConstraintFailure, kNoGameStateId,
                      GetGameStateId(new_state_info), "Update",
                      "enter context is not valid"});
    }
    if (trace_level_ >= GameStateTraceLevel::kInfo) {
      trace_handler_({GameStateTraceType::kAbortChange,
                      GetGameStateId(parent_info),
                      GetGameStateId(new_state_info), "Update",
                      "enter context is not valid"});
    }
    transition_ = false;
    transition_parent_ = nullptr;
    transition_state_ = nullptr;
    return;
  }

  // Notify the parent the new state is going to get created.
  if (parent_info != nullptr) {
    if (trace_level_ >= GameStateTraceLevel::kInfo) {
      trace_handler_(
          {GameStateTraceType::kOnChildEnter, GetGameStateId(parent_info),
           GetGameStateId(new_state_info), "Update",
           absl::StrCat("path=",
                        GetStatePath(GetGameStateId(parent_info),
                                     GetGameStateId(new_state_info)))});
    }
    mutex_.Unlock();
    parent_info->instance->OnChildEnter(new_state_info->id);
    mutex_.Lock();
  }

  // Update the state info.
  new_state_info->active = true;
  new_state_info->parent = parent_info;
  if (parent_info != nullptr) {
    parent_info->child = new_state_info;
  } else {
    top_state_ = new_state_info;
  }
  if (new_state_info->lifetime == GameStateLifetime::Type::kActive) {
    mutex_.Unlock();
    CreateInstance(new_state_info);
    mutex_.Lock();
  }
  new_state_info->instance->context_ = std::move(new_context);

  // Notify the new state that it is entered.
  if (trace_level_ >= GameStateTraceLevel::kInfo) {
    trace_handler_(
        {GameStateTraceType::kOnEnter, kNoGameStateId,
         GetGameStateId(new_state_info), "Update",
         absl::StrCat("path=", GetStatePath(GetGameStateId(new_state_info),
                                            kNoGameStateId))});
  }
  mutex_.Unlock();
  new_state_info->instance->OnEnter();
  mutex_.Lock();

  // Reset transition if we are done.
  if (transition_parent_ == parent_info &&
      transition_state_ == new_state_info) {
    if (trace_level_ >= GameStateTraceLevel::kInfo) {
      trace_handler_({GameStateTraceType::kCompleteChange,
                      GetGameStateId(parent_info),
                      GetGameStateId(new_state_info), "Update",
                      absl::StrCat("path=", GetCurrentStatePath())});
    }
    transition_ = false;
    transition_parent_ = nullptr;
    transition_state_ = nullptr;
  }
}

void GameStateMachine::CreateInstance(GameStateInfo* state_info) {
  state_info->instance = state_info->factory();
  state_info->instance->info_ = state_info;
  state_info->instance->OnInit();
}

void GameStateMachine::DoRegister(
    GameStateId id, GameStateLifetime::Type lifetime,
    GameStateList::Type valid_parents_type,
    std::vector<GameStateId> valid_parents,
    GameStateList::Type valid_siblings_type,
    std::vector<GameStateId> valid_siblings,
    std::vector<ContextConstraint> constraints,
    Callback<std::unique_ptr<GameState>()> factory) {
  GameStateInfo* state_info = nullptr;
  {
    absl::MutexLock lock(&mutex_);
    if (states_.find(id) != states_.end()) {
      LOG_IF(WARNING, enable_logging_)
          << "State " << GetGameStateName(id) << " already registered.";
      return;
    }
    auto& owned_state_info = states_[id];
    owned_state_info = std::make_unique<GameStateInfo>();
    state_info = owned_state_info.get();
    state_info->mutex = &mutex_;
    state_info->state_machine = this;
    state_info->id = id;
    state_info->lifetime = lifetime;
    state_info->valid_parents_type = valid_parents_type;
    state_info->valid_parents = std::move(valid_parents);
    state_info->valid_siblings_type = valid_siblings_type;
    state_info->valid_siblings = std::move(valid_siblings);
    state_info->constraints = std::move(constraints);
    state_info->factory = std::move(factory);
  }
  if (lifetime == GameStateLifetime::kGlobal) {
    CreateInstance(state_info);
  }
}

std::string GameStateMachine::GetStatePath(GameStateId parent,
                                           GameStateId state) {
  std::vector<std::string> names;
  if (parent != kNoGameStateId) {
    GameStateInfo* current_state = top_state_;
    while (current_state != nullptr && current_state->id != parent) {
      names.emplace_back(GetGameStateName(current_state->id));
      current_state = current_state->child;
    }
    names.emplace_back(GetGameStateName(parent));
  }
  if (state != kNoGameStateId) {
    names.emplace_back(GetGameStateName(state));
  }
  if (names.empty()) {
    return "none";
  }
  return absl::StrJoin(names, ".");
}

std::string GameStateMachine::GetCurrentStatePath() {
  std::vector<std::string> names;
  GameStateInfo* current_state = top_state_;
  while (current_state != nullptr) {
    names.emplace_back(GetGameStateName(current_state->id));
    current_state = current_state->child;
  }
  if (names.empty()) {
    return "none";
  }
  return absl::StrJoin(names, ".");
}

}  // namespace gb
