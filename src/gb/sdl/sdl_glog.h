// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "SDL.h"

namespace gb {

// SDL log output function which translates SDL logs to glog logs.
void SdlToGLog(void* user_data, int category, SDL_LogPriority priority,
               const char* message);

}  // namespace gb
