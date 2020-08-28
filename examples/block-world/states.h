#ifndef STATES_H_
#define STATES_H_

#include "gb/game/game_state_machine.h"

class TitleState;
class PlayState;

void RegisterStates(gb::GameStateMachine* state_machine);

#endif  // STATES_H_
