// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PILOT_STEP_PILOT_H_
#define GB_PILOT_STEP_PILOT_H_

#include <deque>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gb/image/image.h"
#include "gb/pilot/pilot.h"

namespace gb {

//==============================================================================
// Steps
//
// Each step is one line of a pilot script. StepPilot says when each one runs,
// and how long it takes.
//
// Each step, and PilotStep, converts to the script line it stands for wherever
// Abseil converts a value to text (AbslStringify), such as absl::StrCat(),
// absl::StrFormat()'s "%v", and LOG(): `click right "OK"`. Names are written
// bare if they are a C identifier or an integer, and quoted otherwise. A script
// reads each line back as the same step (see gb_pilot_script).
//==============================================================================

namespace internal {

// Returns a name bare if it is a C identifier or an integer as written in
// decimal, and quoted otherwise.
std::string FormatPilotName(std::string_view name);

// Returns text quoted, escaping '"' and '\' with a '\', and control characters
// as \n, \t, or \xHH.
std::string QuotePilotText(std::string_view text);

}  // namespace internal

// Waits for a number of frames.
struct PilotWaitStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotWaitStep& step) {
    absl::Format(&sink, "wait %d", step.frames);
  }

  int frames = 0;
};

// Waits until a program condition holds (see Pilot::AddCondition()).
struct PilotWaitForStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotWaitForStep& step) {
    absl::Format(&sink, "wait for %s %d",
                 internal::FormatPilotName(step.condition), step.max_frames);
  }

  std::string condition;

  // The step fails if the condition doesn't hold after this many frames.
  int max_frames = 600;
};

// Presses a key, and releases it in the next frame.
struct PilotKeyStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotKeyStep& step) {
    absl::Format(&sink, "key %s", internal::FormatPilotName(step.key));
  }

  // The key, by the name SDL gives its scancode (see PilotKeyEvent).
  std::string key;
};

// Presses a key, and releases it a number of frames later.
struct PilotHoldStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotHoldStep& step) {
    absl::Format(&sink, "hold %s %d", internal::FormatPilotName(step.key),
                 step.frames);
  }

  // The key, by the name SDL gives its scancode (see PilotKeyEvent).
  std::string key;
  int frames = 0;
};

// Types text.
struct PilotTextStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotTextStep& step) {
    absl::Format(&sink, "text %s", internal::QuotePilotText(step.text));
  }

  // UTF-8 text.
  std::string text;
};

// Moves the mouse.
struct PilotMouseMoveStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotMouseMoveStep& step) {
    absl::Format(&sink, "move %d %d", step.position.x, step.position.y);
  }

  PilotPoint position;
};

// Presses a mouse button, and releases it in the next frame.
struct PilotMouseClickStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotMouseClickStep& step) {
    sink.Append("click");
    if (step.button == PilotMouseButton::kMiddle) {
      sink.Append(" middle");
    } else if (step.button == PilotMouseButton::kRight) {
      sink.Append(" right");
    }
    if (const auto* point = std::get_if<PilotPoint>(&step.target)) {
      absl::Format(&sink, " %d %d", point->x, point->y);
    } else if (const auto* label = std::get_if<std::string>(&step.target)) {
      absl::Format(&sink, " %s", internal::QuotePilotText(*label));
    }
  }

  PilotMouseButton button = PilotMouseButton::kLeft;

  // Where to click: where the mouse is (std::monostate), at a point, or at the
  // item with a label, as drawn in the last frame (see Pilot::AddItemFinder()).
  // The mouse moves there first.
  std::variant<std::monostate, PilotPoint, std::string> target;
};

// Turns the mouse wheel.
struct PilotMouseWheelStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotMouseWheelStep& step) {
    absl::Format(&sink, "wheel %d", step.amount);
  }

  // Steps of the wheel, positive away from the user.
  int amount = 0;
};

// Runs a program command (see Pilot::AddCommand()).
struct PilotRunStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotRunStep& step) {
    absl::Format(&sink, "run %s", internal::FormatPilotName(step.command));
    for (const std::string& arg : step.args) {
      absl::Format(&sink, " %s", internal::FormatPilotName(arg));
    }
  }

  std::string command;
  std::vector<std::string> args;
};

// Fails unless a program condition holds (see Pilot::AddCondition()).
struct PilotExpectStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotExpectStep& step) {
    absl::Format(&sink, "expect %s", internal::FormatPilotName(step.condition));
  }

  std::string condition;
};

// Expects errors whose message contains some text, for a check that is meant
// to fail. From when the step starts, for `max_frames` frames (counting the one
// it starts in, and at least that one), such errors are logged as expected
// rather than failing the run. The step fails if none is logged in those
// frames, or before the run ends, and if the text is empty. It takes no time,
// so it goes just before the step whose check logs the error.
struct PilotExpectErrorStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotExpectErrorStep& step) {
    absl::Format(&sink, "expect error %s %d",
                 internal::QuotePilotText(step.text), step.max_frames);
  }

  std::string text;
  int max_frames = 600;
};

// Saves the next frame drawn, giving it to the frame saver under a name.
struct PilotSaveStep {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PilotSaveStep& step) {
    absl::Format(&sink, "save %s", internal::FormatPilotName(step.name));
  }

  std::string name;
};

// One step of a run.
using PilotStep =
    std::variant<PilotWaitStep, PilotWaitForStep, PilotKeyStep, PilotHoldStep,
                 PilotTextStep, PilotMouseMoveStep, PilotMouseClickStep,
                 PilotMouseWheelStep, PilotRunStep, PilotExpectStep,
                 PilotExpectErrorStep, PilotSaveStep>;

// Writes whichever step it holds. Abseil finds this through the steps' own
// namespace, as PilotStep is a std::variant of them.
template <typename Sink>
void AbslStringify(Sink& sink, const PilotStep& step) {
  std::visit(
      [&sink](const auto& alternative) { AbslStringify(sink, alternative); },
      step);
}

//==============================================================================
// StepPilot
//==============================================================================

// A Pilot that runs a list of steps given to it in code.
//
// It needs nothing beyond this library, so a program can drive itself with
// steps it builds, read, or receives, with no script file:
//
//   StepPilot pilot({.save_frame = [&](std::string_view name,
//                                      const Image& image) {
//     return SaveImage(&file_system, absl::StrCat("out/", name, ".png"),
//                      image.View());
//   }});
//   pilot.AddSteps({PilotWaitStep{.frames = 30},
//                   PilotMouseClickStep{.target = "Get layer 1"},
//                   PilotWaitStep{.frames = 2},
//                   PilotSaveStep{.name = "get_layer_1"}});
//
// The program then runs its frames as Pilot says.
//
// At the start of each frame, steps run in order until one that takes time.
// Every other step takes none, so a run of them happens within the one frame,
// in order. The step after one that takes time starts:
// - PilotWaitStep: `frames` frames later (in the same frame, if 0 or less).
// - PilotKeyStep and PilotMouseClickStep: in the next frame, after releasing
//   what they pressed. PilotHoldStep: `frames` frames later, likewise.
// - PilotWaitForStep: as soon as the condition holds. It is checked when the
//   step starts, and after each frame, and the step fails if it doesn't hold
//   after `max_frames` frames.
// - PilotSaveStep: in the frame after the one saved, which is the frame the
//   step starts in, or if that isn't drawn, the next one that is.
//
// The run is done once its last step has finished, or at its first failure. A
// run fails if:
// - A command or condition isn't registered, a command fails, or an expected
//   condition doesn't hold.
// - A click's label is on no item, or on more than one.
// - A save has no frame saver, the frame saver fails, or no frame is drawn
//   within 600 frames.
// - An error is logged, on any thread, from when the pilot is created until it
//   is done, that no PilotExpectErrorStep expects. The program's own errors
//   fail the run, as do the errors of whatever takes part in it (such as a key
//   name that can't be mapped).
// - An expected error isn't logged.
//
// Each step is logged as it starts, with its frame, and a failure is logged as
// an error. A summary is logged once the run is done: whether it passed, the
// frames run, and the frames saved.
//
// Errors are kept from the whole program, so only one StepPilot should run at
// a time: each fails on the errors of any other, including its failures.
//
// This class is thread-compatible, like Pilot. Only the keeping of logged
// errors is thread-safe.
class StepPilot final : public Pilot {
 public:
  // Keeps a saved frame under its name. Returns false, having logged why, if it
  // can't, which fails the run.
  using FrameSaver =
      absl::AnyInvocable<bool(std::string_view name, const Image& image)>;

  struct Options {
    // The game time each frame stands for.
    absl::Duration delta_time = absl::Seconds(1) / 60;

    // Whether the program's window is hidden for the run.
    bool hide_window = false;

    // Needed only if a step saves a frame.
    FrameSaver save_frame;
  };

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  StepPilot();
  explicit StepPilot(Options options);
  ~StepPilot() override;

  //----------------------------------------------------------------------------
  // Steps
  //----------------------------------------------------------------------------

  // Adds steps to run after those already added. Steps may be added until the
  // run is done, including by a command or condition as it runs. Steps added
  // once it is done log an error, and are ignored.
  void AddSteps(std::vector<PilotStep> steps);

 protected:
  void DoBeginFrame() override;
  void DoEndFrame() override;
  bool DoIsDone() const override { return done_; }
  bool DoHasFailed() const override { return failed_; }
  absl::Duration DoGetDeltaTime() const override { return options_.delta_time; }
  bool DoIsWindowHidden() const override { return options_.hide_window; }
  absl::Span<const std::string> DoGetKeyNames() const override {
    return key_names_;
  }
  absl::Span<const PilotInput> DoGetInput() const override { return input_; }
  PilotMouse DoGetMouse() const override { return mouse_; }
  bool DoIsSavingFrame() const override { return saving_; }
  void DoSaveFrame(const Image& image) override;

 private:
  // Keeps the messages of the errors logged, on any thread.
  class ErrorLog final : public absl::LogSink {
   public:
    // Returns the errors logged since the last call, oldest first.
    std::vector<std::string> TakeErrors();

    void Send(const absl::LogEntry& entry) override;

   private:
    absl::Mutex mutex_;
    std::vector<std::string> errors_ ABSL_GUARDED_BY(mutex_);
  };

  // A PilotExpectErrorStep's expectation, which lasts beyond the step.
  struct ErrorExpectation {
    PilotExpectErrorStep step;
    int last_frame = 0;  // The frame it ends with.
    bool met = false;
  };

  // Runs the step in progress at the start of a frame, starting it if it
  // hasn't started. Returns true if it has finished, or false if it takes more
  // time (or failed).
  bool RunStep(const PilotWaitStep& step);
  bool RunStep(const PilotWaitForStep& step);
  bool RunStep(const PilotKeyStep& step);
  bool RunStep(const PilotHoldStep& step);
  bool RunStep(const PilotTextStep& step);
  bool RunStep(const PilotMouseMoveStep& step);
  bool RunStep(const PilotMouseClickStep& step);
  bool RunStep(const PilotMouseWheelStep& step);
  bool RunStep(const PilotRunStep& step);
  bool RunStep(const PilotExpectStep& step);
  bool RunStep(const PilotExpectErrorStep& step);
  bool RunStep(const PilotSaveStep& step);

  // Runs steps from the one in progress, until one takes time.
  void RunSteps();

  // Ends the frame for the step in progress, finishing it if it is done.
  void EndStepFrame();

  // Checks a wait for step's condition, returning true if it holds.
  bool CheckWaitFor(const PilotWaitForStep& step);

  // Presses a key, and releases it `frames` frames later. Returns true once
  // released.
  bool PressKey(std::string_view key, int frames);

  void MoveMouse(PilotPoint position);
  void SetMouseButton(PilotMouseButton button, bool down);

  // Fails the run on an error logged since the last call that no expectation
  // in its frames expects, and marks the expectations each error meets.
  void ValidateLoggedErrors();

  // Ends the frame for each expectation, failing the run on one whose frames
  // have ended, or on any if the run is ending, that no error met.
  void EndExpectationFrames(bool run_ending);

  void FinishStep();
  void FailStep(std::string_view reason);
  void Fail(std::string_view reason);

  Options options_;
  ErrorLog error_log_;
  std::vector<ErrorExpectation> expectations_;

  // Steps still to finish, the first being the one in progress. Steps may be
  // added while one runs, and a deque keeps references to them valid.
  std::deque<PilotStep> steps_;
  bool step_started_ = false;
  int step_frames_ = 0;            // Frames ended since the step started.
  bool step_frame_saved_ = false;  // Whether a save step's frame was saved.

  std::vector<std::string> key_names_;
  std::vector<PilotInput> input_;
  PilotMouse mouse_;
  bool saving_ = false;
  int frames_saved_ = 0;
  bool failed_ = false;
  bool done_ = false;
};

}  // namespace gb

#endif  // GB_PILOT_STEP_PILOT_H_
