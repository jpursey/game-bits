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

GameStateId GameState::GetId() const { return info_->GetId(); }
GameStateMachine* GameState::GetStateMachine() const {
  return info_->GetStateMachine();
}
GameStateId GameState::GetParentId() const { return info_->GetParentId(); }
GameState* GameState::GetParent() const { return info_->GetParent(); }
GameStateId GameState::GetChildId() const { return info_->GetChildId(); }
GameState* GameState::GetChild() const { return info_->GetChild(); }

bool GameState::ChangeChildState(GameStateId state) {
  return info_->GetStateMachine()->ChangeState(info_->GetId(), state);
}

bool GameState::ChangeState(GameStateId state) {
  return info_->GetStateMachine()->ChangeState(GetParentId(), state);
}

bool GameState::ExitState() {
  return info_->GetStateMachine()->ChangeState(GetParentId(), kNoGameStateId);
}

}  // namespace gb
