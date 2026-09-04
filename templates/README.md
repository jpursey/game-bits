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

## Templates

  - **[cli](cli)**: This template is the most minimal and has nothing but 
    logging initialization in a command line application.
  - **[sdl](sdl)**: This template brings up a bare bones SDL application with 
    logging integration.

## Examples

For more complete game examples showing fuller integration with Game Bits libraries, see the examples directory parallel to this one.
