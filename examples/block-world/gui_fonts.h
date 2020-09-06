// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GUI_FONTS_H_
#define GUI_FONTS_H_

#include "imgui.h"

struct GuiFonts {
  GuiFonts() = default;

  ImFont* title = nullptr;
  ImFont* prompt = nullptr;
  ImFont* console = nullptr;
};

#endif  // GUI_FONTS_H_
