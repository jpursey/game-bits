#include "gbits/game/game_state_machine.h"

#include "glog/logging.h"

namespace gb {

bool GameStateMachine::IsActive(GameStateId state) const {
  auto it = states_.find(state);
  if (it == states_.end()) {
    return nullptr;
  }
  return it->second.active;
}

GameState* GameStateMachine::GetActiveState(GameStateId state) {
  auto it = states_.find(state);
  if (it == states_.end() || !it->second.active) {
    return nullptr;
  }
  return it->second.instance.get();
}

bool GameStateMachine::ChangeState(GameStateId parent, GameStateId state) {
  // TODO
  return false;
}

void GameStateMachine::Update(absl::Duration delta_time) {
  // TODO
}

void GameStateMachine::DoRegister(
    GameStateId key, GameStateLifetime::Type lifetime,
    std::vector<ContextConstraint> constraints,
    std::function<std::unique_ptr<GameState>()> factory) {
  CHECK(states_.find(key) != states_.end())
      << "State " << GetGameStateName(key) << " already registered.";
  auto& state_info = states_[key];
  state_info.lifetime = lifetime;
  state_info.constraints = std::move(constraints);
  state_info.factory = std::move(factory);
  if (lifetime == GameStateLifetime::kGlobal) {
    state_info.instance = state_info.factory();
  }
}

}  // namespace gb
