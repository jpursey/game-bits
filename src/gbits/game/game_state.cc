#include "gbits/game/game_state.h"

#include "gbits/game/game_state_machine.h"
#include "glog/logging.h"

namespace gb {

const char* GetGameStateName(GameStateId id) {
  if (id == kNoGameState) {
    return "kNoGameState";
  }
  return id->GetTypeName();
}

bool GameState::ChangeChildState(GameStateId state) {
  if (machine_ == nullptr) {
    LOG(ERROR) << GetGameStateName(id_) << ": Cannot change child to state "
               << GetGameStateName(state) << " as the state is not active.";
    return false;
  }
  return machine_->ChangeState(id_, state);
}

bool GameState::ChangeState(GameStateId state) {
  if (machine_ == nullptr) {
    LOG(ERROR) << GetGameStateName(id_) << ": Cannot change to sibling state "
               << GetGameStateName(state) << " as the state is not active.";
    return false;
  }
  GameStateId parent = (parent_ != nullptr ? parent_->id_ : kNoGameState);
  return machine_->ChangeState(parent, state);
}

bool GameState::ChangeTopState(GameStateId state) {
  if (machine_ == nullptr) {
    LOG(ERROR) << GetGameStateName(id_) << ": Cannot change to sibling state "
               << GetGameStateName(state) << " as the state is not active.";
    return false;
  }
  return machine_->ChangeTopState(state);
}

bool GameState::ExitState() {
  if (machine_ == nullptr) {
    LOG(ERROR) << GetGameStateName(id_)
               << ": Cannot exit state as the state is not active.";
    return false;
  }
}

}  // namespace gb
