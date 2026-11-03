// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/fake_pilot.h"

#include <utility>

namespace gb {

void FakePilot::DoBeginFrame() { input_ = std::exchange(next_input_, {}); }

void FakePilot::DoEndFrame() { input_.clear(); }

void FakePilot::DoSaveFrame(const Image& image) {
  auto copy = std::make_unique<Image>(image.GetWidth(), image.GetHeight());
  copy->Edit().ModifyRegion().SetAll(image.GetPixels());
  saved_frames_.push_back(std::move(copy));
}

}  // namespace gb
