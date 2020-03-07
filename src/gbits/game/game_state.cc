#include "gbits/game/game_state.h"

#include "gbits/game/game_state_machine.h"
#include "glog/logging.h"

namespace gb {

const char* GetGameStateName(GameStateId id) {
  if (id == kNoGameStateId) {
    return "kNoGameStateId";
  }
  return id->GetTypeName();
}

void SetGameStateName(GameStateId id, const char* name) {
  id->SetTypeName(name);
}

GameStateId GameState::GetId() const { return info_->id; }
GameStateId GameState::GetParentId() const {
  return info_->parent != nullptr ? info_->parent->id : kNoGameStateId;
}
GameState* GameState::GetParent() const {
  return info_->parent != nullptr ? info_->parent->instance.get() : nullptr;
}
GameStateId GameState::GetChildId() const {
  return info_->child != nullptr ? info_->child->id : kNoGameStateId;
}
GameState* GameState::GetChild() const {
  return info_->child != nullptr ? info_->child->instance.get() : nullptr;
}

bool GameState::ChangeChildState(GameStateId state) {
  if (machine_ == nullptr) {
    LOG(ERROR) << "GameState::ChangeChildState: " << GetGameStateName(info_->id)
               << ": Cannot change child to state " << GetGameStateName(state)
               << " as the state is not active.";
    return false;
  }
  return machine_->ChangeState(info_->id, state);
}

bool GameState::ChangeState(GameStateId state) {
  if (machine_ == nullptr) {
    LOG(ERROR) << "GameState::ChangeState: " << GetGameStateName(info_->id)
               << ": Cannot change to sibling state " << GetGameStateName(state)
               << " as the state is not active.";
    return false;
  }
  return machine_->ChangeState(GetParentId(), state);
}

bool GameState::ExitState() {
  if (machine_ == nullptr) {
    LOG(ERROR) << "GameState::ExitState: " << GetGameStateName(info_->id)
               << ": Cannot exit state as the state is not active.";
    return false;
  }
  return machine_->ChangeState(GetParentId(), kNoGameStateId);
}

}  // namespace gb
