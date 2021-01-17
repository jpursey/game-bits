// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/sdl/sdl_glog.h"

#include "glog/logging.h"

namespace gb {

void SdlToGLog(void* user_data, int category, SDL_LogPriority priority,
               const char* message) {
  const char* category_string;
  switch (category) {
    case SDL_LOG_CATEGORY_APPLICATION:
      category_string = "SDL(app) ";
      break;
    case SDL_LOG_CATEGORY_ERROR:
      category_string = "SDL(error) ";
      break;
    case SDL_LOG_CATEGORY_ASSERT:
      category_string = "SDL(assert) ";
      break;
    case SDL_LOG_CATEGORY_SYSTEM:
      category_string = "SDL(system) ";
      break;
    case SDL_LOG_CATEGORY_AUDIO:
      category_string = "SDL(audio) ";
      break;
    case SDL_LOG_CATEGORY_VIDEO:
      category_string = "SDL(video) ";
      break;
    case SDL_LOG_CATEGORY_RENDER:
      category_string = "SDL(render) ";
      break;
    case SDL_LOG_CATEGORY_INPUT:
      category_string = "SDL(input) ";
      break;
    case SDL_LOG_CATEGORY_TEST:
      category_string = "SDL(test) ";
      break;
    default:
      category_string = "SDL(other) ";
  }
  switch (priority) {
    case SDL_LOG_PRIORITY_VERBOSE:
      VLOG(1) << category_string << message;
      break;
    case SDL_LOG_PRIORITY_DEBUG:
    case SDL_LOG_PRIORITY_INFO:
      LOG(INFO) << category_string << message;
      break;
    case SDL_LOG_PRIORITY_WARN:
      LOG(WARNING) << category_string << message;
      break;
    case SDL_LOG_PRIORITY_ERROR:
      LOG(ERROR) << category_string << message;
      break;
    case SDL_LOG_PRIORITY_CRITICAL:
      LOG(DFATAL) << category_string << message;
      break;
    default:
      break;
  }
}

}  // namespace gb
