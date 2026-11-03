// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PILOT_FAKE_PILOT_H_
#define GB_PILOT_FAKE_PILOT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gb/image/image.h"
#include "gb/pilot/pilot.h"

namespace gb {

// A Pilot for tests, which does what the test sets.
//
// It is never done or failed until the test says so. Each frame has the input
// the test set for it, and the mouse is wherever the test last put it. While
// the test has it saving frames, every frame is saved, and it keeps a copy of
// each.
//
// Its state changes only when the test sets it, so the test keeps the rules
// that Pilot sets for subclasses (such as not changing the mouse during a
// frame, and being done once failed) as far as the code it tests needs.
//
// RunCommand(), CheckCondition(), and FindItem() are public, so a test can
// check what a program or GUI registered.
//
// This class is thread-compatible.
class FakePilot final : public Pilot {
 public:
  //----------------------------------------------------------------------------
  // State
  //----------------------------------------------------------------------------

  void SetDone(bool done) { done_ = done; }
  void SetFailed(bool failed) { failed_ = failed; }

  // The delta time is 1/60th of a second, unless set.
  void SetDeltaTime(absl::Duration delta_time) { delta_time_ = delta_time; }

  void SetWindowHidden(bool hidden) { window_hidden_ = hidden; }
  void SetKeyNames(std::vector<std::string> key_names) {
    key_names_ = std::move(key_names);
  }

  //----------------------------------------------------------------------------
  // Input
  //----------------------------------------------------------------------------

  // Sets the input of the next frame to begin, which it has for that frame
  // only.
  void SetNextFrameInput(std::vector<PilotInput> input) {
    next_input_ = std::move(input);
  }

  void SetMouse(PilotMouse mouse) { mouse_ = mouse; }

  //----------------------------------------------------------------------------
  // Saving frames
  //----------------------------------------------------------------------------

  // Sets whether each frame is saved.
  void SetSavingFrames(bool saving) { saving_ = saving; }

  // Returns copies of the frames saved since the last call, in order.
  std::vector<std::unique_ptr<Image>> TakeSavedFrames() {
    return std::exchange(saved_frames_, {});
  }

  //----------------------------------------------------------------------------
  // Registration
  //----------------------------------------------------------------------------

  using Pilot::CheckCondition;
  using Pilot::FindItem;
  using Pilot::RunCommand;

 protected:
  void DoBeginFrame() override;
  void DoEndFrame() override;
  bool DoIsDone() const override { return done_; }
  bool DoHasFailed() const override { return failed_; }
  absl::Duration DoGetDeltaTime() const override { return delta_time_; }
  bool DoIsWindowHidden() const override { return window_hidden_; }
  absl::Span<const std::string> DoGetKeyNames() const override {
    return key_names_;
  }
  absl::Span<const PilotInput> DoGetInput() const override { return input_; }
  PilotMouse DoGetMouse() const override { return mouse_; }
  bool DoIsSavingFrame() const override { return saving_; }
  void DoSaveFrame(const Image& image) override;

 private:
  bool done_ = false;
  bool failed_ = false;
  absl::Duration delta_time_ = absl::Seconds(1) / 60;
  bool window_hidden_ = false;
  std::vector<std::string> key_names_;
  std::vector<PilotInput> next_input_;
  std::vector<PilotInput> input_;
  PilotMouse mouse_;
  bool saving_ = false;
  std::vector<std::unique_ptr<Image>> saved_frames_;
};

}  // namespace gb

#endif  // GB_PILOT_FAKE_PILOT_H_
