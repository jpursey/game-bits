#ifndef TITLE_STATE_H_
#define TITLE_STATE_H_

#include "base_state.h"
#include "states.h"

class TitleState : public BaseState {
 public:
  using ParentStates = gb::NoGameStates;
  using SiblingStates = gb::GameStates<PlayState>;

  TitleState() = default;
  ~TitleState() override = default;

 protected:
  void OnEnter() override;
  void OnUpdate(absl::Duration delta_time) override;
  bool OnSdlEvent(const SDL_Event& event) override;
};

#endif  // TITLE_STATE_H_
