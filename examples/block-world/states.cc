#include "states.h"

#include "play_state.h"
#include "title_state.h"

#define REGISTER_STATE(Class)          \
  gb::SetGameStateName<Class>(#Class); \
  state_machine->Register<Class>();

void RegisterStates(gb::GameStateMachine* state_machine) {
  REGISTER_STATE(TitleState);
  REGISTER_STATE(PlayState);
}
