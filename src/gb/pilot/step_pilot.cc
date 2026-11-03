// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/step_pilot.h"

#include <algorithm>
#include <utility>

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "absl/log/log_sink_registry.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"

namespace gb {
namespace {

// A save step fails if no frame is drawn within this many frames.
constexpr int kMaxSaveFrames = 600;

}  // namespace

//==============================================================================
// Step formatting
//==============================================================================

namespace internal {

std::string FormatPilotName(std::string_view name) {
  const bool is_word =
      !name.empty() && std::all_of(name.begin(), name.end(), [](char ch) {
        return absl::ascii_isalnum(ch) || ch == '_' || ch == '-';
      });
  return is_word ? std::string(name) : QuotePilotText(name);
}

std::string QuotePilotText(std::string_view text) {
  return absl::StrCat("\"", absl::CEscape(text), "\"");
}

}  // namespace internal

//==============================================================================
// StepPilot::ErrorCounter
//==============================================================================

void StepPilot::ErrorCounter::Send(const absl::LogEntry& entry) {
  if (entry.log_severity() >= absl::LogSeverity::kError) {
    ++count_;
  }
}

//==============================================================================
// StepPilot
//==============================================================================

StepPilot::StepPilot() : StepPilot(Options()) {}

StepPilot::StepPilot(Options options) : options_(std::move(options)) {
  absl::AddLogSink(&error_counter_);
}

StepPilot::~StepPilot() {
  // Removing the sink waits for any error being counted on another thread.
  absl::RemoveLogSink(&error_counter_);
}

void StepPilot::AddSteps(std::vector<PilotStep> steps) {
  if (done_) {
    LOG(ERROR) << "Pilot steps were added after the run was done, so they are "
                  "ignored.";
    return;
  }
  for (PilotStep& step : steps) {
    std::string_view key;
    if (const auto* key_step = std::get_if<PilotKeyStep>(&step)) {
      key = key_step->key;
    } else if (const auto* hold_step = std::get_if<PilotHoldStep>(&step)) {
      key = hold_step->key;
    }
    if (!key.empty() && std::find(key_names_.begin(), key_names_.end(), key) ==
                            key_names_.end()) {
      key_names_.emplace_back(key);
    }
    steps_.push_back(std::move(step));
  }
}

void StepPilot::DoBeginFrame() {
  input_.clear();
  saving_ = false;
  FailOnLoggedError();
  RunSteps();
}

void StepPilot::DoEndFrame() {
  if (done_) {
    return;
  }
  FailOnLoggedError();
  if (!failed_ && step_started_) {
    EndStepFrame();
  }
  if (failed_ || steps_.empty()) {
    done_ = true;
    LOG(INFO) << "Pilot run " << (failed_ ? "failed" : "passed")
              << ". Frames run: " << GetFrame() + 1
              << ". Frames saved: " << frames_saved_ << ".";
  }
}

void StepPilot::DoSaveFrame(const Image& image) {
  // Pilot only checks that the frame is being saved in Debug, and a save step
  // is in progress only if it is.
  if (!saving_) {
    return;
  }
  const auto* step = std::get_if<PilotSaveStep>(&steps_.front());
  step_frame_saved_ = true;
  if (!options_.save_frame(step->name, image)) {
    FailStep("The frame saver couldn't keep the frame");
    return;
  }
  ++frames_saved_;
}

void StepPilot::RunSteps() {
  while (!failed_ && !steps_.empty()) {
    const PilotStep& step = steps_.front();
    if (!step_started_) {
      LOG(INFO) << "Pilot frame " << GetFrame() << ": " << step;
      step_started_ = true;
    }
    const bool finished = std::visit(
        [this](const auto& alternative) { return RunStep(alternative); }, step);
    if (failed_ || !finished) {
      return;
    }
    FinishStep();
  }
}

void StepPilot::EndStepFrame() {
  ++step_frames_;
  const PilotStep& step = steps_.front();
  bool finished = false;
  if (const auto* wait = std::get_if<PilotWaitStep>(&step)) {
    finished = step_frames_ >= wait->frames;
  } else if (const auto* wait_for = std::get_if<PilotWaitForStep>(&step)) {
    finished = CheckWaitFor(*wait_for);
  } else if (std::holds_alternative<PilotSaveStep>(step)) {
    finished = step_frame_saved_;
    if (!finished && step_frames_ >= kMaxSaveFrames) {
      FailStep(
          absl::StrCat("No frame was drawn in ", kMaxSaveFrames, " frames"));
    }
  }
  if (!failed_ && finished) {
    FinishStep();
  }
}

bool StepPilot::RunStep(const PilotWaitStep& step) {
  // Once started, a wait finishes at the end of a frame (in EndStepFrame()).
  return step_frames_ >= step.frames;
}

bool StepPilot::RunStep(const PilotWaitForStep& step) {
  // Once started, the condition is checked at the end of each frame (in
  // EndStepFrame()).
  return step_frames_ == 0 && CheckWaitFor(step);
}

bool StepPilot::RunStep(const PilotKeyStep& step) {
  return PressKey(step.key, 1);
}

bool StepPilot::RunStep(const PilotHoldStep& step) {
  return PressKey(step.key, step.frames);
}

bool StepPilot::RunStep(const PilotTextStep& step) {
  input_.push_back(PilotTextEvent{.text = step.text});
  return true;
}

bool StepPilot::RunStep(const PilotMouseMoveStep& step) {
  MoveMouse(step.position);
  return true;
}

bool StepPilot::RunStep(const PilotMouseClickStep& step) {
  if (step_frames_ > 0) {
    SetMouseButton(step.button, false);
    return true;
  }
  if (const auto* point = std::get_if<PilotPoint>(&step.target)) {
    MoveMouse(*point);
  } else if (const auto* label = std::get_if<std::string>(&step.target)) {
    absl::StatusOr<PilotPoint> item_point = FindItem(*label);
    if (!item_point.ok()) {
      FailStep(item_point.status().message());
      return false;
    }
    MoveMouse(*item_point);
  }
  SetMouseButton(step.button, true);
  return false;
}

bool StepPilot::RunStep(const PilotMouseWheelStep& step) {
  input_.push_back(PilotMouseWheelEvent{.amount = step.amount});
  return true;
}

bool StepPilot::RunStep(const PilotRunStep& step) {
  absl::Status status = RunCommand(step.command, step.args);
  if (!status.ok()) {
    FailStep(status.message());
    return false;
  }
  return true;
}

bool StepPilot::RunStep(const PilotExpectStep& step) {
  absl::StatusOr<bool> holds = CheckCondition(step.condition);
  if (!holds.ok()) {
    FailStep(holds.status().message());
    return false;
  }
  if (!*holds) {
    FailStep("The condition doesn't hold");
    return false;
  }
  return true;
}

bool StepPilot::RunStep(const PilotSaveStep& step) {
  // A save finishes at the end of the frame saved (in EndStepFrame()).
  if (options_.save_frame == nullptr) {
    FailStep("No frame saver was given");
    return false;
  }
  saving_ = true;
  return false;
}

bool StepPilot::CheckWaitFor(const PilotWaitForStep& step) {
  absl::StatusOr<bool> holds = CheckCondition(step.condition);
  if (!holds.ok()) {
    FailStep(holds.status().message());
    return false;
  }
  if (*holds) {
    return true;
  }
  if (step_frames_ >= step.max_frames) {
    FailStep(absl::StrCat("The condition doesn't hold after ", step.max_frames,
                          " frames"));
  }
  return false;
}

bool StepPilot::PressKey(std::string_view key, int frames) {
  if (step_frames_ == 0) {
    input_.push_back(PilotKeyEvent{.key = std::string(key), .down = true});
  }
  if (step_frames_ < frames) {
    return false;
  }
  input_.push_back(PilotKeyEvent{.key = std::string(key), .down = false});
  return true;
}

void StepPilot::MoveMouse(PilotPoint position) {
  mouse_.position = position;
  input_.push_back(PilotMouseMoveEvent{.position = position});
}

void StepPilot::SetMouseButton(PilotMouseButton button, bool down) {
  switch (button) {
    case PilotMouseButton::kLeft:
      mouse_.left = down;
      break;
    case PilotMouseButton::kMiddle:
      mouse_.middle = down;
      break;
    case PilotMouseButton::kRight:
      mouse_.right = down;
      break;
  }
  input_.push_back(PilotMouseButtonEvent{
      .button = button, .down = down, .position = mouse_.position});
}

void StepPilot::FinishStep() {
  steps_.pop_front();
  step_started_ = false;
  step_frames_ = 0;
  step_frame_saved_ = false;
}

void StepPilot::FailOnLoggedError() {
  if (!done_ && !failed_ && error_counter_.GetCount() > 0) {
    Fail("An error was logged");
  }
}

void StepPilot::FailStep(std::string_view reason) {
  Fail(absl::StrCat(steps_.front(), " failed: ", reason));
}

void StepPilot::Fail(std::string_view reason) {
  failed_ = true;
  LOG(ERROR) << "Pilot frame " << GetFrame() << ": " << reason;
}

}  // namespace gb
