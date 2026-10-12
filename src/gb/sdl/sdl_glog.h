// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_SDL_SDL_GLOG_H_
#define GB_SDL_SDL_GLOG_H_

#include "SDL.h"

namespace gb {

// SDL log output function which sends SDL logs to Abseil logging. Install it
// with SDL_LogSetOutputFunction(SdlToGLog, nullptr).
//
// Each message is prefixed with its SDL category ("SDL(video) ..."). SDL's
// verbose priority logs as VLOG(1), debug and info as INFO, warn as WARNING,
// error as ERROR, and critical as DFATAL, which ends a debug build.
void SdlToGLog(void* user_data, int category, SDL_LogPriority priority,
               const char* message);

}  // namespace gb

#endif  // GB_SDL_SDL_GLOG_H_
