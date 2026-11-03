// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/pilot/sdl/pilot_sdl.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "gb/pilot/fake_pilot.h"
#include "gb/pilot/pilot.h"
#include "gb/test/log_error_guard.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;

auto LogTextHas(std::string_view text) {
  return Field(&LogRecorder::Message::text, HasSubstr(std::string(text)));
}

// Counts the events it sees, and keeps those `keep` says to.
struct CountingFilter {
  static int SDLCALL Filter(void* userdata, SDL_Event* event) {
    auto* self = static_cast<CountingFilter*>(userdata);
    ++self->count;
    return self->keep ? 1 : 0;
  }

  int count = 0;
  bool keep = true;
};

SDL_Event MakeEvent(Uint32 type) {
  SDL_Event event = {};
  event.type = type;
  return event;
}

SDL_Event MakeWindowEvent(Uint8 window_event) {
  SDL_Event event = MakeEvent(SDL_WINDOWEVENT);
  event.window.event = window_event;
  return event;
}

class PilotSdlTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_EQ(SDL_InitSubSystem(SDL_INIT_EVENTS), 0) << SDL_GetError();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
  }

  void TearDown() override {
    SDL_SetEventFilter(nullptr, nullptr);
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
  }

  // Runs a frame with the input, returning the events queued during it.
  std::vector<SDL_Event> RunFrame(std::vector<PilotInput> input) {
    pilot_.SetNextFrameInput(std::move(input));
    pilot_.BeginFrame();
    std::vector<SDL_Event> events = TakeEvents();
    pilot_.EndFrame();
    return events;
  }

  // Returns the queued events, removing them.
  static std::vector<SDL_Event> TakeEvents() {
    std::vector<SDL_Event> events;
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT,
                          SDL_LASTEVENT) == 1) {
      events.push_back(event);
    }
    return events;
  }

  // Sets SDL's state for a key, as SDL does before filtering a real key event.
  static void SetSdlKey(SDL_Scancode scancode, bool down) {
    const_cast<Uint8*>(SDL_GetKeyboardState(nullptr))[scancode] = down ? 1 : 0;
  }

  static bool IsSdlKeyDown(SDL_Scancode scancode) {
    return SDL_GetKeyboardState(nullptr)[scancode] != 0;
  }

  LogErrorGuard log_error_guard_;
  FakePilot pilot_;
};

//------------------------------------------------------------------------------
// Create
//------------------------------------------------------------------------------

TEST_F(PilotSdlTest, CreatesWithKnownKeyNames) {
  pilot_.SetKeyNames({"A", "Left Shift", "return", "F12"});
  EXPECT_NE(PilotSdl::Create(&pilot_, nullptr), nullptr);
}

TEST_F(PilotSdlTest, UnknownKeyNameFailsCreate) {
  pilot_.SetKeyNames({"A", "Nope", "", "Return"});
  EXPECT_EQ(PilotSdl::Create(&pilot_, nullptr), nullptr);
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("\"Nope\""), LogTextHas("\"\"")));

  // Nothing is filtered.
  SDL_Event event = MakeEvent(SDL_KEYDOWN);
  EXPECT_EQ(SDL_PushEvent(&event), 1);
}

TEST_F(PilotSdlTest, NoInputIsAddedOnceDestroyed) {
  PilotSdl::Create(&pilot_, nullptr).reset();
  EXPECT_THAT(RunFrame({PilotTextEvent{.text = "a"}}), IsEmpty());
}

//------------------------------------------------------------------------------
// Keys
//------------------------------------------------------------------------------

TEST_F(PilotSdlTest, KeyEventsArriveInTheirFrame) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  std::vector<SDL_Event> events =
      RunFrame({PilotKeyEvent{.key = "A", .down = true}});
  ASSERT_EQ(events.size(), 1);
  EXPECT_EQ(events[0].type, SDL_KEYDOWN);
  EXPECT_EQ(events[0].key.windowID, 0);
  EXPECT_EQ(events[0].key.state, SDL_PRESSED);
  EXPECT_EQ(events[0].key.repeat, 0);
  EXPECT_EQ(events[0].key.keysym.scancode, SDL_SCANCODE_A);
  EXPECT_EQ(events[0].key.keysym.sym, SDL_GetKeyFromScancode(SDL_SCANCODE_A));
  EXPECT_EQ(events[0].key.keysym.mod, KMOD_NONE);
  EXPECT_TRUE(IsSdlKeyDown(SDL_SCANCODE_A));

  EXPECT_THAT(RunFrame({}), IsEmpty());
  EXPECT_TRUE(IsSdlKeyDown(SDL_SCANCODE_A));

  events = RunFrame({PilotKeyEvent{.key = "a", .down = true},
                     PilotKeyEvent{.key = "A", .down = false}});
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(events[0].type, SDL_KEYDOWN);
  EXPECT_EQ(events[0].key.repeat, 1);
  EXPECT_EQ(events[1].type, SDL_KEYUP);
  EXPECT_EQ(events[1].key.state, SDL_RELEASED);
  EXPECT_EQ(events[1].key.repeat, 0);
  EXPECT_EQ(events[1].key.keysym.scancode, SDL_SCANCODE_A);
  EXPECT_FALSE(IsSdlKeyDown(SDL_SCANCODE_A));
}

TEST_F(PilotSdlTest, ModifierKeysSetModState) {
  SDL_SetModState(static_cast<SDL_Keymod>(KMOD_CAPS | KMOD_NUM | KMOD_RALT));
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  std::vector<SDL_Event> events =
      RunFrame({PilotKeyEvent{.key = "Left Shift", .down = true},
                PilotKeyEvent{.key = "Right Ctrl", .down = true},
                PilotKeyEvent{.key = "B", .down = true}});
  ASSERT_EQ(events.size(), 3);
  EXPECT_EQ(events[0].key.keysym.mod, KMOD_LSHIFT);
  EXPECT_EQ(events[1].key.keysym.mod, KMOD_LSHIFT | KMOD_RCTRL);
  EXPECT_EQ(events[2].key.keysym.mod, KMOD_LSHIFT | KMOD_RCTRL);
  EXPECT_EQ(SDL_GetModState(), KMOD_LSHIFT | KMOD_RCTRL);
  EXPECT_TRUE(IsSdlKeyDown(SDL_SCANCODE_LSHIFT));
  EXPECT_TRUE(IsSdlKeyDown(SDL_SCANCODE_RCTRL));
  EXPECT_TRUE(IsSdlKeyDown(SDL_SCANCODE_B));

  events = RunFrame({PilotKeyEvent{.key = "Left Shift", .down = false}});
  ASSERT_EQ(events.size(), 1);
  EXPECT_EQ(events[0].key.keysym.mod, KMOD_RCTRL);
  EXPECT_EQ(SDL_GetModState(), KMOD_RCTRL);
  EXPECT_FALSE(IsSdlKeyDown(SDL_SCANCODE_LSHIFT));
}

TEST_F(PilotSdlTest, KeyStateIsThePilotsFromTheFirstFrame) {
  SetSdlKey(SDL_SCANCODE_Z, true);
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  RunFrame({});
  EXPECT_FALSE(IsSdlKeyDown(SDL_SCANCODE_Z));
}

TEST_F(PilotSdlTest, HeldKeysAreReleasedWhenDestroyed) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  RunFrame({PilotKeyEvent{.key = "Left Shift", .down = true},
            PilotKeyEvent{.key = "D", .down = true}});
  pilot_sdl.reset();
  EXPECT_FALSE(IsSdlKeyDown(SDL_SCANCODE_LSHIFT));
  EXPECT_FALSE(IsSdlKeyDown(SDL_SCANCODE_D));
  EXPECT_EQ(SDL_GetModState(), KMOD_NONE);
}

TEST_F(PilotSdlTest, UnknownKeyLogsErrorAndIsSkipped) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  std::vector<SDL_Event> events =
      RunFrame({PilotKeyEvent{.key = "Nope", .down = true},
                PilotKeyEvent{.key = "C", .down = true}});
  ASSERT_EQ(events.size(), 1);
  EXPECT_EQ(events[0].key.keysym.scancode, SDL_SCANCODE_C);
  EXPECT_THAT(log_error_guard_.TakeMessages(),
              ElementsAre(LogTextHas("\"Nope\"")));
}

//------------------------------------------------------------------------------
// Text
//------------------------------------------------------------------------------

TEST_F(PilotSdlTest, TextEventsArriveInTheirFrame) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  std::vector<SDL_Event> events = RunFrame(
      {PilotTextEvent{.text = "hello"}, PilotTextEvent{.text = "world"}});
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(events[0].type, SDL_TEXTINPUT);
  EXPECT_STREQ(events[0].text.text, "hello");
  EXPECT_STREQ(events[1].text.text, "world");
  EXPECT_THAT(RunFrame({}), IsEmpty());
}

TEST_F(PilotSdlTest, LongTextIsSplitBetweenCharacters) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  const std::string ascii(40, 'a');
  std::vector<SDL_Event> events = RunFrame({PilotTextEvent{.text = ascii}});
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(std::string(events[0].text.text), std::string(31, 'a'));
  EXPECT_EQ(std::string(events[1].text.text), std::string(9, 'a'));

  // "\xC3\xA9" is a two byte character, which would end at byte 32.
  const std::string utf8 = std::string(30, 'b') + "\xC3\xA9";
  events = RunFrame({PilotTextEvent{.text = utf8}});
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(std::string(events[0].text.text), std::string(30, 'b'));
  EXPECT_EQ(std::string(events[1].text.text), "\xC3\xA9");

  // Text that isn't UTF-8 is still split.
  const std::string continuations(40, '\x80');
  events = RunFrame({PilotTextEvent{.text = continuations}});
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(std::string(events[0].text.text), std::string(31, '\x80'));
  EXPECT_EQ(std::string(events[1].text.text), std::string(9, '\x80'));
}

//------------------------------------------------------------------------------
// Mouse
//------------------------------------------------------------------------------

TEST_F(PilotSdlTest, MouseEventsArriveInTheirFrame) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  std::vector<SDL_Event> events =
      RunFrame({PilotMouseMoveEvent{.position = {10, 20}},
                PilotMouseButtonEvent{.button = PilotMouseButton::kLeft,
                                      .down = true,
                                      .position = {10, 20}},
                PilotMouseMoveEvent{.position = {15, 18}}});
  ASSERT_EQ(events.size(), 3);
  EXPECT_EQ(events[0].type, SDL_MOUSEMOTION);
  EXPECT_EQ(events[0].motion.state, 0);
  EXPECT_EQ(events[0].motion.x, 10);
  EXPECT_EQ(events[0].motion.y, 20);
  EXPECT_EQ(events[0].motion.xrel, 10);
  EXPECT_EQ(events[0].motion.yrel, 20);
  EXPECT_EQ(events[1].type, SDL_MOUSEBUTTONDOWN);
  EXPECT_EQ(events[1].button.button, SDL_BUTTON_LEFT);
  EXPECT_EQ(events[1].button.state, SDL_PRESSED);
  EXPECT_EQ(events[1].button.clicks, 1);
  EXPECT_EQ(events[1].button.x, 10);
  EXPECT_EQ(events[1].button.y, 20);
  EXPECT_EQ(events[2].type, SDL_MOUSEMOTION);
  EXPECT_EQ(events[2].motion.state, SDL_BUTTON_LMASK);
  EXPECT_EQ(events[2].motion.xrel, 5);
  EXPECT_EQ(events[2].motion.yrel, -2);

  events = RunFrame({PilotMouseButtonEvent{.button = PilotMouseButton::kLeft,
                                           .down = false,
                                           .position = {15, 18}},
                     PilotMouseMoveEvent{.position = {16, 18}}});
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(events[0].type, SDL_MOUSEBUTTONUP);
  EXPECT_EQ(events[0].button.state, SDL_RELEASED);
  EXPECT_EQ(events[0].button.x, 15);
  EXPECT_EQ(events[0].button.y, 18);
  EXPECT_EQ(events[1].motion.state, 0);
  EXPECT_EQ(events[1].motion.xrel, 1);
}

TEST_F(PilotSdlTest, MouseButtonsAreSdlButtons) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  std::vector<SDL_Event> events = RunFrame(
      {PilotMouseButtonEvent{.button = PilotMouseButton::kMiddle, .down = true},
       PilotMouseButtonEvent{.button = PilotMouseButton::kRight, .down = true},
       PilotMouseMoveEvent{.position = {1, 1}}});
  ASSERT_EQ(events.size(), 3);
  EXPECT_EQ(events[0].button.button, SDL_BUTTON_MIDDLE);
  EXPECT_EQ(events[1].button.button, SDL_BUTTON_RIGHT);
  EXPECT_EQ(events[2].motion.state, SDL_BUTTON_MMASK | SDL_BUTTON_RMASK);
}

TEST_F(PilotSdlTest, MouseWheelEventsArriveInTheirFrame) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  std::vector<SDL_Event> events = RunFrame(
      {PilotMouseWheelEvent{.amount = 2}, PilotMouseWheelEvent{.amount = -1}});
  ASSERT_EQ(events.size(), 2);
  EXPECT_EQ(events[0].type, SDL_MOUSEWHEEL);
  EXPECT_EQ(events[0].wheel.y, 2);
  EXPECT_EQ(events[0].wheel.direction, SDL_MOUSEWHEEL_NORMAL);
  EXPECT_EQ(events[1].wheel.y, -1);
}

TEST_F(PilotSdlTest, InputArrivesInOrder) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  std::vector<SDL_Event> events =
      RunFrame({PilotMouseWheelEvent{.amount = 1}, PilotTextEvent{.text = "a"},
                PilotKeyEvent{.key = "A", .down = true},
                PilotMouseMoveEvent{.position = {1, 2}}});
  ASSERT_EQ(events.size(), 4);
  EXPECT_EQ(events[0].type, SDL_MOUSEWHEEL);
  EXPECT_EQ(events[1].type, SDL_TEXTINPUT);
  EXPECT_EQ(events[2].type, SDL_KEYDOWN);
  EXPECT_EQ(events[3].type, SDL_MOUSEMOTION);
}

//------------------------------------------------------------------------------
// Event filter
//------------------------------------------------------------------------------

TEST_F(PilotSdlTest, RealInputIsDropped) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  const Uint32 kTypes[] = {
      SDL_KEYDOWN,       SDL_KEYUP,        SDL_TEXTEDITING,
      SDL_TEXTINPUT,     SDL_MOUSEMOTION,  SDL_MOUSEBUTTONDOWN,
      SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL,   SDL_FINGERDOWN,
      SDL_FINGERUP,      SDL_FINGERMOTION, SDL_DOLLARGESTURE,
      SDL_DOLLARRECORD,  SDL_MULTIGESTURE,
  };
  for (Uint32 type : kTypes) {
    SDL_Event event = MakeEvent(type);
    EXPECT_EQ(SDL_PushEvent(&event), 0) << "Type " << type;
  }
  const Uint8 kWindowEvents[] = {
      SDL_WINDOWEVENT_ENTER,
      SDL_WINDOWEVENT_LEAVE,
      SDL_WINDOWEVENT_FOCUS_GAINED,
      SDL_WINDOWEVENT_FOCUS_LOST,
  };
  for (Uint8 window_event : kWindowEvents) {
    SDL_Event event = MakeWindowEvent(window_event);
    EXPECT_EQ(SDL_PushEvent(&event), 0) << "Window event " << +window_event;
  }
  EXPECT_THAT(TakeEvents(), IsEmpty());
}

TEST_F(PilotSdlTest, QuitAndOtherEventsPass) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  SDL_Event quit = MakeEvent(SDL_QUIT);
  EXPECT_EQ(SDL_PushEvent(&quit), 1);
  SDL_Event close = MakeWindowEvent(SDL_WINDOWEVENT_CLOSE);
  EXPECT_EQ(SDL_PushEvent(&close), 1);
  SDL_Event resized = MakeWindowEvent(SDL_WINDOWEVENT_RESIZED);
  EXPECT_EQ(SDL_PushEvent(&resized), 1);
  SDL_Event user = MakeEvent(SDL_USEREVENT);
  EXPECT_EQ(SDL_PushEvent(&user), 1);

  std::vector<SDL_Event> events = TakeEvents();
  ASSERT_EQ(events.size(), 4);
  EXPECT_EQ(events[0].type, SDL_QUIT);
  EXPECT_EQ(events[1].window.event, SDL_WINDOWEVENT_CLOSE);
  EXPECT_EQ(events[2].window.event, SDL_WINDOWEVENT_RESIZED);
  EXPECT_EQ(events[3].type, SDL_USEREVENT);
}

TEST_F(PilotSdlTest, RealKeyDoesNotChangeKeyState) {
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  RunFrame({PilotKeyEvent{.key = "Left Shift", .down = true}});

  // SDL sets its state for a real key before it filters the key's event.
  SetSdlKey(SDL_SCANCODE_LSHIFT, false);
  SetSdlKey(SDL_SCANCODE_X, true);
  SDL_SetModState(KMOD_LALT);
  SDL_Event event = MakeEvent(SDL_KEYDOWN);
  event.key.keysym.scancode = SDL_SCANCODE_X;
  EXPECT_EQ(SDL_PushEvent(&event), 0);

  EXPECT_TRUE(IsSdlKeyDown(SDL_SCANCODE_LSHIFT));
  EXPECT_FALSE(IsSdlKeyDown(SDL_SCANCODE_X));
  EXPECT_EQ(SDL_GetModState(), KMOD_LSHIFT);
}

TEST_F(PilotSdlTest, PreviousFilterSeesEventsThatPassAndIsSetAgain) {
  CountingFilter previous;
  SDL_SetEventFilter(&CountingFilter::Filter, &previous);
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);

  SDL_Event key = MakeEvent(SDL_KEYDOWN);
  EXPECT_EQ(SDL_PushEvent(&key), 0);
  EXPECT_EQ(previous.count, 0);
  SDL_Event user = MakeEvent(SDL_USEREVENT);
  EXPECT_EQ(SDL_PushEvent(&user), 1);
  EXPECT_EQ(previous.count, 1);
  previous.keep = false;
  EXPECT_EQ(SDL_PushEvent(&user), 0);
  EXPECT_EQ(previous.count, 2);
  EXPECT_THAT(TakeEvents(),
              ElementsAre(Field(&SDL_Event::type, Uint32{SDL_USEREVENT})));

  pilot_sdl.reset();
  SDL_EventFilter filter = nullptr;
  void* userdata = nullptr;
  ASSERT_TRUE(SDL_GetEventFilter(&filter, &userdata));
  EXPECT_EQ(filter, &CountingFilter::Filter);
  EXPECT_EQ(userdata, &previous);
}

TEST_F(PilotSdlTest, NoFilterIsSetAgainWhenThereWasNone) {
  PilotSdl::Create(&pilot_, nullptr).reset();
  EXPECT_FALSE(SDL_GetEventFilter(nullptr, nullptr));
  SDL_Event key = MakeEvent(SDL_KEYDOWN);
  EXPECT_EQ(SDL_PushEvent(&key), 1);
}

TEST_F(PilotSdlTest, QueuedEventsAreKeptWhenCreatedAndDestroyed) {
  SDL_Event before = MakeEvent(SDL_USEREVENT);
  ASSERT_EQ(SDL_PushEvent(&before), 1);
  SDL_Event key = MakeEvent(SDL_KEYDOWN);
  ASSERT_EQ(SDL_PushEvent(&key), 1);
  SDL_Event focus = MakeWindowEvent(SDL_WINDOWEVENT_FOCUS_LOST);
  ASSERT_EQ(SDL_PushEvent(&focus), 1);

  // The user's input already queued is dropped.
  std::unique_ptr<PilotSdl> pilot_sdl = PilotSdl::Create(&pilot_, nullptr);
  EXPECT_THAT(TakeEvents(),
              ElementsAre(Field(&SDL_Event::type, Uint32{SDL_USEREVENT})));

  SDL_Event during = MakeEvent(SDL_USEREVENT + 1);
  ASSERT_EQ(SDL_PushEvent(&during), 1);
  pilot_sdl.reset();
  EXPECT_THAT(TakeEvents(),
              ElementsAre(Field(&SDL_Event::type, Uint32{SDL_USEREVENT + 1})));
}

}  // namespace
}  // namespace gb
