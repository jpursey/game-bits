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

void GameStateMachine::CreateInstance(GameStateId id,
                                      GameStateInfo* state_info) {
  auto state = state_info->factory();
  state->id_ = id;
  state->machine_ = this;
  state_info->instance = std::move(state);
}

void GameStateMachine::DoRegister(
    GameStateId id, GameStateLifetime::Type lifetime,
    GameStateList::Type valid_parents_type,
    std::vector<GameStateId> valid_parents,
    std::vector<ContextConstraint> constraints,
    std::function<std::unique_ptr<GameState>()> factory) {
  CHECK(states_.find(id) != states_.end())
      << "State " << GetGameStateName(id) << " already registered.";
  auto& state_info = states_[id];
  state_info.lifetime = lifetime;
  state_info.valid_parents_type = valid_parents_type;
  state_info.valid_parents = std::move(valid_parents);
  state_info.constraints = std::move(constraints);
  state_info.factory = std::move(factory);
  if (lifetime == GameStateLifetime::kGlobal) {
    CreateInstance(id, &state_info);
  }
}

}  // namespace gb
