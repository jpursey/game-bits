# Game Bits Templates

This directory contains a number of basic Game Bits templates. To create a new 
project based on these templates, copy the directory to a new location and
define the `GB_DIR` environment variable to refer to the root `game-bits`
folder.

Each template is a standalone CMake project, and is built the same way as Game
Bits itself: either open the copied directory as a local folder in Visual
Studio, or build it from a shell that has an x64 MSVC developer environment set
up (via `vcvars64.bat`, or `Enter-VsDevShell` in PowerShell):

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B out/build/x64-Debug
cmake --build out/build/x64-Debug
```

The resulting executable is written to the template's `bin/` directory.

## Building for the web

Both templates also build for the web, on Linux. The [cli](cli) template is a
program that runs under Node with the machine's own files, environment, and
arguments, as it does on any other platform. This needs the Emscripten SDK,
installed with Game Bits' `tools/install-emsdk.sh` (see "Building for the web"
in Game Bits' README.md), and its environment in the shell. From the copied
directory, with `GB_DIR` set:

```
source ~/emsdk/emsdk_env.sh
emcmake cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B out/build/web-Debug
cmake --build out/build/web-Debug
"$EMSDK_NODE" bin/cli_template.js
```

The program is `bin/cli_template.js`, beside its `bin/cli_template.wasm`, and
is run with the Node that comes with the SDK (`EMSDK_NODE`), which is the one
`ctest` runs Game Bits' tests with. Add `-DGB_WEB_THREADS=ON` to the configure
command (in a build tree of its own, such as `out/build/web-threads-Debug`)
for a program that can start threads. Each build writes its program to the
same `bin/`, replacing the other's. The first build in each tree is a full
build of the Game Bits libraries, about two minutes on 4 cores.

A windowed program, such as the [sdl](sdl) template, builds for the web as a
page instead: `bin/sdl_template.html`, beside its `.js` and `.wasm`, with SDL's
window as the page's canvas. A browser opens it from a web server, not a
`file:` address, such as Python's, from the template's `bin/`:

```
python3 -m http.server 8000
```

Then open `http://localhost:8000/sdl_template.html`. Each `arg` in the page's
address is one of the program's arguments, in order
(`sdl_template.html?arg=--help`), and its output goes to the browser's console.

## Running with a pilot script

The [sdl](sdl) template takes Game Bits' pilot flags (`--pilot <script>` and
`--pilot-output <folder>`), so a script can drive it, as Game Bits' examples
are driven: Game Bits' `tools/run-program.sh` runs it with a script of steps,
on Linux in a virtual X server and as a page in headless Chromium, and saves
its log to `out/run/sdl_template/`:

```
$GB_DIR/tools/run-program.sh --exe bin/sdl_template --steps 'hide window; wait 30; key A; resize 640 360; wait 2'
$GB_DIR/tools/run-program.sh --exe bin/sdl_template.html --steps 'hide window; wait 30; key A; resize 640 360; wait 2'
```

`src/gb/pilot/script/pilot_script.h` in Game Bits lists the steps.

## Templates

  - **[cli](cli)**: This template is the most minimal and has nothing but 
    logging initialization in a command line application.
  - **[sdl](sdl)**: This template is a bare bones game: a `gb::Game` with an
    SDL window, which logs the input it gets, and quits on Escape.

## Examples

For more complete game examples showing fuller integration with Game Bits libraries, see the examples directory parallel to this one.
