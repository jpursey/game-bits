# Scripted runs of graphical programs

A graphical program built on Game Bits can run a script of steps by itself, so
a session can check its drawing and its GUI without the user. The script is a
text file named on the command line. It counts time in frames, gives the
program key and mouse input (ImGui included, without touching the real
cursor), runs commands and waits on conditions the program names, presses
ImGui items by their labels, and saves exact frames as PNGs read back from the
renderer, which works with the window hidden. When the script ends the program
exits normally, and its exit code and log say whether every step passed.

`tools/run-program.ps1` drives programs from outside today, posting key
messages to the window and capturing it with `PrintWindow()`. That can't use
the mouse, waits in seconds rather than frames, can't see what the program is
doing, and needs the window on screen. This replaces its stepping. The script
is left only to launch the program, catch a hang, and collect what it saved.

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
  ...
  RenderCheck game;
  return game.Run(gb::ContextBuilder().SetOwned(*std::move(pilot)).Build(),
                  {});
}

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
hold w 20                  # Down for 20 frames
move 640 400               # Mouse, in window pixels
click right                # At the mouse
click 700 120              # Moves there first
wheel -2
text "hello"
run reset_peaks            # A program command, with any further values
wait for world_loaded 600  # A program condition, failing after 600 frames
expect world_loaded        # Fails if the condition doesn't hold now
```

- Time passes only in steps that wait or hold input: `wait`, `wait for`,
  `key` and `click` (one frame each, for the release), and `hold`. Every other
  step happens in the current frame, in order.
- `wait for` waits at most 600 frames (10 seconds) unless given a limit.
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
code or runs longer than `-TimeoutSecs`, saving every thread's stack first, as
today. The frames and the log are in `out\run\<program>\`.

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
  command converts what it needs. A typed value is a backlog item.
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
error logged while it runs, so a program's own errors fail it.

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
struct PilotKeyStep { std::string key; };              // Down, then up.
struct PilotHoldStep { std::string key; int frames; };
struct PilotTextStep { std::string text; };
struct PilotMouseMoveStep { PilotPoint position; };
struct PilotMouseClickStep {
  PilotMouseButton button = PilotMouseButton::kLeft;

  // At the mouse, at a point, or at the item with a label.
  std::variant<std::monostate, PilotPoint, std::string> target;
};
struct PilotMouseWheelStep { int amount; };
struct PilotRunStep { std::string command; std::vector<std::string> args; };
struct PilotExpectStep { std::string condition; };
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
  click is released at the start of the next frame, and a hold `frames`
  frames later, and the steps after it run in that frame. A wait, `wait for`,
  or save finishes at the end of a frame (in `EndFrame()`): once its frames
  have passed, its condition holds (checked when it starts, and after each
  frame), or its frame was saved. The steps after it run from the next frame.
  So the step after one that takes N frames always starts N frames later, and
  no frame is run after the last step needs it.
- **Done** once every step has finished, or at the first failure, decided at
  the end of a frame. A program that adds steps as it goes must add them
  before the end of the frame in which the last one finishes. With no steps,
  the run is done after one frame.
- **Failures:** a failed command, a missing command or condition, an `expect`
  that doesn't hold, a `wait for` that times out, a label that no item or more
  than one has, a `save` with no `save_frame`, a frame not saved within 600
  frames or that `save_frame` couldn't keep, and any error logged while it
  runs, from any thread (an absl log sink counts them). The first ends the
  run.
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
    $key=("key" $key=name);
    $hold=("hold" $key=name $frames=%int);
    $text=("text" $text=%string);
    $move=("move" $x=%int $y=%int);
    $click=("click" [$button=button] [$label=%string | $x=%int $y=%int]);
    $wheel=("wheel" $amount=%int);
    $run=("run" $command=name $args=name*);
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
  `io.DeltaTime` from the delta time. The SDL backend reads the real cursor
  in `ImGui_ImplSDL2_NewFrame()`, so this replaces what it read, and programs
  call both as they do today.
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

`tools/run-program.ps1` loses its key and screenshot steps. It takes
`-Script` (or `-Steps`, a script written inline with `;` between steps, saved
to `out\run\<program>\steps.txt`), launches the program with the pilot
flags, and keeps its hang detection and stack dump, now on a run timeout.
CLAUDE.md's "Run an example" says so.

### Left out

- **Steps over a pipe**, which a session would write to and read results
  from. Runs are repeatable, so a session that wants to look at a frame
  before choosing the next step can run the script again with the step
  added. `StepPilot` takes steps from anywhere, and more while it runs, so a
  pipe can be added later as a source of steps. It would need an option to
  wait for more steps rather than be done when they run out.
- **Resizing the window** from a step, and polled mouse state
  (`SDL_GetMouseState()`) for programs other than ImGui.

### To confirm

- Confirmed in CL9, by render-check saving frames through a `StepPilot`:
  Vulkan keeps presenting to a window hidden with `SDL_HideWindow()`, at its
  size, and the surface supports `eTransferSrc` swap chain images on this GPU.
- Confirmed in CL5, by `gb_pilot_sdl`'s tests: `SDL_PeepEvents(SDL_ADDEVENT)`
  skips the event filter in SDL 2.0.10, and writing SDL's key state array
  works.
- Mouse coordinates in SDL window pixels match the saved frame's pixels,
  including on a display scaled above 100% (CL11).
- Confirmed in CL6, by `gb_pilot_script`'s tests: the step grammar holds in
  `gb_parse`, with `wait for` beside `wait`, `click`'s optional forms, and
  `run`'s values stopping at the line break.

## CLs

The interface comes first, then `StepPilot` beside it (CL3), then the tier 1
libraries that take part (CL4 and CL5), then tier 2: script files (CL6 and
CL7), and the render system and GUI (CL8 to CL10). Each library that takes
part is tested against `FakePilot`, so after CL2 they can be built in any
order.

### CL1 [x] stb, image: Save PNGs

Depends on: nothing.

- `src/stb`: an `stb_image_write` library, from the `stb` submodule already
  in `third_party`.
- `gb_image`: `SaveImage(FileSystem*, filename, const ImageView&)` and
  `SaveImage(File*, const ImageView&)`, writing a PNG.
- Unused, so no visible change.

**Verify**
- Standard checks (see CLAUDE.md).
- Unit tests: an image saved to a memory file system loads back with the same
  pixels; saving to a file that can't be written fails.

### CL2 [x] pilot: The interface, and a fake

Depends on: nothing.

- New `gb_pilot` library: `Pilot`, `PilotInput` and its events, `PilotMouse`,
  and `PilotHandle`, with the registries of commands, conditions, and item
  finders, and `FakePilot`.
- `pilot.h` documents what a piloted program must do (Taking part in a run),
  and what an implementation must give (Implementing a pilot).
- Unused, so no visible change.

**Verify**
- Standard checks.
- Unit tests, through `FakePilot`: commands run with their values, and a
  missing one fails; conditions likewise; a name registered twice logs an
  error; item finders are asked in turn, and a label two of them find is
  ambiguous; destroying a handle removes what it registered, and a callback
  can remove registrations (its own included) while it runs; unpaired
  `BeginFrame()` and `EndFrame()`, and saving a frame not being saved or
  twice, fail their `DCHECK` (death tests, in Debug).

### CL3 [x] pilot: Steps from code

Depends on: CL2.

- `gb_pilot`: `PilotStep` and its steps, and `StepPilot`: frames, input,
  commands, conditions, waits and timeouts, clicks on items, saving frames
  through `save_frame`, failures (with the log sink), the step log, and the
  summary.
- Unused, so no visible change.

**Verify**
- Standard checks.
- Unit tests, driving frames by hand as a program would: input by frame for
  each step; commands with values, a missing command, a failing command;
  `wait for` that passes and that times out; `expect`; a click at the place
  an item finder gives, and at a missing or ambiguous label; `save` giving
  `save_frame` the next frame drawn, and failing with no saver, when the
  saver fails, and when no frame is drawn; steps added while running,
  including by a command; done after the last step; an error logged from
  another thread failing the run; the key names of every step added.

### CL4 [x] game: Run frames for the pilot

Depends on: CL2.

- `Game`: `kConstraintPilot`, the fixed time step, no sleeping,
  `BeginFrame()` and `EndFrame()` around `Update()`, stopping when done, and
  `Run()` returning false on failure.

**Verify**
- Standard checks.
- Unit tests, with `FakePilot` and a `FakeClock` that would fail the test
  if slept on: every `delta_time` is the pilot's; the game stops when the
  pilot is done; `Run()` returns false when it failed, and true
  otherwise; without a pilot, nothing changes.

### CL5 [x] pilot_sdl: Input through SDL

Depends on: CL2.

- `gb_pilot`: `AddFrameListener()`, so `PilotSdl` learns when each frame
  begins.
- New `gb_pilot_sdl` library: `PilotSdl`, with the event filter, the
  injected events, key and modifier state, and `hide window`.

**Verify**
- Standard checks.
- Unit tests, with `FakePilot` and SDL's events subsystem alone (no
  video, so no window): each kind of input arrives as the right SDL events in
  its frame; pushed real input is dropped; quit events pass;
  `SDL_GetKeyboardState()` and `SDL_GetModState()` match the input given; an
  unknown key name fails `Create()`.
- Unit tests, with `FakePilot`: `BeginFrame()` calls frame listeners in order
  once the frame's input is set; a listener can add and remove listeners.
- Can't be unit tested: hiding the window, which needs SDL's video subsystem
  and a window on screen. CL11 checks it.

### CL6 [x] pilot_script: Read scripts

Depends on: CL3.

- New `gb_pilot_script` library: `PilotScript` and `ReadPilotScript()`, which
  reads a script into `PilotStep`s with a `ParserProgram` and the script
  grammar, with errors by line and column.
- `gb_pilot`: steps convert to text that reads back as the same step.
- Unused, so no visible change.

**Verify**
- Standard checks.
- Unit tests: every step and its values, including `click`'s forms and `run`
  with no values and with several; comments, blank lines, CRLF line breaks,
  and a last step with no line break after it; quoted and integer names,
  labels, window-qualified labels, and string escapes; `hide window` at the
  top, and failing after another step; a step that doesn't match, an unknown
  step, a step with values missing or left over, negative frames, and bad
  tokens, each failing with its position; steps from code read back from
  their text.

### CL7 [x] pilot_script: Scripts from the command line

Depends on: CL1, CL6.

- `gb_pilot_script`: `cli_pilot.h`, with `kCliPilotScript`, `kCliPilotOutput`,
  and `CreateCliPilot()`, with its frame saver writing PNGs.
- Unused, so no visible change.

**Verify**
- Standard checks.
- Unit tests, with a memory file system: no `--pilot` gives null; a context
  without a file system fails; a missing script fails parsing; a script with errors gives an error, logged; a script
  runs through the pilot created, driven frame by frame, hiding the window,
  and its `save` steps write `<output>/<name>.png`, which load back as the
  frame given, in `out/pilot` by default; a frame that can't be written fails
  the run.

### CL8 [x] render: Save frames

Depends on: CL2.

- `RenderBackend::EndFrameAndReadImage()`, and `RenderSystem::EndFrame()`
  saving for the pilot in its context.
- `TestRenderBackend` returns a frame of the clear color.
- `VulkanBackend` gets a stub returning null, so it builds; CL9 does the real
  one.
- `gb_pilot`: `IsSavingFrame()` is false outside a frame and once the frame
  is saved, so every caller saves at most once a frame.

**Verify**
- Standard checks.
- Unit tests, with `FakePilot`: `EndFrame()` reads nothing without a pilot,
  or unless the pilot is saving; the pilot is given the frame it asked for,
  at its size and in the clear color, and only that frame, and the first of
  two drawn in a pilot frame; a frame that can't be read isn't given to it.
- `gb_pilot`: `IsSavingFrame()` is false outside a frame and once saved.

### CL9 [x] render_vulkan: Read back presented frames

Depends on: CL8.

- `VulkanBackend`: transfer source swap chain images, the copy, the wait, and
  the format conversion.

**Verify**
- Standard checks.
- Can't be unit tested: checked with render-check, by temporary code that
  puts a `StepPilot` with save steps in its render system's context and
  calls the pilot's `BeginFrame()` and `EndFrame()` around each frame, run
  with `run-program.ps1` and its saved frames compared with its
  `PrintWindow()` screenshot, with the window shown and hidden, and with the
  validation layer reporting no errors. Removed before review.

### CL10 [x] imgui: Mouse, time, and items from the pilot

Depends on: CL2.

- `ImGuiInstance`: the new frame hook, the test engine hooks, and the item
  finder.
- `gb_imconfig.h`: `IMGUI_ENABLE_TEST_ENGINE`.

**Verify**
- Standard checks.
- Unit tests, with `FakePilot` and the test render backend: the item
  finder gives the centers of a button and a check box drawn in the last
  frame; a missing label fails, and so does one used in two windows unless
  qualified by its window; the fake's mouse, pressed there, presses the
  button and toggles the check box; `io.DeltaTime` is the pilot's delta time; without
  a pilot the test engine hooks are off.

### CL11 [ ] render-check: Scripted checks

Depends on: CL4, CL5, CL7, CL9, CL10.

- render-check runs on `gb::Game`, takes the pilot flags through `gb_cli`,
  and creates `PilotSdl`.
- Scripts in `examples/render-check/scripts/` for its hand checks: resizing
  is left out (no step resizes), and each other section's buttons and check
  boxes are pressed by label, with frames saved before and after.

**Verify**
- Standard checks.
- Each script runs with `run-program.ps1`, passes, and its saved frames show
  what the panel's text says they should, read by the session. The run is
  repeated with the window hidden, and the frames match.
- A script with a failing step, and one with a timed out `wait for`, exit
  with code 1 and say why in the log.
- By hand (the user): the program runs as before without the flags, and
  moving the mouse over the window during a run changes nothing.

### CL12 [ ] tools, docs: Run scripts from a session

Depends on: CL11.

- `tools/run-program.ps1` as described in Session tools.
- CLAUDE.md "Run an example", and README.md's library list (the new
  libraries) and render-check section.

**Verify**
- Standard checks.
- render-check's scripts run through the new `run-program.ps1`, with `-Script`
  and with `-Steps`. A script that hangs the program (a `wait for` with a
  timeout longer than `-TimeoutSecs`) saves the stacks and fails.

### CL13 [ ] block-world: Scripted runs

Depends on: CL12.

- BlockWorld takes its arguments through `gb_cli` (its starting state becomes
  a flag), and creates `PilotSdl`.
- A script that walks forward with `w` and saves frames before and after.

**Verify**
- Standard checks.
- The script runs with `run-program.ps1` and its frames show the view moving.
