// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/sdl/pilot_sdl.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"

namespace gb {

namespace {

// The modifier each modifier key sets while held.
struct ModKey {
  SDL_Scancode scancode;
  SDL_Keymod mod;
};
constexpr ModKey kModKeys[] = {
    {SDL_SCANCODE_LSHIFT, KMOD_LSHIFT}, {SDL_SCANCODE_RSHIFT, KMOD_RSHIFT},
    {SDL_SCANCODE_LCTRL, KMOD_LCTRL},   {SDL_SCANCODE_RCTRL, KMOD_RCTRL},
    {SDL_SCANCODE_LALT, KMOD_LALT},     {SDL_SCANCODE_RALT, KMOD_RALT},
    {SDL_SCANCODE_LGUI, KMOD_LGUI},     {SDL_SCANCODE_RGUI, KMOD_RGUI},
    {SDL_SCANCODE_MODE, KMOD_MODE},
};

// Returns the key's scancode, or logs an error and returns SDL_SCANCODE_UNKNOWN
// if SDL doesn't know it.
SDL_Scancode FindScancode(std::string_view key) {
  const SDL_Scancode scancode =
      SDL_GetScancodeFromName(std::string(key).c_str());
  if (scancode == SDL_SCANCODE_UNKNOWN) {
    LOG(ERROR) << "Pilot key \"" << key << "\" is not a key that SDL knows.";
  }
  return scancode;
}

Uint8 ToSdlButton(PilotMouseButton button) {
  switch (button) {
    case PilotMouseButton::kLeft:
      return SDL_BUTTON_LEFT;
    case PilotMouseButton::kMiddle:
      return SDL_BUTTON_MIDDLE;
    case PilotMouseButton::kRight:
      return SDL_BUTTON_RIGHT;
  }
  return SDL_BUTTON_LEFT;
}

// Returns whether the event is the user's keyboard, mouse, touch, or text
// input, or a change of the window's keyboard or mouse focus.
bool IsUserInput(const SDL_Event& event) {
  switch (event.type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    case SDL_TEXTEDITING:
    case SDL_TEXTINPUT:
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
    case SDL_FINGERDOWN:
    case SDL_FINGERUP:
    case SDL_FINGERMOTION:
    case SDL_DOLLARGESTURE:
    case SDL_DOLLARRECORD:
    case SDL_MULTIGESTURE:
      return true;
    case SDL_WINDOWEVENT:
      switch (event.window.event) {
        case SDL_WINDOWEVENT_ENTER:
        case SDL_WINDOWEVENT_LEAVE:
        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST:
          return true;
      }
      break;
  }
  return false;
}

// Sets SDL's event filter, keeping the events already queued (other than the
// user's input, if `drop_user_input`), which SDL discards when its filter is
// set.
void SetEventFilterKeepingEvents(SDL_EventFilter filter, void* userdata,
                                 bool drop_user_input) {
  std::vector<SDL_Event> events;
  SDL_Event event;
  while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT,
                        SDL_LASTEVENT) == 1) {
    if (!drop_user_input || !IsUserInput(event)) {
      events.push_back(event);
    }
  }
  SDL_SetEventFilter(filter, userdata);
  if (!events.empty()) {
    SDL_PeepEvents(events.data(), static_cast<int>(events.size()), SDL_ADDEVENT,
                   SDL_FIRSTEVENT, SDL_LASTEVENT);
  }
}

}  // namespace

std::unique_ptr<PilotSdl> PilotSdl::Create(Pilot* pilot, SDL_Window* window) {
  bool keys_known = true;
  for (const std::string& key : pilot->GetKeyNames()) {
    if (FindScancode(key) == SDL_SCANCODE_UNKNOWN) {
      keys_known = false;
    }
  }
  if (!keys_known) {
    return nullptr;
  }

  Uint32 window_id = 0;
  if (window != nullptr) {
    window_id = SDL_GetWindowID(window);
    if (pilot->IsWindowHidden()) {
      SDL_HideWindow(window);
    }
  }
  return absl::WrapUnique(new PilotSdl(pilot, window_id));
}

PilotSdl::PilotSdl(Pilot* pilot, Uint32 window_id)
    : pilot_(pilot), window_id_(window_id) {
  SDL_GetEventFilter(&previous_filter_, &previous_userdata_);
  SetEventFilterKeepingEvents(&FilterEvent, this, /*drop_user_input=*/true);
  frame_listener_ = pilot_->AddFrameListener([this] { AddInput(); });
}

PilotSdl::~PilotSdl() {
  SDL_EventFilter filter = nullptr;
  void* userdata = nullptr;
  SDL_GetEventFilter(&filter, &userdata);
  DCHECK(filter == &FilterEvent && userdata == this)
      << "SDL's event filter was changed while a PilotSdl exists.";
  SetEventFilterKeepingEvents(previous_filter_, previous_userdata_,
                              /*drop_user_input=*/false);

  // Keys the pilot still holds are released, so they don't stay held.
  keys_.fill(0);
  RestoreKeys();
}

int SDLCALL PilotSdl::FilterEvent(void* userdata, SDL_Event* event) {
  return static_cast<PilotSdl*>(userdata)->KeepEvent(event) ? 1 : 0;
}

bool PilotSdl::KeepEvent(SDL_Event* event) {
  if (IsUserInput(*event)) {
    if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
      // SDL changes its keyboard state before it filters the event.
      RestoreKeys();
    }
    return false;
  }
  return previous_filter_ == nullptr ||
         previous_filter_(previous_userdata_, event) != 0;
}

void PilotSdl::AddInput() {
  std::vector<SDL_Event> events;
  for (const PilotInput& input : pilot_->GetInput()) {
    std::visit(
        [this, &events](const auto& alternative) {
          AppendEvents(alternative, &events);
        },
        input);
  }
  RestoreKeys();
  if (events.empty()) {
    return;
  }

  const Uint32 timestamp = SDL_GetTicks();
  for (SDL_Event& event : events) {
    event.common.timestamp = timestamp;
  }

  // Added events skip the event filter.
  const int count = static_cast<int>(events.size());
  const int added = SDL_PeepEvents(events.data(), count, SDL_ADDEVENT,
                                   SDL_FIRSTEVENT, SDL_LASTEVENT);
  if (added != count) {
    LOG(ERROR) << "SDL's event queue took " << std::max(added, 0) << " of "
               << count << " pilot input events: " << SDL_GetError();
  }
}

void PilotSdl::AppendEvents(const PilotKeyEvent& input,
                            std::vector<SDL_Event>* events) {
  const SDL_Scancode scancode = FindScancode(input.key);
  if (scancode == SDL_SCANCODE_UNKNOWN) {
    return;
  }
  const bool repeat = input.down && keys_[scancode] != 0;
  keys_[scancode] = input.down ? 1 : 0;

  SDL_Event& event = events->emplace_back();
  event.type = input.down ? SDL_KEYDOWN : SDL_KEYUP;
  event.key.windowID = window_id_;
  event.key.state = input.down ? SDL_PRESSED : SDL_RELEASED;
  event.key.repeat = repeat ? 1 : 0;
  event.key.keysym.scancode = scancode;
  event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
  event.key.keysym.mod = static_cast<Uint16>(GetModState());
}

void PilotSdl::AppendEvents(const PilotTextEvent& input,
                            std::vector<SDL_Event>* events) {
  // Each text event holds a few bytes, so longer text is split between
  // characters. UTF-8 continuation bytes are 10xxxxxx.
  constexpr int kMaxSize = SDL_TEXTINPUTEVENT_TEXT_SIZE - 1;
  std::string_view text = input.text;
  while (!text.empty()) {
    const int text_size = static_cast<int>(text.size());
    int size = std::min(text_size, kMaxSize);
    if (size < text_size) {
      while (size > 0 &&
             (static_cast<unsigned char>(text[size]) & 0xC0) == 0x80) {
        --size;
      }
      if (size == 0) {
        size = kMaxSize;
      }
    }

    SDL_Event& event = events->emplace_back();
    event.type = SDL_TEXTINPUT;
    event.text.windowID = window_id_;
    text.copy(event.text.text, size);
    text.remove_prefix(size);
  }
}

void PilotSdl::AppendEvents(const PilotMouseMoveEvent& input,
                            std::vector<SDL_Event>* events) {
  SDL_Event& event = events->emplace_back();
  event.type = SDL_MOUSEMOTION;
  event.motion.windowID = window_id_;
  event.motion.state = mouse_buttons_;
  event.motion.x = input.position.x;
  event.motion.y = input.position.y;
  event.motion.xrel = input.position.x - mouse_position_.x;
  event.motion.yrel = input.position.y - mouse_position_.y;
  mouse_position_ = input.position;
}

void PilotSdl::AppendEvents(const PilotMouseButtonEvent& input,
                            std::vector<SDL_Event>* events) {
  const Uint8 button = ToSdlButton(input.button);
  if (input.down) {
    mouse_buttons_ |= SDL_BUTTON(button);
  } else {
    mouse_buttons_ &= ~SDL_BUTTON(button);
  }
  mouse_position_ = input.position;

  SDL_Event& event = events->emplace_back();
  event.type = input.down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  event.button.windowID = window_id_;
  event.button.button = button;
  event.button.state = input.down ? SDL_PRESSED : SDL_RELEASED;
  event.button.clicks = 1;
  event.button.x = input.position.x;
  event.button.y = input.position.y;
}

void PilotSdl::AppendEvents(const PilotMouseWheelEvent& input,
                            std::vector<SDL_Event>* events) {
  SDL_Event& event = events->emplace_back();
  event.type = SDL_MOUSEWHEEL;
  event.wheel.windowID = window_id_;
  event.wheel.y = input.amount;
  event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
}

SDL_Keymod PilotSdl::GetModState() const {
  int mod = KMOD_NONE;
  for (const ModKey& mod_key : kModKeys) {
    if (keys_[mod_key.scancode] != 0) {
      mod |= mod_key.mod;
    }
  }
  return static_cast<SDL_Keymod>(mod);
}

void PilotSdl::RestoreKeys() {
  // SDL only reads the array it returns, which is in writable memory.
  Uint8* state = const_cast<Uint8*>(SDL_GetKeyboardState(nullptr));
  std::copy(keys_.begin(), keys_.end(), state);
  SDL_SetModState(GetModState());
}

}  // namespace gb
