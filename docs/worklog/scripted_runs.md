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
// main(): the flags come from gb_autopilot_script, parsed by gb_cli.
const gb::CliCommand kProgram = {
    .name = "render_check",
    .flags = {&gb::kCliAutopilotScript, &gb::kCliAutopilotOutput},
    .constraints = {&gb::kConstraintCliFileSystem},
    .run = Run,
};

bool Run(gb::ValidatedContext& context, const gb::CliCommandLine& line) {
  // Null with no --autopilot flag, so the program runs as usual. Everything
  // else sees only the Autopilot interface.
  std::unique_ptr<gb::Autopilot> autopilot =
      gb::CreateCliAutopilot(context, line);
  ...
  RenderCheck game;
  return game.Run(gb::ContextBuilder().SetOwned(std::move(autopilot)).Build(),
                  {});
}

// Game::Init(): after creating the window, if there is an autopilot.
autopilot_sdl_ = gb::AutopilotSdl::Create(autopilot, window_);

// Anywhere, through the interface: commands and conditions the script can
// name.
commands_ = autopilot->AddCommand("reset_peaks", [this](auto args) { ... });
loaded_ = autopilot->AddCondition("world_loaded", [this] { return ...; });
```

- `Game` runs a frame per autopilot frame, on a fixed time step, without
  sleeping, and stops when the script ends. `Run()` returns false if a step
  failed.
- `RenderSystem` and `ImGuiInstance` find the autopilot in their contexts.
  The render system saves the frames the script asks for, and the GUI takes
  its mouse and frame time from the script and reports its items' places.
- `AutopilotSdl` adds the script's input to SDL's event queue each frame and
  drops the user's real input, so a run is the same whatever the user does.

A program with its own loop calls `autopilot->BeginFrame()` and
`EndFrame()` around each frame and stops when `IsDone()`, as `Game` does.

### Scripts

One step per line; `#` starts a comment. Names (of steps, keys, commands,
conditions, and saved frames) are identifiers, as in C. Strings with spaces
are quoted.

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

The script launches the program with `--autopilot <script> --autopilot-output
out\run\<program>`, waits for it to exit, and fails if it exits with an error
code or runs longer than `-TimeoutSecs`, saving every thread's stack first, as
today. The frames and the log are in `out\run\<program>\`.

## Design

### Names

The backlog calls this "scripted runs". The library and class need a name of
their own:

- **Autopilot** (`gb_autopilot`, `Autopilot`, `AutopilotSdl`, "autopilot
  script"). Plain, and nothing in Game Bits uses it. *Recommended.*
- **Run script** (`gb_run_script`, `RunScript`). Says what it is, but "run"
  is already `Game::Run()`, `CliCommand::run`, and `RunCommandLine()`.
- **Playback** (`gb_playback`, `Playback`). Suggests replaying recorded input,
  which this isn't (yet), and audio.

"Script" alone is avoided: README.md offers `gb_parse` for "game scripting
languages".

With "autopilot", the pieces are:
- `Autopilot` (`gb_autopilot`): the interface everything else is given.
- `ScriptAutopilot` (`gb_autopilot_script`): the autopilot that runs a
  script, and `AutopilotScript`, the steps read from one.
- `FakeAutopilot` (`gb_autopilot`): for tests, like `gb_job`'s
  `FakeJobSystem`.
- `AutopilotSdl` (`gb_autopilot_sdl`): SDL input.

Other new names: `AddCommand()`, `AddCondition()`, and `AddItemFinder()` (the
caller registers and the autopilot keeps them, so "Add"), `AutopilotHandle`
(what each returns), and `SaveImage()` in `gb_image`, matching `LoadImage()`.

### Tiers

The libraries that take part in a run (the game loop, the renderer, the GUI,
and SDL input) depend only on the `Autopilot` interface, which they find in
their contexts. The script runner implements it, and only programs link it.
So no Game Bits library depends on script reading, `gb_parse`, or `gb_cli`
for this, and their tests use `FakeAutopilot` rather than script files.

| Library                         | Tier | Depends on                                                  |
| ------------------------------- | ---- | ----------------------------------------------------------- |
| `stb_image_write`               | 0    | the `stb` submodule (already vendored)                      |
| `gb_image`                      | 1    | adds `stb_image_write`                                      |
| `gb_autopilot`                  | 1    | `gb_base`, `gb_image`                                       |
| `gb_autopilot_sdl`              | 1    | `gb_autopilot`, SDL                                         |
| `gb_game`                       | 1    | adds `gb_autopilot`                                         |
| `gb_autopilot_script`           | 2    | `gb_autopilot`, `gb_cli`, `gb_file`, `gb_image`, `gb_parse` |
| `gb_render`, `gb_render_vulkan` | 2    | adds `gb_autopilot`                                         |
| `gb_imgui`                      | 2    | adds `gb_autopilot`                                         |

- Every edge points to a lower tier, except two within tier 1: `gb_autopilot`
  on `gb_image`, and `gb_game` on `gb_autopilot` (and through it `gb_image`,
  with `gb_file` and `stb`), for `SaveFrame(const Image&)`. Passing raw RGBA
  bytes instead would keep `gb_autopilot` on `gb_base` alone, at the cost of
  the `Image` type; `gb_image` is small and low enough not to be worth it.
- `gb_autopilot_script` is tier 2: it combines four tier 1 libraries
  (`gb_autopilot`, `gb_cli`, `gb_image`, and `gb_parse`), and no tier 1
  library depends on it, so placing it there costs nothing.
  `gb_autopilot_sdl` stays in tier 1 with `gb_autopilot`, as an integration
  with SDL, as `gb_render_vulkan_sdl` is with `gb_render_vulkan`.
- `FakeAutopilot` is in `gb_autopilot` itself, as `FakeJobSystem` is in
  `gb_job` and `FakeClock` in `gb_base`. `gb_render_testing` is a library of
  its own because its `RenderTest` fixture needs GoogleTest, which
  `FakeAutopilot` doesn't.
- Where a lower tier needs something from a higher one, it is given it: GUIs
  register item finders, the render system hands over frames, and
  `AutopilotSdl` pulls each frame's input.
- Nothing goes in `gb_base`. The lowest library that takes part is `gb_game`.
  A `Clock` can't stand in for the autopilot: `GameLoop()` spins on `Now()`
  for the last millisecond, so a clock that only advances when slept on never
  ends the spin, and the loop has to begin and end frames anyway. Counting
  logged errors is an absl log sink.
- `gb_autopilot` is in `src/gb/autopilot`, with `gb_autopilot_sdl` in
  `src/gb/autopilot/sdl` and `gb_autopilot_script` in
  `src/gb/autopilot/script`, as `gb_render_vulkan` is in `src/gb/render`.
  README.md lists `gb_autopilot` (with `gb_autopilot_sdl` under it) in tier 1,
  and `gb_autopilot_script` in tier 2.

### Autopilot interface (gb_autopilot)

```
// Drives a program frame by frame. A program creates one (such as a
// ScriptAutopilot) and puts it in its context, where the game loop, render
// system, GUI, and SDL input find it.
class Autopilot {
 public:
  virtual ~Autopilot();

  // Registered by the program, so a step can name them. Each lasts until its
  // handle is destroyed.
  [[nodiscard]] AutopilotHandle AddCommand(std::string_view name,
                                           Command command);
  [[nodiscard]] AutopilotHandle AddCondition(std::string_view name,
                                             Condition condition);

  // Registered by GUIs, to say where a labelled item is.
  [[nodiscard]] AutopilotHandle AddItemFinder(ItemFinder finder);

  // Called by the loop around each frame, in pairs.
  void BeginFrame();
  void EndFrame();
  bool IsDone() const;
  bool HasFailed() const;
  absl::Duration GetFrameTime() const;
  int GetFrame() const;

  // Input for the current frame, in order, and the mouse after it.
  absl::Span<const AutopilotInput> GetInput() const;
  AutopilotMouse GetMouse() const;

  // Called by the render system: whether this frame is to be saved, and its
  // image if so.
  bool IsSavingFrame() const;
  void SaveFrame(const Image& image);

 protected:
  Autopilot() = default;

  // Looked up by the subclass as steps run. Each fails, with an error saying
  // so, if nothing by that name is registered.
  absl::Status RunCommand(std::string_view name,
                          absl::Span<const std::string> args);
  absl::StatusOr<bool> CheckCondition(std::string_view name);
  absl::StatusOr<AutopilotPoint> FindItem(std::string_view label);

  // What the subclass implements, behind the public functions above.
  virtual void DoBeginFrame() = 0;
  virtual void DoEndFrame() = 0;
  ...
};
```

- **Shared and specific:** the registries of commands, conditions, and item
  finders are the same for every autopilot, so the base class keeps them, and
  subclasses look them up as steps run. Frames, input, and saving are each
  subclass's own. The public functions are not virtual, so the base class
  checks what every caller must get right in one place: `BeginFrame()` and
  `EndFrame()` in pairs (a `DCHECK`), and `SaveFrame()` only when
  `IsSavingFrame()`.
- **Input** is in the autopilot's own types (`AutopilotInput`: key down, key
  up, text, mouse move, button down and up, wheel), so it has no SDL in it.
  Keys are named as SDL names scancodes (`SDL_GetScancodeFromName()`), the
  names a reader of the script knows.
- **Handles** (`AutopilotHandle`) are RAII: a command, condition, or finder
  is removed when its handle is destroyed, so a game state's commands go with
  it, and nothing is left calling into a destroyed object. A handle must not
  outlive its autopilot, which owns the registration (a `DCHECK` in the
  autopilot's destructor that none are left).
- Thread-compatible: it is used on the thread running the loop.

**Brittleness:** a loop must call `BeginFrame()` and `EndFrame()` in pairs.
`Game` does this for every program on it; a program with its own loop must
remember, and the base class `DCHECK`s it.

### Fake autopilot (gb_autopilot)

`FakeAutopilot` holds state that a test sets and reads: the frame time, when
it is done or failed, the input and mouse for the next frame, whether to save
a frame, and the frames it was given. Its `RunCommand()`, `CheckCondition()`,
and `FindItem()` are public, so a test can check what a program or GUI
registered. The tests for `Game`, `RenderSystem`, `ImGuiInstance`, and
`AutopilotSdl` use it.

### Script runner (gb_autopilot_script)

```
class ScriptAutopilot final : public Autopilot {
 public:
  // OPTIONAL: Frames per second of the fixed time step. 60 by default.
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintFrameRate, ...);
  // REQUIRED: The file system the script is read from and frames saved to.
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kInRequired, FileSystem);

  // Reads and checks the script. Returns null, logging each error with its
  // line and column, if it can't be read or has errors.
  static std::unique_ptr<ScriptAutopilot> Create(Contract contract,
                                                 std::string_view script_path,
                                                 std::string_view output_path);
};
```

- **Script reading** (`AutopilotScript`) is a `gb_parse` `ParserProgram`: a
  lexer config (C identifiers, strings, and integers including negative ones,
  `#` line comments, and line breaks as tokens) and a grammar, so a step that
  doesn't match fails with its line and column, and a new step is a new line
  of grammar. `AutopilotScript` turns each matched step into a `Step` by the
  name of the alternative that matched. Command and condition names are
  checked as each step runs, as the program may add them late (in a game
  state, say). Key names are kept as written; `AutopilotSdl` checks them.

  ```
  script { ([$steps=step] %linebreak)* [$steps=step] %end ; }
  step {
    $hide_window=("hide" "window") ;
    $wait_for=("wait" "for" $condition=%ident [$frames=%int]) ;
    $wait=("wait" $frames=%int) ;
    $key=("key" $key=key_name) ;
    $hold=("hold" $key=key_name $frames=%int) ;
    $text=("text" $text=%string) ;
    $move=("move" $x=%int $y=%int) ;
    $click=("click" [$button=button] [$label=%string | $x=%int $y=%int]) ;
    $wheel=("wheel" $amount=%int) ;
    $run=("run" $command=%ident $args=value*) ;
    $expect=("expect" $condition=%ident) ;
    $save=("save" $name=%ident) ;
  }
  key_name { %ident ; %string ; }
  button { "left" ; "middle" ; "right" ; }
  value { %ident ; %int ; %string ; }
  ```

  `Parser::Parse()` succeeds when the root rule matches, whether or not it
  reached the end of the content, so the final `%end` is what makes a bad
  line an error rather than the end of the script. A trial of this rule in
  `gb_parse` found it parses blank lines, a last step without a line break,
  and an empty script, rejects two steps on a line, and collects both
  `$steps` in order, each telling which step it is by its name. A misspelled
  step is reported as "Expected end of file" at the step's name, so CL5 gives
  it a clearer message.
- **Saving** writes each frame it is given as a PNG with `gb_image`'s
  `SaveImage()`, in the output folder.
- **Failures:** a failed step, a wait that times out, a condition or command
  that doesn't exist, a frame that wasn't saved, and any error logged while
  running (a log sink counts them, from any thread). The first ends the
  script.
- **Command line:** `kCliAutopilotScript` (`--autopilot <script>`, validated
  as an existing file) and `kCliAutopilotOutput` (`--autopilot-output
  <folder>`, `out/autopilot` by default), and `CreateCliAutopilot()`, which
  creates the autopilot from them, or returns null if the script flag isn't
  given.
- Thread-compatible, like `Autopilot`. Only the log sink is thread-safe.

### Frames (gb_game)

`Game` takes the autopilot from its context (`kConstraintAutopilot`,
optional). With one, `GameLoop()` passes the autopilot's frame time as every
`delta_time`, skips its sleep and spin, calls `BeginFrame()` and `EndFrame()`
around `Update()`, and stops once `IsDone()`. `Run()` returns false if the
autopilot failed, which the programs already turn into exit code 1.

### Saving frames (gb_render, gb_render_vulkan)

```
// RenderBackend: ends the frame as EndFrame() does, then waits for the GPU
// and returns the presented image as RGBA. Null if it can't be read.
virtual std::unique_ptr<Image> EndFrameAndReadImage(RenderInternal) = 0;

// RenderSystem: the same, for programs that want a screenshot themselves.
std::unique_ptr<Image> EndFrameAndReadImage();
```

- `RenderSystem::EndFrame()` reads the image when its context's autopilot
  `IsSavingFrame()`, and gives it to `SaveFrame()`. The script runner writes
  the PNG, so `gb_render` writes no files for this.
- `VulkanBackend` creates the swap chain with `eTransferSrc` usage too, when
  the surface supports it. To read a frame, it records a copy of the swap
  chain image to a host-visible buffer after the render pass (with layout
  transitions either side), submits and presents as usual, waits on the
  frame's fence, and converts the surface format (BGRA on Windows) to RGBA.
- The wait stalls the CPU for one frame, on saved frames only.
- `TestRenderBackend` returns an image of the frame size filled with the
  clear color, so `gb_render`'s tests cover the plumbing.
- Adding a pure virtual to `RenderBackend` breaks any backend outside Game
  Bits. There are none in the sibling projects.

### GUI (gb_imgui)

- `ImGuiInstance` takes the autopilot from its context. With one, it adds an
  ImGui context hook (`ImGuiContextHookType_NewFramePre`, run at the start of
  `ImGui::NewFrame()`, after the platform backend has set up `ImGuiIO`) that
  sets `io.MousePos` and `io.MouseDown` from `GetMouse()`, and
  `io.DeltaTime` from the frame time. The SDL backend reads the real cursor
  in `ImGui_ImplSDL2_NewFrame()`, so this replaces what it read, and programs
  call both as they do today.
- `gb_imconfig.h` defines `IMGUI_ENABLE_TEST_ENGINE`, and `gb_imgui`
  implements its hooks (`ImGuiTestEngineHook_ItemAdd()` and `ItemInfo()`,
  which ImGui calls for every item with its rectangle and label). With an
  autopilot, the instance turns them on (`TestEngineHookItems`) and keeps
  each frame's labelled items, and its item finder answers `click "<label>"`
  from the last frame's. Without one, they are off, and each item costs ImGui
  one branch.
- Keys and text reach ImGui as SDL events, through the program's own
  `ImGui_ImplSDL2_ProcessEvent()` calls.

### SDL input (gb_autopilot_sdl)

```
class AutopilotSdl final {
 public:
  // Checks the script's key names, hides the window if the script asks, and
  // starts filtering input. Returns null if a key name is unknown.
  static std::unique_ptr<AutopilotSdl> Create(Autopilot* autopilot,
                                              SDL_Window* window);
};
```

- At each `BeginFrame()`, it adds the frame's input to SDL's queue as SDL
  events (key, text, mouse motion, button, and wheel), with
  `SDL_PeepEvents(SDL_ADDEVENT)`, which bypasses the event filter.
- Its event filter (`SDL_SetEventFilter()`) drops the user's real input
  (keyboard, mouse, text, and focus changes), so the user's mouse and keys
  can't change a run. Window and quit events still pass, so closing the
  window still ends the program.
- Programs that poll SDL's state see the script's: it sets the modifier state
  (`SDL_SetModState()`, which ImGui reads for shift, ctrl, and alt), and the
  key state array that `SDL_GetKeyboardState()` returns (which BlockWorld's
  movement reads). Polling the mouse (`SDL_GetMouseState()`) still sees the
  real mouse, so programs read mouse input from events.
- A `hide window` step (at the top of a script) hides the window, so a run
  doesn't disturb the desktop.

**Brittleness:** `SDL_GetKeyboardState()` returns a `const` pointer to SDL's
own array, which this writes. It works because SDL 2 keeps the array in
writable memory and only reads it, but it relies on that.

### Session tools

`tools/run-program.ps1` loses its key and screenshot steps. It takes
`-Script` (or `-Steps`, a script written inline with `;` between steps, saved
to `out\run\<program>\steps.txt`), launches the program with the autopilot
flags, and keeps its hang detection and stack dump, now on a run timeout.
CLAUDE.md's "Run an example" says so.

### Left out

- **Steps over a pipe**, which a session would write to and read results
  from. Runs are repeatable, so a session that wants to look at a frame
  before choosing the next step can run the script again with the step
  added. The script reader and runner don't care where steps come from, so a
  pipe can be added later as a source of steps.
- **Resizing the window** from a step, and polled mouse state
  (`SDL_GetMouseState()`) for programs other than ImGui.

### To confirm

- Vulkan keeps presenting to a window hidden with `SDL_HideWindow()`, at its
  size (CL8). If not, `hide window` moves it off screen instead.
- The surface supports `eTransferSrc` swap chain images on this GPU (CL8).
- `SDL_PeepEvents(SDL_ADDEVENT)` skips the event filter in SDL 2.0.10, and
  writing SDL's key state array works (CL4).
- Mouse coordinates in SDL window pixels match the saved frame's pixels,
  including on a display scaled above 100% (CL10).
- The rest of the step grammar holds in `gb_parse` as it did for `wait` and
  `save` in the trial: `wait for` beside `wait`, `click`'s optional forms, and
  `run`'s values stopping at the line break (CL5).

## CLs

The interface comes first, then the tier 1 libraries that use it (CL3 and
CL4), then tier 2: the script runner (CL5 and CL6), and the render system and
GUI (CL7 to CL9). Each library that uses the interface is tested against
`FakeAutopilot`, so after CL2 they can be built in any order.

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

### CL2 [ ] autopilot: The interface, and a fake

Depends on: nothing.

- New `gb_autopilot` library: `Autopilot`, `AutopilotInput`,
  `AutopilotMouse`, and `AutopilotHandle`, with the registries of commands,
  conditions, and item finders, and `FakeAutopilot`.
- Unused, so no visible change.

**Verify**
- Standard checks.
- Unit tests, through `FakeAutopilot`: commands run with their values, and a
  missing one fails; conditions likewise; item finders are asked in turn, and
  a label two of them find is ambiguous; destroying a handle removes what it
  registered; unpaired `BeginFrame()` and `EndFrame()` fail their `DCHECK`
  (death tests, in Debug).

### CL3 [ ] game: Run frames for the autopilot

Depends on: CL2.

- `Game`: `kConstraintAutopilot`, the fixed time step, no sleeping,
  `BeginFrame()` and `EndFrame()` around `Update()`, stopping when done, and
  `Run()` returning false on failure.

**Verify**
- Standard checks.
- Unit tests, with `FakeAutopilot` and a `FakeClock` that would fail the test
  if slept on: every `delta_time` is the frame time; the game stops when the
  autopilot is done; `Run()` returns false when it failed, and true
  otherwise; without an autopilot, nothing changes.

### CL4 [ ] autopilot_sdl: Input through SDL

Depends on: CL2.

- New `gb_autopilot_sdl` library: `AutopilotSdl`, with the event filter, the
  injected events, key and modifier state, and `hide window`.

**Verify**
- Standard checks.
- Unit tests, with `FakeAutopilot` and SDL's events subsystem alone (no
  video, so no window): each kind of input arrives as the right SDL events in
  its frame; pushed real input is dropped; quit events pass;
  `SDL_GetKeyboardState()` and `SDL_GetModState()` match the input given; an
  unknown key name fails `Create()`.

### CL5 [ ] autopilot_script: Read scripts

Depends on: CL2.

- New `gb_autopilot_script` library: `AutopilotScript`, the steps read from a
  script by a `ParserProgram` with the script grammar, with errors by line and
  column.
- Unused, so no visible change.

**Verify**
- Standard checks.
- Unit tests: every step and its values, including `click`'s forms and `run`
  with no values and with several; comments, blank lines, and a last step
  with no line break after it; quoted key names, labels, and window-qualified
  labels; a step that doesn't match, an unknown step, and a step with values
  missing or left over, each failing with its position.

### CL6 [ ] autopilot_script: Run scripts

Depends on: CL1, CL5.

- `ScriptAutopilot`: frames, input, commands, conditions, waits and timeouts,
  saving frames, failures (with the log sink), and the summary.
- `kCliAutopilotScript`, `kCliAutopilotOutput`, and `CreateCliAutopilot()`.
- Unused, so no visible change.

**Verify**
- Standard checks.
- Unit tests, driving frames by hand: input by frame for each step; commands
  with values, a missing command, a failing command; `wait for` that passes
  and that times out; `expect`; `click "<label>"` at the place an item finder
  gives; `save` writing a PNG to a memory file system, and failing when no
  frame is drawn; an error logged from another thread failing the run;
  parsing the flags.

### CL7 [ ] render: Save frames

Depends on: CL2.

- `RenderBackend::EndFrameAndReadImage()`, `RenderSystem::EndFrameAndReadImage()`,
  and `RenderSystem::EndFrame()` saving for the autopilot in its context.
- `TestRenderBackend` returns a frame of the clear color.
- `VulkanBackend` gets a stub returning null, so it builds; CL8 does the real
  one.

**Verify**
- Standard checks.
- Unit tests, with `FakeAutopilot`: `EndFrameAndReadImage()` returns the
  frame; the autopilot is given the frame it asked for, and only that frame.

### CL8 [ ] render_vulkan: Read back presented frames

Depends on: CL7.

- `VulkanBackend`: transfer source swap chain images, the copy, the wait, and
  the format conversion.

**Verify**
- Standard checks.
- Can't be unit tested: checked with render-check, by a temporary key that
  saves a frame through `RenderSystem::EndFrameAndReadImage()`, run with
  `run-program.ps1` and compared with its `PrintWindow()` screenshot, with the
  window shown and hidden, and with the validation layer reporting no errors.
  Removed before review.

### CL9 [ ] imgui: Mouse, time, and items from the autopilot

Depends on: CL2.

- `ImGuiInstance`: the new frame hook, the test engine hooks, and the item
  finder.
- `gb_imconfig.h`: `IMGUI_ENABLE_TEST_ENGINE`.

**Verify**
- Standard checks.
- Unit tests, with `FakeAutopilot` and the test render backend: the item
  finder gives the centers of a button and a check box drawn in the last
  frame; a missing label fails, and so does one used in two windows unless
  qualified by its window; the fake's mouse, pressed there, presses the
  button and toggles the check box; `io.DeltaTime` is the frame time; without
  an autopilot the test engine hooks are off.

### CL10 [ ] render-check: Scripted checks

Depends on: CL3, CL4, CL6, CL8, CL9.

- render-check runs on `gb::Game`, takes the autopilot flags through `gb_cli`,
  and creates `AutopilotSdl`.
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

### CL11 [ ] tools, docs: Run scripts from a session

Depends on: CL10.

- `tools/run-program.ps1` as described in Session tools.
- CLAUDE.md "Run an example", and README.md's library list (the new
  libraries) and render-check section.

**Verify**
- Standard checks.
- render-check's scripts run through the new `run-program.ps1`, with `-Script`
  and with `-Steps`. A script that hangs the program (a `wait for` with a
  timeout longer than `-TimeoutSecs`) saves the stacks and fails.

### CL12 [ ] block-world: Scripted runs

Depends on: CL11.

- BlockWorld takes its arguments through `gb_cli` (its starting state becomes
  a flag), and creates `AutopilotSdl`.
- A script that walks forward with `w` and saves frames before and after.

**Verify**
- Standard checks.
- The script runs with `run-program.ps1` and its frames show the view moving.
