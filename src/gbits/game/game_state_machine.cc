#include "gbits/game/game_state_machine.h"

#include <algorithm>

#include "absl/strings/str_join.h"
#include "glog/logging.h"

namespace gb {

std::unique_ptr<GameStateMachine> GameStateMachine::Create(Contract contract) {
  if (!contract.IsValid()) {
    LOG(ERROR) << "GameStateMachine::Create: Invalid context";
    return nullptr;
  }
  return std::unique_ptr<GameStateMachine>(
      new GameStateMachine(std::move(contract)));
}

GameStateMachine::GameStateMachine(ValidatedContext context)
    : context_(std::move(context)) {}

const GameStateInfo* GameStateMachine::GetStateInfo(GameStateId id) const {
  auto it = states_.find(id);
  if (it == states_.end()) {
    return nullptr;
  }
  return &it->second;
}

GameStateInfo* GameStateMachine::GetStateInfo(GameStateId id) {
  auto it = states_.find(id);
  if (it == states_.end()) {
    return nullptr;
  }
  return &it->second;
}

bool GameStateMachine::IsActive(GameStateId state) const {
  auto state_info = GetStateInfo(state);
  return state_info != nullptr ? state_info->active : false;
}

GameState* GameStateMachine::GetActiveState(GameStateId state) {
  auto state_info = GetStateInfo(state);
  if (state_info == nullptr || !state_info->active) {
    return nullptr;
  }
  return state_info->instance.get();
}

bool GameStateMachine::ChangeState(GameStateId parent, GameStateId state) {
  // Validate the parent.
  GameStateInfo* parent_info = nullptr;
  if (parent != kNoGameState) {
    parent_info = GetStateInfo(parent);
    if (parent_info == nullptr) {
      LOG(ERROR) << "GameStateMachine::ChangeState(" << GetGameStateName(parent)
                 << ", " << GetGameStateName(state)
                 << "): Parent state is not registered.";
      if (error_callback_ != nullptr) {
        error_callback_({GameStateErrorInfo::kInvalidParent, parent, state});
      }
      return false;
    }
    if (!parent_info->active) {
      LOG(ERROR) << "GameStateMachine::ChangeState(" << GetGameStateName(parent)
                 << ", " << GetGameStateName(state)
                 << "): Parent state is not active.";
      if (error_callback_ != nullptr) {
        error_callback_({GameStateErrorInfo::kInvalidParent, parent, state});
      }
      return false;
    }
  }

  // Validate the new state.
  GameStateInfo* state_info = nullptr;
  if (state != kNoGameState) {
    state_info = GetStateInfo(state);
    if (state_info == nullptr) {
      LOG(ERROR) << "GameStateMachine::ChangeState(" << GetGameStateName(parent)
                 << ", " << GetGameStateName(state)
                 << "): new state is not registered.";
      if (error_callback_ != nullptr) {
        error_callback_({GameStateErrorInfo::kInvalidState, parent, state});
      }
      return false;
    }
    if (state_info->active) {
      LOG(ERROR) << "GameStateMachine::ChangeState(" << GetGameStateName(parent)
                 << ", " << GetGameStateName(state)
                 << "): new state is already active.";
      if (error_callback_ != nullptr) {
        error_callback_({GameStateErrorInfo::kInvalidState, parent, state});
      }
      return false;
    }
  }

  // Validate the new state can be parented as requested.
  if (parent != kNoGameState &&
      state_info->valid_parents_type != GameStateList::kAll) {
    bool valid = true;
    if (state_info->valid_parents_type == GameStateList::kNone) {
      valid = false;
    } else {
      CHECK_EQ(state_info->valid_parents_type, GameStateList::kExplicit);
      valid = (std::find(state_info->valid_parents.begin(),
                         state_info->valid_parents.end(),
                         parent) != state_info->valid_parents.end());
    }
    if (!valid) {
      LOG(ERROR) << "GameStateMachine::ChangeState(" << GetGameStateName(parent)
                 << ", " << GetGameStateName(state)
                 << "): parent is not valid for new state.";
      if (error_callback_ != nullptr) {
        error_callback_({GameStateErrorInfo::kInvalidParent, parent, state});
      }
      return false;
    }
  }

  if (logging_enabled_) {
    if (transition_) {
      LOG(INFO) << "GameState: Aborting " << GetStatePath(parent, state);
    }
    LOG(INFO) << "GameState: Request " << GetCurrentStatePath() << " to "
              << GetStatePath(parent, state);
  }
  transition_ = true;
  transition_parent_ = parent_info;
  transition_state_ = state_info;
  return true;
}

void GameStateMachine::Update(absl::Duration delta_time) {
  static int64_t update_id = 0;
  ++update_id;

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
        state_info->instance->OnUpdate(delta_time);
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
  // Cache current request
  GameStateInfo* parent_info = transition_parent_;
  GameStateInfo* new_state_info = transition_state_;
  transition_ = false;
  transition_parent_ = nullptr;
  transition_state_ = nullptr;

  // Find states that need to exit.
  GameStateInfo* exit_info = top_state_;
  if (exit_info != nullptr) {
    while (exit_info->child != nullptr) {
      exit_info = exit_info->child;
    }
  }

  // Exit all the states, aborting if a new transition is initiated.
  while (exit_info != parent_info) {
    if (logging_enabled_) {
      LOG(INFO) << "GameState: OnExit "
                << GetStatePath(exit_info->id, kNoGameState);
    }
    exit_info->instance->OnExit();

    // Complete the context.
    if (!exit_info->instance->context_.Assign(ValidatedContext{})) {
      LOG(ERROR) << "GameStateMachine::ProcessTransition: "
                 << GetGameStateName(exit_info->id)
                 << ".OnExit: Context could not complete.";
      if (error_callback_ != nullptr) {
        error_callback_({GameStateErrorInfo::kConstraintFailure,
                         (exit_info->parent != nullptr ? exit_info->parent->id
                                                       : kNoGameState),
                         exit_info->id});
      }
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
    if (exit_info->lifetime == GameStateLifetime::Type::kActive) {
      exit_info->instance.reset();
    }

    // Now notify the parent that the child exited.
    if (exit_parent != nullptr) {
      if (logging_enabled_) {
        LOG(INFO) << "GameState: OnChildExit "
                  << GetStatePath(exit_parent->id, exit_info->id);
      }
      exit_parent->instance->OnChildExit(exit_info->id);
    }

    // If a new transition was queued, start over.
    if (transition_) {
      return;
    }

    exit_info = exit_parent;
  }

  // Is there a new state?
  if (new_state_info == nullptr) {
    return;
  }

  // Validate the context for the new state.
  ValidatedContext new_context(context_, new_state_info->constraints);
  if (!new_context.IsValid()) {
    LOG(ERROR) << "GameStateMachine::ProcessTransition: "
               << GetGameStateName(new_state_info->id)
               << ".OnEnter: Context is not valid.";
    if (error_callback_ != nullptr) {
      error_callback_(
          {GameStateErrorInfo::kConstraintFailure,
           (parent_info != nullptr ? parent_info->id : kNoGameState),
           new_state_info->id});
    }
    return;
  }

  // Notify the parent the new state is going to get created.
  if (parent_info != nullptr) {
    if (logging_enabled_) {
      LOG(INFO) << "GameState: OnChildEnter "
                << GetStatePath(parent_info->id, new_state_info->id);
    }
    parent_info->instance->OnChildEnter(new_state_info->id);
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
    CreateInstance(new_state_info);
  }

  // Notify the new state that it is entered.
  if (logging_enabled_) {
    LOG(INFO) << "GameState: OnChildEnter "
              << GetStatePath(new_state_info->id, kNoGameState);
  }
  new_state_info->instance->OnEnter();
}

void GameStateMachine::CreateInstance(GameStateInfo* state_info) {
  auto state = state_info->factory();
  state->info_ = state_info;
  state->machine_ = this;
  state_info->instance = std::move(state);
}

void GameStateMachine::DoRegister(
    GameStateId id, GameStateLifetime::Type lifetime,
    GameStateList::Type valid_parents_type,
    std::vector<GameStateId> valid_parents,
    std::vector<ContextConstraint> constraints,
    std::function<std::unique_ptr<GameState>()> factory) {
  CHECK(states_.find(id) == states_.end())
      << "State " << GetGameStateName(id) << " already registered.";
  auto& state_info = states_[id];
  state_info.id = id;
  state_info.lifetime = lifetime;
  state_info.valid_parents_type = valid_parents_type;
  state_info.valid_parents = std::move(valid_parents);
  state_info.constraints = std::move(constraints);
  state_info.factory = std::move(factory);
  if (lifetime == GameStateLifetime::kGlobal) {
    CreateInstance(&state_info);
  }
}

std::string GameStateMachine::GetStatePath(GameStateId parent,
                                           GameStateId state) {
  std::vector<std::string> names;
  if (parent != kNoGameState) {
    GameStateInfo* current_state = top_state_;
    while (current_state != nullptr && current_state->id != parent) {
      names.emplace_back(GetGameStateName(current_state->id));
      current_state = current_state->child;
    }
    names.emplace_back(GetGameStateName(parent));
  }
  if (state != kNoGameState) {
    names.emplace_back(GetGameStateName(state));
  }
  if (names.empty()) {
    return "<none>";
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
    return "<none>";
  }
  return absl::StrJoin(names, ".");
}

}  // namespace gb
