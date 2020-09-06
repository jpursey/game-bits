// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef STATES_H_
#define STATES_H_

#include "gb/game/game_state_machine.h"

class TitleState;
class PlayState;

void RegisterStates(gb::GameStateMachine* state_machine);

#endif  // STATES_H_
