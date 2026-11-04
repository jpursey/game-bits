// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_PILOT_PILOT_H_
#define GB_PILOT_PILOT_H_

#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "gb/image/image.h"

namespace gb {

class Pilot;

//==============================================================================
// Input
//==============================================================================

// A point in the program's window, in window pixels from its top left.
struct PilotPoint {
  bool operator==(const PilotPoint&) const = default;

  int x = 0;
  int y = 0;
};

enum class PilotMouseButton {
  kLeft,
  kMiddle,
  kRight,
};

// A key is pressed or released.
struct PilotKeyEvent {
  bool operator==(const PilotKeyEvent&) const = default;

  // The key, by the name SDL gives its scancode (see
  // SDL_GetScancodeFromName()), as written in the script.
  std::string key;
  bool down = false;
};

// Text is typed.
struct PilotTextEvent {
  bool operator==(const PilotTextEvent&) const = default;

  // UTF-8 text.
  std::string text;
};

// The mouse moves.
struct PilotMouseMoveEvent {
  bool operator==(const PilotMouseMoveEvent&) const = default;

  PilotPoint position;
};

// A mouse button is pressed or released.
struct PilotMouseButtonEvent {
  bool operator==(const PilotMouseButtonEvent&) const = default;

  PilotMouseButton button = PilotMouseButton::kLeft;
  bool down = false;

  // Where the mouse is.
  PilotPoint position;
};

// The mouse wheel turns.
struct PilotMouseWheelEvent {
  bool operator==(const PilotMouseWheelEvent&) const = default;

  // Steps of the wheel, positive away from the user.
  int amount = 0;
};

// One input event of a frame.
using PilotInput =
    std::variant<PilotKeyEvent, PilotTextEvent, PilotMouseMoveEvent,
                 PilotMouseButtonEvent, PilotMouseWheelEvent>;

// The mouse, after a frame's input.
struct PilotMouse {
  bool operator==(const PilotMouse&) const = default;

  PilotPoint position;
  bool left = false;
  bool middle = false;
  bool right = false;
};

//==============================================================================
// PilotHandle
//==============================================================================

// Keeps a command, condition, or item finder registered with a pilot, and
// removes it when destroyed. A handle that is default constructed or moved from
// holds nothing.
//
// A handle must be destroyed before the pilot it came from.
//
// This class is thread-compatible.
class PilotHandle final {
 public:
  PilotHandle() = default;
  PilotHandle(const PilotHandle&) = delete;
  PilotHandle(PilotHandle&& other);
  PilotHandle& operator=(const PilotHandle&) = delete;
  PilotHandle& operator=(PilotHandle&& other);
  ~PilotHandle();

 private:
  friend class Pilot;

  PilotHandle(Pilot* pilot, int id) : pilot_(pilot), id_(id) {}

  // Removes what the handle holds, if anything, so it holds nothing.
  void Remove();

  Pilot* pilot_ = nullptr;
  int id_ = 0;
};

//==============================================================================
// Pilot
//==============================================================================

// Drives a program frame by frame, giving it input, and saving the frames it
// draws.
//
// A program creates one (such as a StepPilot, which runs a list of steps) and
// puts it in its context, where the parts of the program that take part in a
// run find it. It must be stored as a Pilot, not as its subclass (such as with
// SetOwned<Pilot>()), or it isn't found.
//
// Nothing here reaches into the program: it is piloted only as far as its code
// does what the pilot asks, in the three phases below. Game Bits libraries that
// take part do some of this for the programs that use them, as each one says,
// and a program does the rest itself.
//
// Before the first frame:
// - Hide the window if IsWindowHidden(), while still drawing every frame at the
//   window's size.
// - Check the names in GetKeyNames(), logging an error for any that can't be
//   mapped to a key.
// - Register the program's commands and conditions with AddCommand() and
//   AddCondition(), and an item finder for each GUI with AddItemFinder(). Each
//   is kept while it means something: a game state's while the state is
//   active, and a GUI's while the GUI exists. They may also be added and
//   removed later, at any time, but a pilot only finds what is registered when
//   it looks.
// - Register a frame listener with AddFrameListener() for code that acts as
//   each frame begins, from outside the program's frame (such as applying input
//   by adding events to a queue the program reads).
//
// Each frame, in this order:
// 1. Stop if IsDone(), ending the run.
// 2. Call BeginFrame(), which calls the frame listeners. The rest of the
// frame's
//    functions are only called between this and EndFrame().
// 3. Run the frame:
//    - Apply GetInput() in order, as if the user had given it, and ignore the
//      user's real keyboard and mouse. Code that polls the mouse rather than
//      reading events reads GetMouse(). Log an error for a key name that
//      wasn't checked before the first frame and can't be mapped, as a pilot
//      may learn of more keys as it runs.
//    - Advance game time by GetDeltaTime(), rather than by the time measured,
//      and don't wait on real time (vsync only slows a run down). A GUI that
//      keeps its own frame time and mouse takes them from GetDeltaTime() and
//      GetMouse().
//    - If IsSavingFrame(), read back the frame once drawn, as presented and
//      one pixel per window pixel, and give it to SaveFrame(). If more than
//      one is drawn, the first is saved, as IsSavingFrame() is then false.
// 4. Call EndFrame().
//
// After the last frame:
// - Exit with an error if HasFailed(), or if the loop stopped before IsDone()
//   (such as when the window was closed), as the run didn't finish.
//
// Throughout, log what goes wrong as errors, which a pilot may count as
// failures.
//
// Only pairing BeginFrame() and EndFrame(), and calling SaveFrame() only when
// asked and once a frame, are checked (in Debug). A program that leaves out the
// rest isn't stopped, but its run doesn't do what was asked of it.
//
// Positions are in window pixels, which are the pixels of the saved frames, so
// a click at a point seen in a saved frame lands on what is drawn there.
//
// This class is thread-compatible. Everything that takes part in a run is
// expected to use it on the thread running the loop, and commands, conditions,
// item finders, and frame listeners are called on that thread, during
// BeginFrame() or EndFrame(), so they can use the program's state without
// locking.
class Pilot {
 public:
  // Runs a program command with the values given to it, returning an error if
  // it fails.
  using Command =
      absl::AnyInvocable<absl::Status(absl::Span<const std::string> args)>;

  // Returns whether a program condition holds now.
  using Condition = absl::AnyInvocable<bool()>;

  // Returns where each item with the label is, as drawn in the last frame, or
  // nothing if no item has it.
  using ItemFinder =
      absl::AnyInvocable<std::vector<PilotPoint>(std::string_view label)>;

  // Called as each frame begins, once it is set up.
  using FrameListener = absl::AnyInvocable<void()>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  Pilot(const Pilot&) = delete;
  Pilot& operator=(const Pilot&) = delete;

  // Every handle from the pilot must be destroyed first.
  virtual ~Pilot();

  //----------------------------------------------------------------------------
  // Registration
  //
  // Each registration lasts until its handle is destroyed. A command,
  // condition, item finder, or frame listener may add and remove registrations
  // (its own included) while it runs.
  //----------------------------------------------------------------------------

  // Adds a command or condition that the pilot can name. Each name may only be
  // registered once at a time, for commands and conditions separately: a name
  // that is already registered logs an error, and returns a handle that holds
  // nothing.
  [[nodiscard]] PilotHandle AddCommand(std::string_view name, Command command);
  [[nodiscard]] PilotHandle AddCondition(std::string_view name,
                                         Condition condition);

  // Adds an item finder, which a GUI registers to say where its items are.
  // Every item finder is asked about a label, and only one item in all of them
  // may have it.
  [[nodiscard]] PilotHandle AddItemFinder(ItemFinder finder);

  // Adds a frame listener, which BeginFrame() calls once the frame is set up,
  // before the rest of the frame runs (such as to apply the frame's input).
  // Frame listeners are called in the order they were added.
  [[nodiscard]] PilotHandle AddFrameListener(FrameListener listener);

  //----------------------------------------------------------------------------
  // Frames
  //----------------------------------------------------------------------------

  // Called by the loop around each frame, in pairs.
  void BeginFrame();
  void EndFrame();

  // Whether the loop should stop, which it checks before each frame, and once
  // it has stopped, whether the run failed.
  bool IsDone() const { return DoIsDone(); }
  bool HasFailed() const { return DoHasFailed(); }

  // The game time each frame stands for. The loop passes it to the game as its
  // delta time in place of the time measured, and the GUI uses it too, so a run
  // is the same however fast the program runs.
  absl::Duration GetDeltaTime() const { return DoGetDeltaTime(); }

  // The frame in progress, counting from 0, or between frames, the next one.
  int GetFrame() const { return frame_; }

  // Whether the program's window is hidden for the run. This holds from before
  // the first frame.
  bool IsWindowHidden() const { return DoIsWindowHidden(); }

  //----------------------------------------------------------------------------
  // Input
  //----------------------------------------------------------------------------

  // Every key name the input uses, so the names can be checked before the
  // first frame.
  absl::Span<const std::string> GetKeyNames() const { return DoGetKeyNames(); }

  // The input of the frame in progress, in order, and the mouse after it.
  absl::Span<const PilotInput> GetInput() const { return DoGetInput(); }
  PilotMouse GetMouse() const { return DoGetMouse(); }

  //----------------------------------------------------------------------------
  // Saving frames
  //----------------------------------------------------------------------------

  // Called by the render system: whether a frame drawn now is to be saved, and
  // if so, its image once drawn. Only one image is saved a frame, so
  // IsSavingFrame() is false outside a frame, and once SaveFrame() has been
  // called in it. SaveFrame() may only be called when IsSavingFrame().
  bool IsSavingFrame() const {
    return in_frame_ && !frame_saved_ && DoIsSavingFrame();
  }
  void SaveFrame(const Image& image);

 protected:
  Pilot() = default;

  //----------------------------------------------------------------------------
  // Derived class interface
  //
  // A subclass decides what happens each frame, and the base class keeps the
  // registrations and checks the calls made by the program. FakePilot is the
  // smallest subclass, and StepPilot a complete one.
  //----------------------------------------------------------------------------

  // Run what is registered under the name, or find the item with the label.
  // Each fails, with an error saying so, if nothing is registered by that
  // name, and FindItem() also fails if more than one item has the label. These
  // errors are the subclass's to report, as failures of the run.
  absl::Status RunCommand(std::string_view name,
                          absl::Span<const std::string> args);
  absl::StatusOr<bool> CheckCondition(std::string_view name);
  absl::StatusOr<PilotPoint> FindItem(std::string_view label);

  // Implement the public functions above. Programs rely on the following:
  //
  // - DoBeginFrame() sets up the frame: its input, the mouse, and whether it is
  //   saved. DoEndFrame() finishes it. The pilot does its own work (running
  //   commands, checking conditions, and finding items) in these, so it happens
  //   on the loop thread, between the program's frames.
  // - DoGetInput(), DoGetMouse(), and DoIsSavingFrame() don't change between
  //   BeginFrame() and EndFrame(), as the program may read them any number of
  //   times, in any order.
  // - DoIsDone() stays true once true. DoHasFailed() may become true at any
  //   time, and a pilot that has failed is done by the end of that frame.
  // - DoGetDeltaTime(), DoIsWindowHidden(), and the key names already given by
  //   DoGetKeyNames() hold for the whole run, from before the first frame.
  // - DoSaveFrame() is called at most once a frame, and only when saving.
  virtual void DoBeginFrame() = 0;
  virtual void DoEndFrame() = 0;
  virtual bool DoIsDone() const = 0;
  virtual bool DoHasFailed() const = 0;
  virtual absl::Duration DoGetDeltaTime() const = 0;
  virtual bool DoIsWindowHidden() const = 0;
  virtual absl::Span<const std::string> DoGetKeyNames() const = 0;
  virtual absl::Span<const PilotInput> DoGetInput() const = 0;
  virtual PilotMouse DoGetMouse() const = 0;
  virtual bool DoIsSavingFrame() const = 0;
  virtual void DoSaveFrame(const Image& image) = 0;

 private:
  friend class PilotHandle;

  // A registered callback, held by shared pointer so it outlives its removal
  // while it runs. Item finders and frame listeners have no name.
  template <typename Callback>
  struct Registration {
    int id;
    std::string name;
    std::shared_ptr<Callback> callback;
  };

  // Returns the callback registered under the name, or null.
  template <typename Callback>
  static std::shared_ptr<Callback> Find(
      const std::vector<Registration<Callback>>& registrations,
      std::string_view name);

  // Calls `call` with each callback registered when this is called, in order,
  // skipping any removed by an earlier call.
  template <typename Callback, typename Call>
  static void CallEach(const std::vector<Registration<Callback>>& registrations,
                       Call call);

  // Removes the registration with the ID, returning its callback, or null if
  // it isn't in `registrations`.
  template <typename Callback>
  static std::shared_ptr<void> Take(
      std::vector<Registration<Callback>>& registrations, int id);

  // Removes the registration with the ID.
  void Remove(int id);

  int next_id_ = 1;
  int frame_ = 0;
  bool in_frame_ = false;
  bool frame_saved_ = false;
  std::vector<Registration<Command>> commands_;
  std::vector<Registration<Condition>> conditions_;
  std::vector<Registration<ItemFinder>> item_finders_;
  std::vector<Registration<FrameListener>> frame_listeners_;
};

}  // namespace gb

#endif  // GB_PILOT_PILOT_H_
