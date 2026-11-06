# Scripted runs of graphical programs

A graphical program built on Game Bits can run a script of steps by itself, so
a session can check its drawing and its GUI without the user. The script is a
text file named on the command line. It counts time in frames, gives the
program key and mouse input (ImGui included, without touching the real
cursor), runs commands and waits on conditions the program names, presses
ImGui items by their labels, and saves exact frames as PNGs read back from the
renderer, which works with the window hidden. When the script ends the program
exits normally, and its exit code and log say whether every step passed.

Driving a program from outside, by posting key messages to its window and
capturing it with `PrintWindow()`, can't use the mouse, waits in seconds rather
than frames, can't see what the program is doing, and needs the window on
screen. So `tools/run-program.ps1` no longer does: it launches the program with
a script, catches a hang, and reports what it saved.

render-check and block-world both run scripts, in
`examples/render-check/scripts/` and `examples/block-world/scripts/`.

## Behavior

### Turning it on

A program opts in where it already creates each piece. Programs on `gb::Game`
that create their systems with `SetParent(context)` get most of it from the
context:

```
// main(): the flags come from gb_pilot_script, parsed by gb_cli.
const gb::CliCommand kProgram = {
    .name = "render_check",
    .flags = {&gb::kCliPilotScript, &gb::kCliPilotOutput},
    .run = Run,
};

bool Run(gb::ValidatedContext& context, const gb::CliCommandLine& line) {
  // Null with no --pilot flag, so the program runs as usual, and an error if
  // the script has errors. The context is passed as a CliPilotContract, which
  // validates that it has the file system. Everything else sees only the Pilot
  // interface.
  absl::StatusOr<std::unique_ptr<gb::Pilot>> pilot =
      gb::CreateCliPilot(context, line);
  if (!pilot.ok()) {
    return false;
  }
  RenderCheck game;
  return game.Run(gb::ContextBuilder()
                      .SetParent(context)
                      .SetOwned(*std::move(pilot))
                      .Build(),
                  {});
}

// main(): paths on the command line are opened as the OS opens them.
gb::FileSystem file_system;
file_system.Register(gb::LocalFileProtocol::CreateOsRoot());
file_system.SetDefaultProtocol("file");
gb::Context context;
context.SetPtr<gb::FileSystem>(&file_system);
return gb::RunCommandLine(&context, &kProgram, gb::GetMainArgs(argc, argv));

// Game::Init(): after creating the window, if there is a pilot.
pilot_sdl_ = gb::PilotSdl::Create(pilot, window_);

// Anywhere, through the interface: commands and conditions the script can
// name.
commands_ = pilot->AddCommand("reset_peaks", [this](auto args) { ... });
loaded_ = pilot->AddCondition("world_loaded", [this] { return ...; });
```

- `Game` runs a frame per pilot frame, on a fixed time step, without
  sleeping, and stops when the script ends. `Run()` returns false if a step
  failed.
- `RenderSystem` and `ImGuiInstance` find the pilot in their contexts.
  The render system saves the frames the script asks for, and the GUI takes
  its mouse and delta time from the script and reports its items' places.
- `PilotSdl` adds the script's input to SDL's event queue each frame and
  drops the user's real input, so a run is the same whatever the user does.

None of these libraries is needed. A program with its own loop, renderer, GUI,
or input does for itself what each of them does, as listed in Taking part in
a run. A program can also drive itself from code, with no script file, by
giving a `StepPilot` its steps:

```
gb::StepPilot pilot({.save_frame = [&](std::string_view name,
                                        const gb::Image& image) {
  return gb::SaveImage(&file_system, absl::StrCat("out/", name, ".png"),
                       image.View());
}});
pilot.AddSteps({gb::PilotWaitStep{.frames = 30},
                gb::PilotMouseClickStep{.target = "Get layer 1"},
                gb::PilotWaitStep{.frames = 2},
                gb::PilotSaveStep{.name = "get_layer_1"}});
```

### Scripts

One step per line; `#` starts a comment. Names (of keys, commands,
conditions, and saved frames) are identifiers as in C, or integers. Other
names, text, and labels are quoted.

```
# Waits for the program to settle, then presses a button by its label.
hide window                # Before the first frame, if at all
wait 30
click "Get layer 1"
wait 2
save get_layer_1           # <output>/get_layer_1.png
key f1                     # Down this frame, up the next
key "Left Shift"           # SDL's name for a key, quoted if it has spaces
key w for 20               # Down for 20 frames, up the next
move 640 400               # Mouse, in window pixels
move by 100 0              # From where it is, such as to turn a camera
click right                # At the mouse
click 700 120              # Moves there first
click "Get layer 1" for 30 # Holds the button for 30 frames
press "Extra rectangles"   # Holds the button until released, to drag
move 400 300
release                    # Left, unless named as for click
press key "Left Shift"     # Holds the key until released
click "Get layer 1"
release key "Left Shift"
wheel -2
text "hello"
run reset_peaks            # A program command, with any further values
wait for world_loaded 600  # A program condition, failing after 600 frames
expect world_loaded        # Fails if the condition doesn't hold now
expect error "cannot be edited" 10  # Errors with this text pass, for 10 frames
```

- Time passes only in steps that wait or give input that must not share a
  frame: `wait`, `wait for`, `key`, `click`, `press`, and `release`, and
  `wheel` after a move. Every other step happens in the current frame, in
  order.
- A key or button is held for one frame unless given a number of frames, and
  then released in a frame with no other input, so `key` and `click` take two
  frames by default. A program that polls input (as ImGui does the mouse) sees
  only how it stands in each frame, so it would miss a release followed by
  another press or a move in the same frame.
- A `click` or `press` moves the mouse to its point or label first, if it isn't
  there, and never presses in a frame the mouse moves in: the press comes a
  frame later. A `wheel` is never in a frame the mouse moves in either, and
  turns at the start of the next frame instead. A program that judges a press
  or the wheel by where the mouse was in the frame before (such as whether
  ImGui wants the mouse, as block-world does) sees it there, as it would a real
  mouse, so a script needs no `move` and `wait` before a `click`, `press`, or
  `wheel`.
- `press` and `press key` hold a button or key while later steps run, until
  `release` or `release key`, so moves between them drag. A press takes its
  frame, so what follows comes in a later frame, and a release is in a frame
  with no other input (the next one, if a step before it in the frame gave
  input). Pressing what is held, releasing what isn't, or ending the run with
  anything held fails the run.
- `wait for` waits at most 600 frames (10 seconds) unless given a limit.
- `expect error` goes before the step that logs the error, for a check that
  is meant to fail. Errors whose message contains its text don't fail the
  run, for 600 frames unless given a limit, and it fails if none is logged by
  then or by the end of the run.
- `save` saves the next frame drawn, as RenderSystem draws nothing while the
  window is minimized. It fails if none is drawn within 600 frames.
- `click "<label>"` presses the ImGui item with that label (the text shown,
  before any `##`), as drawn in the last frame. It fails if no item has the
  label, or more than one does, unless it is named with its window:
  `click "Render Check/Get layer 1"`.
- Errors in the script fail the run before its first frame, with the line
  and column.

### Results

- Every step is logged as it runs, with its frame. A failure is logged as an
  error and fails the run. So does any error logged by the program while the
  script runs (the Vulkan validation layer's included).
- The script ends after its last step, or at its first failure. Either way,
  the program then shuts down normally, so every run also checks that it
  exits cleanly.
- A summary is logged last: passed or failed, the frames run, and the frames
  saved.

### Running from a session

```
.\tools\run-program.ps1 -Exe bin\render_check.exe -Script examples\render-check\scripts\textures.txt
```

The script launches the program with `--pilot <script> --pilot-output
out\run\<program>`, waits for it to exit, and fails if it exits with an error
code or runs longer than `-TimeoutSecs` (60), saving every thread's stack first.
The frames and the log are in `out\run\<program>\`, and it prints the frames
saved, the log's errors, and the run's summary. `-Steps` takes a script inline,
with `;` between steps, and `-Arguments` the program's other flags.

## Design

### Names

The backlog calls this "scripted runs". The library and class need a name of
their own:

- **Pilot** (`gb_pilot`, `Pilot`, `PilotSdl`, "pilot script"). Plain, short
  enough that the names built on it can say exactly what they are
  (`PilotMouseButtonEvent`), and nothing in Game Bits or the projects on it
  uses it. *Chosen.*
- **Autopilot** (`gb_autopilot`). The same meaning, but long enough that the
  names built on it were pulled shorter, and less clear.
- **Driver** (`ScriptDriver`). Collides with GPU and device drivers.
- **Bot** (`ScriptBot`). In games, a bot is an AI player.
- **Run script** (`gb_run_script`, `RunScript`). Says what it is, but "run"
  is already `Game::Run()`, `CliCommand::run`, and `RunCommandLine()`.
- **Playback** (`gb_playback`, `Playback`). Suggests replaying recorded input,
  which this isn't (yet), and audio.

"Script" alone is avoided: README.md offers `gb_parse` for "game scripting
languages". "Rig" is a skeletal rig in games.

Input names say which device they are from (`PilotMouseButton`, not
`PilotButton`), so gamepads can be added beside them. Keys are only on the
keyboard, so "key" is enough.

With "pilot", the pieces are:
- `Pilot` (`gb_pilot`): the interface everything else is given.
- `StepPilot` (`gb_pilot`): the pilot that runs a list of steps (`PilotStep`)
  given to it in code, with nothing else needed.
- `FakePilot` (`gb_pilot`): for tests, like `gb_job`'s
  `FakeJobSystem`.
- `PilotScript` (`gb_pilot_script`): the steps read from a script file, run
  by a `StepPilot`.
- `PilotSdl` (`gb_pilot_sdl`): SDL input.

`StepPilot` was chosen over `QueuePilot` (`gb_container` has a `Queue`) and
`ListPilot` (says how, not what). A script is already "one step per line", so
the steps in code and in a file have the same name. "Step" also names the
fixed time step, but always as "time step".

Other new names: `AddCommand()`, `AddCondition()`, `AddItemFinder()`, and
`AddFrameListener()` (the caller registers and the pilot keeps them, so "Add"),
`PilotHandle`
(what each returns), and `SaveImage()` in `gb_image`, matching `LoadImage()`.

### Tiers

The libraries that take part in a run (the game loop, the renderer, the GUI,
and SDL input) depend only on the `Pilot` interface, which they find in
their contexts. `StepPilot`, in `gb_pilot` itself, implements it, and
`gb_pilot_script` reads script files into its steps, which only programs
link. So no Game Bits library depends on script reading, `gb_parse`, or
`gb_cli` for this, and their tests use `FakePilot` rather than script files.

Each of these libraries is optional. A program that uses none of them can
still be piloted, by doing for itself what they do (see Taking part in a
run), and can be driven from code with a `StepPilot` without
`gb_pilot_script`.

| Library                         | Tier | Depends on                                              |
| ------------------------------- | ---- | ------------------------------------------------------- |
| `stb_image_write`               | 0    | the `stb` submodule (already vendored)                  |
| `gb_image`                      | 1    | adds `stb_image_write`                                  |
| `gb_pilot`                      | 1    | `gb_image`                                              |
| `gb_pilot_sdl`                  | 1    | `gb_pilot`, SDL                                         |
| `gb_game`                       | 1    | adds `gb_pilot`                                         |
| `gb_pilot_script`               | 2    | `gb_pilot`, `gb_cli`, `gb_file`, `gb_image`, `gb_parse` |
| `gb_render`, `gb_render_vulkan` | 2    | adds `gb_pilot`                                         |
| `gb_imgui`                      | 2    | adds `gb_pilot`                                         |

- Every edge points to a lower tier, except two within tier 1: `gb_pilot`
  on `gb_image`, and `gb_game` on `gb_pilot` (and through it `gb_image`,
  with `gb_file` and `stb`), for `SaveFrame(const Image&)`. Passing raw RGBA
  bytes instead would keep `gb_pilot` on `gb_base` alone, at the cost of
  the `Image` type; `gb_image` is small and low enough not to be worth it.
- `gb_pilot_script` is tier 2: it combines four tier 1 libraries
  (`gb_pilot`, `gb_cli`, `gb_image`, and `gb_parse`), and no tier 1
  library depends on it, so placing it there costs nothing.
  `gb_pilot_sdl` stays in tier 1 with `gb_pilot`, as an integration
  with SDL, as `gb_render_vulkan_sdl` is with `gb_render_vulkan`.
- `FakePilot` is in `gb_pilot` itself, as `FakeJobSystem` is in
  `gb_job` and `FakeClock` in `gb_base`. `gb_render_testing` is a library of
  its own because its `RenderTest` fixture needs GoogleTest, which
  `FakePilot` doesn't.
- Where a lower tier needs something from a higher one, it is given it: GUIs
  register item finders, the render system hands over frames, and
  `PilotSdl` registers a frame listener, which the pilot calls as each frame
  begins.
- Nothing goes in `gb_base`. The lowest library that takes part is `gb_game`.
  A `Clock` can't stand in for the pilot: `GameLoop()` spins on `Now()`
  for the last millisecond, so a clock that only advances when slept on never
  ends the spin, and the loop has to begin and end frames anyway. Counting
  logged errors is an absl log sink.
- `gb_pilot` is in `src/gb/pilot`, with `gb_pilot_sdl` in
  `src/gb/pilot/sdl` and `gb_pilot_script` in
  `src/gb/pilot/script`, as `gb_render_vulkan` is in `src/gb/render`.
  README.md lists `gb_pilot` (with `gb_pilot_sdl` under it) in tier 1,
  and `gb_pilot_script` in tier 2.

### Pilot interface (gb_pilot)

```
// Drives a program frame by frame. A program creates one (such as a
// StepPilot) and puts it in its context, where the game loop, render
// system, GUI, and SDL input find it.
class Pilot {
 public:
  virtual ~Pilot();

  // Registered by the program, so a step can name them. Each lasts until its
  // handle is destroyed.
  [[nodiscard]] PilotHandle AddCommand(std::string_view name, Command command);
  [[nodiscard]] PilotHandle AddCondition(std::string_view name,
                                         Condition condition);

  // Registered by GUIs, to say where a labelled item is.
  [[nodiscard]] PilotHandle AddItemFinder(ItemFinder finder);

  // Registered by code that acts as each frame begins, such as SDL input.
  // BeginFrame() calls each once the frame is set up.
  [[nodiscard]] PilotHandle AddFrameListener(FrameListener listener);

  // Called by the loop around each frame, in pairs.
  void BeginFrame();
  void EndFrame();
  bool IsDone() const;
  bool HasFailed() const;
  absl::Duration GetDeltaTime() const;
  int GetFrame() const;

  // Read by the SDL input before the first frame: whether to hide the window,
  // and every key name the input uses, so it can check them.
  bool IsWindowHidden() const;
  absl::Span<const std::string> GetKeyNames() const;

  // Input for the current frame, in order, and the mouse after it.
  absl::Span<const PilotInput> GetInput() const;
  PilotMouse GetMouse() const;

  // Called by the render system: whether this frame is to be saved, and its
  // image if so.
  bool IsSavingFrame() const;
  void SaveFrame(const Image& image);

 protected:
  Pilot() = default;

  // Looked up by the subclass as steps run. Each fails, with an error saying
  // so, if nothing by that name is registered.
  absl::Status RunCommand(std::string_view name,
                          absl::Span<const std::string> args);
  absl::StatusOr<bool> CheckCondition(std::string_view name);
  absl::StatusOr<PilotPoint> FindItem(std::string_view label);

  // What the subclass implements, behind the public functions above.
  virtual void DoBeginFrame() = 0;
  virtual void DoEndFrame() = 0;
  ...
};
```

- **Shared and specific:** the registries of commands, conditions, and item
  finders are the same for every pilot, so the base class keeps them, and
  subclasses look them up as steps run. Frames, input, and saving are each
  subclass's own. The public functions are not virtual, so the base class
  checks what every caller must get right in one place: `BeginFrame()` and
  `EndFrame()` in pairs (a `DCHECK`), and `SaveFrame()` only when
  `IsSavingFrame()`, once a frame.
- **Input** is in the pilot's own types, so it has no SDL in it. `PilotInput`
  is a `std::variant` of one struct per kind of event, each with only its own
  fields: `PilotKeyEvent` (a key down or up), `PilotTextEvent`,
  `PilotMouseMoveEvent`, `PilotMouseButtonEvent` (down or up), and
  `PilotMouseWheelEvent`. Keys are named as SDL names scancodes
  (`SDL_GetScancodeFromName()`), the names a reader of the script knows.
- **Command values** are passed as the text written in the script
  (`absl::Span<const std::string>`), which the runner already holds, and each
  command converts what it needs (backlog: *Typed values in gb_base*).
- **Delta time:** `GetDeltaTime()` is the game time each frame stands for.
  `Game` passes it to `Update()` as `delta_time` in place of the time it
  measured, and ImGui takes it as `io.DeltaTime`, so a run is the same however
  fast the machine is. The pilot owns it, as scripts count time in frames. It
  is never less than `Pilot::kMinDeltaTime` (1 ms), as ImGui asserts that time
  passes each frame.
- **Names** of commands and conditions are each registered once at a time.
  Registering one again logs an error (failing a script run) and returns a
  handle that holds nothing.
- **Handles** (`PilotHandle`) are RAII: a command, condition, or finder
  is removed when its handle is destroyed, so a game state's commands go with
  it, and nothing is left calling into a destroyed object. A handle must not
  outlive its pilot, which owns the registration (a `DCHECK` in the
  pilot's destructor that none are left).
- Thread-compatible: everything that takes part (the loop, the render system,
  ImGui, and SDL events) runs on the loop thread, and commands, conditions, and
  item finders are called there, during `BeginFrame()` or `EndFrame()`, so they
  can use the program's state without locking. Callbacks already run outside
  the registries, so registering from another thread could be made safe with
  a mutex, without changing the interface, once something needs it.

**Brittleness:** a program is only piloted as far as its code does what the
pilot asks (see Taking part in a run). The Game Bits libraries do it for the
programs that use them; a program with its own loop, renderer, GUI, or input
must do it itself. Of these, only pairing `BeginFrame()` and `EndFrame()`, and
saving only when asked and once a frame, are checked (by `DCHECK`s); the rest
show up as a run
that doesn't do what its steps say.

### Taking part in a run (gb_pilot)

Nothing in `gb_pilot` reaches into the program: a program is piloted only as
far as its code does what the pilot asks. `pilot.h` lists what that is, in
three phases, so the library stands on its own. Each Game Bits library that
takes part says in its own docs which parts it does (they are noted here),
and a program does the rest itself.

**Before the first frame:**
- Hide the window if `IsWindowHidden()`, while still drawing every frame at
  the window's size. *`PilotSdl`.*
- Check the names in `GetKeyNames()`, logging an error for any that can't be
  mapped to a key. *`PilotSdl`.*
- Register the program's commands and conditions (`AddCommand()`,
  `AddCondition()`), and an item finder for each GUI (`AddItemFinder()`),
  giving the center of each item with the label, in window pixels, as drawn
  in the last frame. Each is kept while it means something: a game state's
  while the state is active, and a GUI's while the GUI exists. They may also
  be added and removed later, at any time, but a pilot only finds what is
  registered when it looks. *The program; `ImGuiInstance` for its finder.*
- Register a frame listener (`AddFrameListener()`) for code that acts as each
  frame begins, from outside the program's frame (such as applying input by
  adding events to a queue the program reads). *`PilotSdl`.*

**Each frame, in this order:**
1. Stop if `IsDone()`, ending the run. *`Game`.*
2. Call `BeginFrame()`, which calls the frame listeners. The rest of the
   frame's functions are only called between this and `EndFrame()`. *`Game`.*
3. Run the frame:
   - Apply `GetInput()` in order, as if the user had given it, and ignore the
     user's real keyboard and mouse. Code that polls the mouse rather than
     reading events reads `GetMouse()`. Log an error for a key name that
     wasn't checked before the first frame and can't be mapped, as a pilot
     may learn of more keys as it runs. *`PilotSdl`, for SDL programs.*
   - Advance game time by `GetDeltaTime()`, rather than by the time measured,
     and don't wait on real time (vsync only slows a run down). A GUI that
     keeps its own frame time and mouse takes them from `GetDeltaTime()` and
     `GetMouse()`. *`Game`; `ImGuiInstance`.*
   - If `IsSavingFrame()`, read back the frame once drawn (RGBA, as
     presented, one pixel per window pixel), and give it to `SaveFrame()`.
     *`RenderSystem`.*
4. Call `EndFrame()`. *`Game`.*

**After the last frame:**
- Exit with an error if `HasFailed()`, or if the loop stopped before
  `IsDone()` (such as when the window was closed), as the run didn't finish.
  *`Game` (`Run()` returns false).*

Throughout, log what goes wrong as errors. A `StepPilot` fails the run on any
error logged while it runs that an `expect error` step doesn't expect, so a
program's own errors fail it.

Window pixels are the pixels of the saved frames, so a click at a point seen in
a saved frame lands on what is drawn there.

### Implementing a pilot (gb_pilot)

Driving a program needs no implementation: a `StepPilot` is given the steps
to run as plain structs (the same ones a script is read into), and does all
of the work of running them (see Steps from code). A new subclass is only for
a new way of deciding what happens each frame, beyond a list of steps (such as
one that reacts to what it sees). A subclass decides what happens each frame;
the base class keeps the registrations and checks the calls. `pilot.h` says
what each `Do*` function must give, which every program relies on:
- `DoBeginFrame()` sets up the frame: its input, the mouse, and whether it is
  saved. `DoEndFrame()` finishes it. The pilot does its own work (running
  commands, checking conditions, finding items) in these, so it happens on the
  loop thread, between frames of the program.
- `DoGetInput()`, `DoGetMouse()`, and `DoIsSavingFrame()` don't change between
  `BeginFrame()` and `EndFrame()`, as the program may read them any number of
  times, in any order.
- `DoIsDone()` stays true once true. `DoHasFailed()` may become true at any
  time, and a pilot that has failed is done by the end of that frame.
- `DoGetDeltaTime()`, `DoIsWindowHidden()`, and the key names already given by
  `DoGetKeyNames()` hold for the whole run, from before the first frame.
- `DoSaveFrame()` is called at most once a frame, only when saving.
- `RunCommand()`, `CheckCondition()`, and `FindItem()` reach the program. Their
  errors are the pilot's to report, as failures of the run.

`FakePilot` is the smallest implementation (it leaves being done and failing
to the test), and `StepPilot` the complete one.

### Steps from code (StepPilot, gb_pilot)

`StepPilot` runs a list of steps given to it in code. It needs nothing beyond
`gb_pilot`, so a program can be driven by a list of steps it builds itself, or
from any source, with no script file, `gb_parse`, or `gb_cli`. Script files are
one source of its steps (`gb_pilot_script`).

```
// One step of a run, as in a script (see Scripts).
struct PilotWaitStep { int frames; };
struct PilotWaitForStep { std::string condition; int max_frames = 600; };
struct PilotKeyStep { std::string key; int frames = 1; };  // Frames held.
struct PilotKeyPressStep { std::string key; };    // Held until released.
struct PilotKeyReleaseStep { std::string key; };  // Named as pressed.
struct PilotTextStep { std::string text; };
struct PilotMouseMoveStep { PilotPoint position; };
struct PilotMouseMoveByStep { PilotPoint offset; };  // From where it is.

// At the mouse, at a point, or at the item with a label.
using PilotMouseTarget = std::variant<std::monostate, PilotPoint, std::string>;
struct PilotMouseClickStep {
  PilotMouseButton button = PilotMouseButton::kLeft;
  PilotMouseTarget target;
  int frames = 1;  // Frames held.
};
struct PilotMousePressStep {  // Held until released.
  PilotMouseButton button = PilotMouseButton::kLeft;
  PilotMouseTarget target;
};
struct PilotMouseReleaseStep {  // Where the mouse is.
  PilotMouseButton button = PilotMouseButton::kLeft;
};
struct PilotMouseWheelStep { int amount; };
struct PilotRunStep { std::string command; std::vector<std::string> args; };
struct PilotExpectStep { std::string condition; };
struct PilotExpectErrorStep { std::string text; int max_frames = 600; };
struct PilotSaveStep { std::string name; };
using PilotStep = std::variant<PilotWaitStep, ..., PilotSaveStep>;

class StepPilot final : public Pilot {
 public:
  // Keeps a saved frame under its name. Returns false, having logged why, if
  // it can't, which fails the run.
  using FrameSaver =
      absl::AnyInvocable<bool(std::string_view name, const Image& image)>;

  struct Options {
    absl::Duration delta_time = absl::Seconds(1) / 60;
    bool hide_window = false;

    // Needed only if a step saves a frame.
    FrameSaver save_frame;
  };

  StepPilot();
  explicit StepPilot(Options options);

  // Adds steps to run after those already added. Steps may be added until the
  // run is done, including by a command or condition as it runs. Steps added
  // once it is done log an error, and are ignored.
  void AddSteps(std::vector<PilotStep> steps);
};
```

- **Timing** is as in Scripts: at the start of each frame (in
  `BeginFrame()`), steps run in order until one that takes time. A key or
  click presses in the frame it starts in, releases at the start of the frame
  `frames` (at least 1) later, and finishes at the end of that frame, so the
  release has a frame to itself. A press step presses in the frame it starts
  in and finishes at the end of it. A release step releases in the frame it
  starts in if no step before it has given input in that frame, or else in the
  next frame, and finishes at the end of the frame it releases in. A held key
  is released by the name it was pressed with. A wait, `wait for`, or save
  finishes at the end of a frame (in `EndFrame()`): once its frames have
  passed, its condition holds (checked when it starts, and after each frame),
  or its frame was saved. The steps after any of these run from the next
  frame.
  So the step after one that takes N frames always starts N frames later, and
  no frame is run after the last step needs it.
- **Done** once every step has finished, or at the first failure, decided at
  the end of a frame. A program that adds steps as it goes must add them
  before the end of the frame in which the last one finishes. With no steps,
  the run is done after one frame.
- **Failures:** a failed command, a missing command or condition, an `expect`
  that doesn't hold, a `wait for` that times out, a label that no item or more
  than one has, a `save` with no `save_frame`, a frame not saved within 600
  frames or that `save_frame` couldn't keep, an expected error that isn't
  logged, a key or button pressed while held, released while not, or held
  when the last step finishes, and any other error logged while it runs, from
  any thread (an absl log sink keeps them). The first ends the run.
- **Expected errors:** the log sink keeps each error's message, under a
  mutex, and the pilot takes them on the loop thread: at the start and end of
  each frame, and as each step starts, so errors logged before an
  `expect error` step are judged without it. An error whose message contains the text of an
  expectation still in its frames is expected, and logged as such; any other
  fails the run. An expectation lasts its `max_frames` frames, counting the
  one it starts in, even once met, as a check may log its error more than
  once. It fails when its frames end, or when the run ends, if no error met
  it. Several may be in effect at once, and one error meets every
  expectation it matches.
- **Logging:** each step is logged as it runs, with its frame, and a failure
  as an error. A summary is logged once done: passed or failed, the frames
  run, and the frames saved.
- **Key names** from every step added so far are in `GetKeyNames()`.
- **Hiding the window** is an option rather than a step, as it only means
  something before the first frame.
- Thread-compatible, like `Pilot`. Only the log sink is thread-safe.

### Fake pilot (gb_pilot)

`FakePilot` holds state that a test sets and reads: the delta time, when
it is done or failed, the input and mouse for the next frame, whether to save
a frame, and the frames it was given. Its `RunCommand()`, `CheckCondition()`,
and `FindItem()` are public, so a test can check what a program or GUI
registered. The tests for `Game`, `RenderSystem`, `ImGuiInstance`, and
`PilotSdl` use it.

### Script files (gb_pilot_script)

```
// The steps read from a script file.
struct PilotScript {
  bool hide_window = false;
  std::vector<PilotStep> steps;
};

// Reads a script. Returns an error giving the line and column of the first
// problem, after the filename if there is one.
absl::StatusOr<PilotScript> ReadPilotScript(std::string text,
                                            std::string_view filename = {});

// cli_pilot.h: `--pilot <script>` and `--pilot-output <folder>`.
extern const CliFlag<std::string_view> kCliPilotScript;
extern const CliFlag<std::string_view> kCliPilotOutput;

// Creates a StepPilot from the command line's --pilot flags, with a frame
// saver that writes PNGs to the output folder. Returns null if --pilot isn't
// given, or an error (logged) if the script can't be read.
using CliPilotContract = ContextContract<kConstraintCliFileSystem>;
absl::StatusOr<std::unique_ptr<Pilot>> CreateCliPilot(
    CliPilotContract contract, const CliCommandLine& command_line);
```

- **Script reading** is a `gb_parse` `ParserProgram`: a lexer config (C
  identifiers, double quoted strings with `\n`, `\t`, and `\xHH` escapes,
  32-bit integers including negative ones, `#` line comments, and line breaks
  as tokens) and a grammar, so a step that doesn't match fails with its line
  and column, and a new step is a new line of grammar. `ReadPilotScript()`
  turns each matched step into a `PilotStep` by the name of the alternative
  that matched. `hide window` becomes `hide_window`, and is only allowed
  before every other step. Frames can't be negative. Command and condition
  names are checked as each step runs, as the program may add them late (in a
  game state, say). Key names are kept as written; `PilotSdl` checks them.
  Every name may be an identifier, an integer (kept as written, so `key 1`
  and `save 007` work), or a string.

  ```
  line {
    %linebreak;
    $step=step:"Unknown step" end_of_line:"Expected end of line";
  }
  end_of_line { %linebreak; %end; }
  step {
    $hide_window=("hide" "window");
    $wait_for=("wait" "for" $condition=name [$frames=%int]);
    $wait=("wait" $frames=%int);
    $key=("key" $key=name ["for" $frames=%int]);
    $press_key=("press" "key" $key=name);
    $release_key=("release" "key" $key=name);
    $text=("text" $text=%string);
    $move_by=("move" "by" $x=%int $y=%int);
    $move=("move" $x=%int $y=%int);
    $click=("click" [$button=button] [$label=%string | $x=%int $y=%int]
            ["for" $frames=%int]);
    $press=("press" [$button=button] [$label=%string | $x=%int $y=%int]);
    $release=("release" [$button=button]);
    $wheel=("wheel" $amount=%int);
    $run=("run" $command=name $args=name*);
    $expect_error=("expect" "error" $text=%string [$frames=%int]);
    $expect=("expect" $condition=name);
    $save=("save" $name=name);
  }
  name { %ident; %int; %string; }
  button { "left"; "middle"; "right"; }
  ```

  Each line is a `Parse()` of `line`, until the next token is the end of the
  script, as OZ-3 parses its programs. So errors come in file order, and the
  last line needs no line break. The parser reports the error at the furthest
  token it reached, and of those the last one set, and a `:"..."` message is
  set at the token where its item starts. So a line that isn't a step is
  "Unknown step" at its first token, and values left over are "Expected end of
  line" at the first extra one. Missing values are reported by the step, at
  the token where the value should be.
- **Steps as text** read back as themselves, so a logged step can be pasted
  into a script. `gb_pilot` writes a name bare only if it is a C identifier or
  an integer as written in decimal, and escapes text only as the script reads
  it (`\"`, `\\`, `\n`, `\t`, and `\xHH` for other control characters,
  leaving UTF-8 as it is).
- **Saving:** `CreateCliPilot()`'s frame saver writes each frame as
  `<output>/<name>.png`, with `gb_image`'s `SaveImage()`, creating the output
  folder (and those above it) when it saves a frame, so a run that saves none
  creates nothing.
- **Command line:** `kCliPilotScript` (`--pilot <script>`, validated
  as an existing file) and `kCliPilotOutput` (`--pilot-output
  <folder>`, `out/pilot` by default), read and written through the command
  line's file system (`kConstraintCliFileSystem`). `CreateCliPilot()` takes
  its context as a `CliPilotContract`, so the command needn't list the file
  system in its own constraints, and a context without one fails there.
  The paths are opened as given, as `gb_cli` leaves opening paths to the
  program, so a program using these makes its protocol the file system's
  default rather than setting `cli_file_protocol`.
- Running, failures, and the summary are all `StepPilot`'s, so a script runs
  exactly as the same steps given in code would.

### Frames (gb_game)

`Game` takes the pilot from its context (`kConstraintPilot`,
optional). With one, `Run()` uses `PilotLoop()` in place of `GameLoop()`,
which passes the pilot's delta time as every `delta_time`, never uses the
clock (so there is no sleep or spin, and the max frame rate is unused), calls
`BeginFrame()` and `EndFrame()` around `Update()`, and stops once `IsDone()`.
`Run()` returns false if the pilot failed, which the programs already turn
into exit code 1. It also returns false, logging an error, if `Update()` stops
the game before the pilot is done (such as the window being closed), as the
run didn't finish. A game that stops in the frame the pilot finishes (a last
step that runs a quit command) passes.

The loop stays in `Game`, rather than in a `Pilot::RunFrames()` given a frame
callback: the pilot never runs the program's frames itself, it is only told
where each one begins and ends. So the pilot doesn't learn that a run was cut
short, and `Game`'s error is what reports it.

### Saving frames (gb_render, gb_render_vulkan)

```
// RenderBackend: ends the frame as EndFrame() does, then waits for the GPU
// and returns the presented image as RGBA. Null if it can't be read.
virtual std::unique_ptr<Image> EndFrameAndReadImage(RenderInternal) = 0;
```

- `RenderSystem::EndFrame()` reads the image, through the backend's
  `EndFrameAndReadImage()`, when its context's pilot
  (`RenderSystem::kConstraintPilot`, optional) `IsSavingFrame()`, and gives
  it to `SaveFrame()`. The pilot decides what to do with it (`StepPilot`
  hands it to its frame saver), so `gb_render` writes no files for this.
  Programs keep calling `EndFrame()` as they do now, and nothing else in
  `RenderSystem` reads back a frame.
- A frame that can't be read isn't given to the pilot. The backend logs
  why, which fails a `StepPilot` run.
- `Pilot::IsSavingFrame()` is false outside a frame, and once the frame is
  saved, so a program that draws twice in a pilot frame saves the first, and
  only it is read back. The once-a-frame `DCHECK` only catches a caller that
  saves without asking.
- `VulkanBackend` creates the swap chain with `eTransferSrc` usage too, when
  the surface supports it. To read a frame, it records a copy of the swap
  chain image to a host-visible buffer after the render pass (with layout
  transitions either side), submits and presents as usual, waits on the
  frame's fence, and converts the surface format (BGRA on Windows) to RGBA,
  opaque as it is presented. Only 8-bit RGBA and BGRA surfaces can be read.
- The wait stalls the CPU for one frame, on saved frames only.
- `TestRenderBackend` returns an image of the frame size filled with the
  clear color, so `gb_render`'s tests cover the plumbing.
- Adding a pure virtual to `RenderBackend` breaks any backend outside Game
  Bits. There are none in the sibling projects.

### GUI (gb_imgui)

- `ImGuiInstance` takes the pilot from its context. With one, it adds an
  ImGui context hook (`ImGuiContextHookType_NewFramePre`, run at the start of
  `ImGui::NewFrame()`, after the platform backend has set up `ImGuiIO`) that
  sets `io.MousePos` and `io.MouseDown` from `GetMouse()`, and
  `io.DeltaTime` from the delta time. It also clears `io.IniFilename`, so a
  run's window layout doesn't depend on earlier runs or change later ones. The SDL backend reads the real cursor
  in `ImGui_ImplSDL2_NewFrame()`, so this replaces what it read, and programs
  call both as they would without a pilot.
- `gb_imconfig.h` defines `IMGUI_ENABLE_TEST_ENGINE`, and `gb_imgui`
  implements its hooks (`ImGuiTestEngineHook_ItemAdd()` and `ItemInfo()`,
  which ImGui calls for every item with its rectangle and label). With an
  pilot, the instance turns them on (`TestEngineHookItems`) and keeps
  each frame's labelled items, and its item finder answers `click "<label>"`
  from the last frame's. Without one, they are off, and each item costs ImGui
  one branch.
- Keys and text reach ImGui as SDL events, through the program's own
  `ImGui_ImplSDL2_ProcessEvent()` calls.

### SDL input (gb_pilot_sdl)

```
class PilotSdl final {
 public:
  // Checks the script's key names, hides the window if the script asks, and
  // starts filtering input. Returns null if a key name is unknown.
  static std::unique_ptr<PilotSdl> Create(Pilot* pilot,
                                              SDL_Window* window);
};
```

- It registers a frame listener (`AddFrameListener()`), which `BeginFrame()`
  calls once the frame is set up. The listener adds the frame's input to SDL's
  queue as SDL events (key, text, mouse motion, button, and wheel), with
  `SDL_PeepEvents(SDL_ADDEVENT)`, which bypasses the event filter. Text longer
  than an SDL text event holds is split between UTF-8 characters.
- Its event filter (`SDL_SetEventFilter()`) drops the user's real input
  (keyboard, mouse, touch, text, and keyboard and mouse focus changes), so the
  user's mouse and keys can't change a run. Everything else passes, including
  window and quit events, so closing the window still ends the program. An
  event filter the program set first still sees what passes, and is set again
  when `PilotSdl` is destroyed. Setting a filter makes SDL discard the queued
  events, so `PilotSdl` takes them first and adds them back (dropping the
  user's input when it is created).
- Programs that poll SDL's state see the script's: it sets the modifier state
  (`SDL_SetModState()`, which ImGui reads for shift, ctrl, and alt), and the
  key state array that `SDL_GetKeyboardState()` returns (which BlockWorld's
  movement reads), each frame. Caps lock and num lock are off, so a run doesn't
  depend on the user's. SDL changes the state for a real key before filtering
  its event, so the filter sets the script's state again. Polling the mouse
  (`SDL_GetMouseState()`) still sees the real mouse, so programs read mouse
  input from events.
- A `hide window` step (at the top of a script) hides the window, so a run
  doesn't disturb the desktop.

**Brittleness:** `SDL_GetKeyboardState()` returns a `const` pointer to SDL's
own array, which this writes. It works because SDL 2 keeps the array in
writable memory and only reads it, but it relies on that. A program that sets
its own event filter while `PilotSdl` exists must remove it before `PilotSdl`
is destroyed (a `DCHECK`). SDL can also change the modifier state without a
key event (caps lock and num lock as the window gains focus), which lasts until
the next frame begins.

### Session tools

`tools/run-program.ps1` takes `-Script`, or `-Steps` (a script written inline
with `;` between steps, saved to `steps.txt` in the output folder), and with
neither saves one frame from a hidden window. It launches the program with the
pilot flags and any `-Arguments`, and its hang detection and stack dump work on
a run timeout. It lists only the frames this run saved, leaving any from
earlier runs, and prints the log's errors and the pilot's summary.
CLAUDE.md's "Run an example" says how a session uses it.

### Programs

- **render-check** runs on `gb::Game`, so its frames follow the pilot. Its
  storage buffer colors change by game time, and a check whose result isn't
  as expected logs an error, which fails a run. Its `gui_under_limit`
  condition holds unless the GUI is being kept over its vertex limit. One
  script per part of the panel presses its buttons and check boxes by label,
  collapsing the sections above to bring each into the window.
- **block-world** takes `--state` (TitleState by default) beside the pilot
  flags. With a pilot, `World::Draw()` builds every chunk mesh in view in the
  frame it is first drawn, rather than for 10 ms of real time a frame, so what
  is drawn doesn't depend on the machine. It leaves loading its window layout
  to ImGui's first frame, which `ImGuiInstance` stops under a pilot.

### Confirmed

- Vulkan keeps presenting to a window hidden with `SDL_HideWindow()`, at its
  size, and the surface supports `eTransferSrc` swap chain images on this GPU.
  Frames read back match `PrintWindow()` screenshots of the same frames.
- `SDL_PeepEvents(SDL_ADDEVENT)` skips the event filter in SDL 2.0.10, and
  writing SDL's key state array works.
- Mouse coordinates in SDL window pixels match the saved frame's pixels on a
  display at 100%, as render-check's scripts click items by label. A display
  scaled above 100% is still to check (backlog: *Pilot clicks on a scaled
  display*).
- The step grammar holds in `gb_parse`, with `wait for` beside `wait`,
  `click`'s optional forms, and `run`'s values stopping at the line break.
- ImGui 1.84 reads the mouse once a frame, so a release followed by another
  press in the same frame is missed. This is why each release has a frame to
  itself.

### Building blocks

Pieces other features can use:
- `SaveImage()` (`gb_image`), writing an image as a PNG to a file system or
  file.
- `StepPilot`, to drive a program from code with steps it builds, such as a
  program's own tests of its drawing, with no script file.
- `Pilot` and `PilotHandle`, for a new way of deciding each frame's input
  (a pilot that reacts to what it sees, or steps from a pipe), and
  `FakePilot` for tests of code that takes part.
- `RenderBackend::EndFrameAndReadImage()`, reading back the presented frame,
  which any screenshot feature can use.
- `PilotSdl`'s event filter and injected events, for giving an SDL program
  input it didn't get from the user.
- `ImGuiInstance`'s item finder (ImGui's test engine hooks), which finds a
  drawn item's place by its label.
- Steps convert to the text that reads back as them, so a logged step can be
  pasted into a script.
- `tools/run-program.ps1` and `tools/dump-stacks.ps1`, for running any program
  that takes the pilot flags, and for finding where any program is stuck.

## Checks

- Unit tests, against `FakePilot` for the libraries that take part:
  - `gb_image_test`: a saved PNG loads back with the same pixels, and saving
    to a file that can't be written fails.
  - `gb_pilot_test`: the registries and handles; `StepPilot`'s timing of every
    step, including held keys and clicks, its failures, expected errors (from
    any thread), the step log and summary; and steps read back from their
    text.
  - `gb_pilot_script_test`: every step and its values, comments, line breaks,
    escapes, and each error with its position; `CreateCliPilot()` with a
    memory file system, writing `<output>/<name>.png`.
  - `gb_game_test`: with a pilot, every `delta_time` is the pilot's, the clock
    is never slept on, and `Run()` returns whether the run passed.
  - `gb_pilot_sdl_test`: each kind of input arrives as SDL events in its
    frame, real input is dropped, quit events pass, and the key and modifier
    state match (SDL's events subsystem alone, no window).
  - `gb_render_test`: the pilot is given the frame it asked for, once, from
    the test backend.
  - `gb_imgui_test`: the item finder, the pilot's mouse pressing items, its
    delta time, and the ini file left alone.
- `examples/render-check/scripts/` cover the panel's checks: `instances`,
  `textures`, `texture_arrays`, `gui` (with the log and the vertex limit's
  expected error), and `scene`. Each passes, with the window shown and hidden,
  and the frames match apart from the process memory line. A script with a
  missing label, a timed out `wait for`, or an unexpected result exits with
  code 1 and says why.
- `examples/block-world/scripts/walk.txt` starts from the title and walks
  forward, and repeated runs save identical frames.
- Still by hand: render-check's resizing, and block-world's mouse (turning the
  camera, and placing and removing blocks).
