// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PILOT_SCRIPT_PILOT_SCRIPT_H_
#define GB_PILOT_SCRIPT_PILOT_SCRIPT_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "gb/pilot/step_pilot.h"

namespace gb {

// The steps read from a pilot script, to run with a StepPilot.
struct PilotScript {
  // True if the script starts with `hide window` (see StepPilot::Options).
  bool hide_window = false;

  std::vector<PilotStep> steps;
};

// Reads a pilot script.
//
// A script has one step per line, and `#` starts a comment. A name (of a key,
// command, condition, or saved frame) or a command's value is a C identifier,
// an integer, or a quoted string. Text and labels are always quoted. Strings
// take C escapes for '"', '\', newline (\n), tab (\t), and any byte (\xHH).
// The steps are:
//
//   hide window                         Only before every other step.
//   wait <frames>                       PilotWaitStep
//   wait for <condition> [<frames>]     PilotWaitForStep
//   key <key> [for <frames>]            PilotKeyStep
//   press key <key>                     PilotKeyPressStep
//   release key <key>                   PilotKeyReleaseStep
//   text "<text>"                       PilotTextStep
//   move <x> <y>                        PilotMouseMoveStep
//   click [left|middle|right] ["<label>" | <x> <y>] [for <frames>]
//                                       PilotMouseClickStep
//   press [left|middle|right] ["<label>" | <x> <y>]
//                                       PilotMousePressStep
//   release [left|middle|right]         PilotMouseReleaseStep
//   wheel <amount>                      PilotMouseWheelStep
//   run <command> <value>...            PilotRunStep
//   expect <condition>                  PilotExpectStep
//   expect error "<text>" [<frames>]    PilotExpectErrorStep
//   save <name>                         PilotSaveStep
//
// Frames can't be negative. Each step converts to the line that reads back as
// it (see step_pilot.h).
//
// Returns an error giving the line and column of the first problem, after
// `filename` if there is one. Keys, commands, and conditions are not checked
// here, as PilotSdl checks the keys, and the program may add commands and
// conditions as it runs.
absl::StatusOr<PilotScript> ReadPilotScript(std::string text,
                                            std::string_view filename = {});

}  // namespace gb

#endif  // GB_PILOT_SCRIPT_PILOT_SCRIPT_H_
