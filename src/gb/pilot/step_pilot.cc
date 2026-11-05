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
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"

namespace gb {
namespace {

// A save step fails if no frame is drawn within this many frames.
constexpr int kMaxSaveFrames = 600;

// Returns true if a name reads back as itself without quotes: a C identifier,
// or an integer as it is written in decimal.
bool IsBareName(std::string_view name) {
  if (name.empty()) {
    return false;
  }
  if (absl::ascii_isalpha(name[0]) || name[0] == '_') {
    return std::all_of(name.begin(), name.end(), [](char ch) {
      return absl::ascii_isalnum(ch) || ch == '_';
    });
  }
  int value = 0;
  return absl::SimpleAtoi(name, &value) && absl::StrCat(value) == name;
}

}  // namespace

//==============================================================================
// Step formatting
//==============================================================================

namespace internal {

std::string FormatPilotName(std::string_view name) {
  return IsBareName(name) ? std::string(name) : QuotePilotText(name);
}

std::string QuotePilotText(std::string_view text) {
  std::string quoted = "\"";
  for (const char ch : text) {
    if (ch == '"' || ch == '\\') {
      quoted.push_back('\\');
      quoted.push_back(ch);
    } else if (ch == '\n') {
      quoted += "\\n";
    } else if (ch == '\t') {
      quoted += "\\t";
    } else if (absl::ascii_iscntrl(ch)) {
      absl::StrAppendFormat(&quoted, "\\x%02x", static_cast<unsigned char>(ch));
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('"');
  return quoted;
}

}  // namespace internal

//==============================================================================
// StepPilot::ErrorLog
//==============================================================================

std::vector<std::string> StepPilot::ErrorLog::TakeErrors() {
  absl::MutexLock lock(&mutex_);
  return std::exchange(errors_, {});
}

void StepPilot::ErrorLog::Send(const absl::LogEntry& entry) {
  if (entry.log_severity() < absl::LogSeverity::kError) {
    return;
  }
  std::string error(entry.text_message());
  absl::MutexLock lock(&mutex_);
  errors_.push_back(std::move(error));
}

//==============================================================================
// StepPilot
//==============================================================================

StepPilot::StepPilot() : StepPilot(Options()) {}

StepPilot::StepPilot(Options options) : options_(std::move(options)) {
  absl::AddLogSink(&error_log_);
}

StepPilot::~StepPilot() {
  // Removing the sink waits for any error being kept on another thread.
  absl::RemoveLogSink(&error_log_);
}

void StepPilot::AddSteps(std::vector<PilotStep> steps) {
  if (done_) {
    LOG(ERROR) << "Pilot steps were added after the run was done, so they are "
                  "ignored.";
    return;
  }
  for (PilotStep& step : steps) {
    // What is pressed is held for at least one frame.
    if (auto* key_step = std::get_if<PilotKeyStep>(&step)) {
      key_step->frames = std::max(key_step->frames, 1);
      if (std::find(key_names_.begin(), key_names_.end(), key_step->key) ==
          key_names_.end()) {
        key_names_.push_back(key_step->key);
      }
    } else if (auto* click_step = std::get_if<PilotMouseClickStep>(&step)) {
      click_step->frames = std::max(click_step->frames, 1);
    }
    steps_.push_back(std::move(step));
  }
}

void StepPilot::DoBeginFrame() {
  input_.clear();
  saving_ = false;
  ValidateLoggedErrors();
  RunSteps();
}

void StepPilot::DoEndFrame() {
  if (done_) {
    return;
  }
  ValidateLoggedErrors();
  if (!failed_ && step_started_) {
    EndStepFrame();
  }
  if (!failed_) {
    EndExpectationFrames(/*run_ending=*/steps_.empty());
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
      // Errors logged before a step are judged without it, so an expect error
      // step only expects errors that follow it.
      ValidateLoggedErrors();
      if (failed_) {
        return;
      }
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
  } else if (const auto* key = std::get_if<PilotKeyStep>(&step)) {
    finished = step_frames_ > key->frames;
  } else if (const auto* click = std::get_if<PilotMouseClickStep>(&step)) {
    finished = step_frames_ > click->frames;
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
  // Once started, it finishes at the end of the frame it is released in (in
  // EndStepFrame()).
  if (step_frames_ == 0) {
    input_.push_back(PilotKeyEvent{.key = step.key, .down = true});
  } else if (step_frames_ == step.frames) {
    input_.push_back(PilotKeyEvent{.key = step.key, .down = false});
  }
  return false;
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
  // Once started, it finishes at the end of the frame it is released in (in
  // EndStepFrame()).
  if (step_frames_ > 0) {
    if (step_frames_ == step.frames) {
      SetMouseButton(step.button, false);
    }
    return false;
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

bool StepPilot::RunStep(const PilotExpectErrorStep& step) {
  // Empty text would expect every error.
  if (step.text.empty()) {
    FailStep("No text to expect");
    return false;
  }

  // It lasts at least the frame it starts in.
  expectations_.push_back(
      {.step = step,
       .last_frame = GetFrame() + std::max(step.max_frames, 1) - 1});
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

void StepPilot::ValidateLoggedErrors() {
  if (done_ || failed_) {
    return;
  }
  for (const std::string& error : error_log_.TakeErrors()) {
    const ErrorExpectation* expected_by = nullptr;
    for (ErrorExpectation& expectation : expectations_) {
      if (absl::StrContains(error, expectation.step.text)) {
        expectation.met = true;
        expected_by = &expectation;
      }
    }
    if (expected_by == nullptr) {
      Fail(absl::StrCat("An error was logged: ", error));
      return;
    }
    LOG(INFO) << "Pilot frame " << GetFrame() << ": The error was expected, by "
              << expected_by->step;
  }
}

void StepPilot::EndExpectationFrames(bool run_ending) {
  const int frame = GetFrame();
  for (const ErrorExpectation& expectation : expectations_) {
    if (expectation.met) {
      continue;
    }
    const bool ended = frame >= expectation.last_frame;
    if (!ended && !run_ending) {
      continue;
    }
    const std::string when =
        ended ? absl::StrCat("in ", expectation.step.max_frames, " frames")
              : "before the run ended";
    Fail(absl::StrCat(expectation.step,
                      " failed: No error with its text was logged ", when));
    return;
  }
  std::erase_if(expectations_, [frame](const ErrorExpectation& expectation) {
    return frame >= expectation.last_frame;
  });
}

void StepPilot::FailStep(std::string_view reason) {
  Fail(absl::StrCat(steps_.front(), " failed: ", reason));
}

void StepPilot::Fail(std::string_view reason) {
  failed_ = true;
  LOG(ERROR) << "Pilot frame " << GetFrame() << ": " << reason;
}

}  // namespace gb
