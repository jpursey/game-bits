// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PILOT_SDL_PILOT_SDL_H_
#define GB_PILOT_SDL_PILOT_SDL_H_

#include <array>
#include <memory>
#include <vector>

#include "SDL.h"
#include "gb/pilot/pilot.h"

namespace gb {

// Gives an SDL program the pilot's input, and keeps the user's real keyboard
// and mouse from reaching it.
//
// This does the input parts of a run for an SDL program (see Pilot):
// - Create() checks the pilot's key names, and hides the window if the pilot
//   asks.
// - As each frame begins, it adds the frame's input to SDL's event queue, as
//   key, text, mouse motion, mouse button, and mouse wheel events. It also sets
//   the keyboard and modifier state that SDL_GetKeyboardState() and
//   SDL_GetModState() return to the keys the pilot holds, with caps lock and
//   num lock off, so a run doesn't depend on the user's.
// - While it exists, SDL's event filter drops the user's real keyboard, mouse,
//   touch, and text input, and the window's keyboard and mouse focus changes,
//   so the user can't change a run. Everything else still passes (such as
//   closing the window, which still ends the program).
//
// SDL's own mouse state (SDL_GetMouseState()) is still the user's mouse, so a
// program reads the mouse from events, or from Pilot::GetMouse().
//
// SDL can still change the modifier state without a key event (such as caps
// lock as the window gains focus), which lasts until the next frame begins.
//
// An event filter set before this is created still sees the events that pass,
// and is set again when this is destroyed. Any event filter set while this
// exists must be removed before it is destroyed. The events already queued when
// it is created are kept, other than the user's input, and so are those queued
// when it is destroyed. Destroying it releases the keys the pilot holds.
//
// SDL's events subsystem must be initialized while this exists.
//
// This class is thread-compatible. It must be used on the thread that pumps
// SDL's events, which runs the loop.
class PilotSdl final {
 public:
  // Creates the pilot's SDL input. Returns null, logging an error for each,
  // if any of the pilot's key names isn't a key that SDL knows.
  //
  // `pilot` must outlive this, and so must `window`, unless it is null. With no
  // window, events have a window ID of 0, and nothing is hidden.
  static std::unique_ptr<PilotSdl> Create(Pilot* pilot, SDL_Window* window);

  PilotSdl(const PilotSdl&) = delete;
  PilotSdl& operator=(const PilotSdl&) = delete;
  ~PilotSdl();

 private:
  PilotSdl(Pilot* pilot, Uint32 window_id);

  // SDL's event filter, which returns whether to keep the event.
  static int SDLCALL FilterEvent(void* userdata, SDL_Event* event);
  bool KeepEvent(SDL_Event* event);

  // Adds the frame's input to SDL's event queue, and sets SDL's keyboard state.
  void AddInput();

  // Each appends the SDL events for one input event.
  void AppendEvents(const PilotKeyEvent& input, std::vector<SDL_Event>* events);
  void AppendEvents(const PilotTextEvent& input,
                    std::vector<SDL_Event>* events);
  void AppendEvents(const PilotMouseMoveEvent& input,
                    std::vector<SDL_Event>* events);
  void AppendEvents(const PilotMouseButtonEvent& input,
                    std::vector<SDL_Event>* events);
  void AppendEvents(const PilotMouseWheelEvent& input,
                    std::vector<SDL_Event>* events);

  // Returns the modifier state of the keys the pilot holds.
  SDL_Keymod GetModState() const;

  // Sets SDL's keyboard and modifier state to the pilot's.
  void RestoreKeys();

  Pilot* const pilot_;
  const Uint32 window_id_;
  SDL_EventFilter previous_filter_ = nullptr;
  void* previous_userdata_ = nullptr;
  PilotHandle frame_listener_;

  // Whether the pilot holds each key, by scancode.
  std::array<Uint8, SDL_NUM_SCANCODES> keys_ = {};

  // The mouse as last given to the program.
  PilotPoint mouse_position_;
  Uint32 mouse_buttons_ = 0;
};

}  // namespace gb

#endif  // GB_PILOT_SDL_PILOT_SDL_H_
