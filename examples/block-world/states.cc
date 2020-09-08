// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "states.h"

// Game includes
#include "play_state.h"
#include "title_state.h"

#define REGISTER_STATE(Class)          \
  gb::SetGameStateName<Class>(#Class); \
  state_machine->Register<Class>();

void RegisterStates(gb::GameStateMachine* state_machine) {
  REGISTER_STATE(TitleState);
  REGISTER_STATE(PlayState);
}
