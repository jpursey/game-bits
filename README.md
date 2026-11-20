# Game Bits

This is a collection of libraries used to make games... or really anything else.
This is an unapologetically opinionated "framework", which one day hopes to
grow up into a full featured Game Engine geared at hobbyist game programmers.

To jump right in and start building and using Game Bits, see the
[Getting started](#getting-started) section.

# Overview

This is the personal game engine for myself (John Pursey), which I use on my own
games. I more than welcome feedback, bug reports, feature requests. pull
requests, and am happy to directly help those who want to use it for their own
projects. However, I make no guarantees about long term interface stability,
will prioritize feature requests for my own games first, and am liable to
nitpick quite a bit on pull requests if they don't conform to the style and
design of Game Bits overall.

Why is this open source then? Well for one, I am happy to share and if it is
useful or interesting to family and friends, that's great. If someone else out
there finds this at all useful or interesting, then even better!

## Design principles

Game Bits libraries are developed with the following high level design
principles:

*  *Modern C++:* At this point, C++ is the dominant language for most game
   engines (if not necessarily games). Using C++ hopefully makes it useful to
   many who may be interested in it for game development. However, the main
   reason C++ was chosen is it is my dominant programming language, and it is an
   outlet for me to explore interesting C++ and game engine design patterns.
   Game Bits is generally an early adopter of new C++ revisions once compiler
   support becomes widespread.

*  *Platform independent:* Game Bits is developed on Windows and also builds on
   Linux, and everything is built with cross-platform support in mind. All
   dependencies on libraries other than the standard C/C++ library are
   relegated to platform specific implementations of a generic interface.

*  *Multithreaded:* Game Bits libraries are designed with explicit thread
   guarantees, and most are intended to be used in multithreaded contexts.
   "system" classes (those that manage resources for a specific module) are
   thread-safe after initial initialization, and classes/libraries which are
   intrinsically intended for broad use across threads treat thread-safety as
   a primary design consideration.

*  *Bug resistant:* Game Bits libraries are well tested (unit tests are near
   universal), and the APIs have well defined pre/post conditions and provide
   clear lifetime semantics which are supported by smart pointers and similar
   RAII classes.

*  *Fast enough:* Game Bits code is explicitly designed to be used for games,
   including games with high computing and resource demands. To that end, Game
   Bits code is not needlessly slow and algorithmic optimization is important.
   Systems where performance are critical to the design are further tuned and
   optimized (for instance, the JobSystem). However, performance is not the
   primary goal at the cost of being too tightly coupled, inflexible and/or
   error prone.

*  *Modular:* In a perfect world, every library would have no required
   dependencies on any other module, allowing people to only take the bits they
   want and replace or ignore the bits they don't. This was the original goal
   (hence Game *Bits*). However, in practice this results in an explosion of
   interfaces and abstractions which hurts both understandability and
   performance. Instead, library dependencies are managed in multiple tiers:
   Dependencies on lower level tier libraries are unrestricted, dependencies
   within a tier are minimized, and dependencies on higher level tiers are
   forbidden. Ultimately, this is a long-winded way to say it is a framework --
   but one where dependency management is a primary design consideration. See
   [Libraries](#libraries) for a break down of the libraries by tier.

*  *Google dependent:* While not really a "principle" per-se, Game Bits uses
   other Google open source libraries when they could be applied. In other
   words, if Google has an open source library for a domain useful to Game Bits,
   that will be preferred over other options. This is both to reduce cognitive
   load for myself (being a Google engineer as my day job), and to explore the
   libraries in a different context.

## Libraries

Game Bits libraries are divided in various tiers, with dependencies flowing from
the higher level tiers to the lower level tiers. Optional integrations between
tiers or with external libraries (like SDL or Vulkan) are implemented as
sub-libraries (organized as subdirectories).

*  *Tier 0:* These are foundational libraries that define universal utility
   classes, cross-cutting funtionality common to all code, third-party wrappers
   and OS-level platform abstraction.
   *  [`gb_base`](src/gb/base): Every other library depends on base (even in
      tier 0). It contains utility types and classes universal across Game Bits.
      It also includes fundamental interfaces (like `Allocator`) that are
      extended by other tier 0 libraries.
   *  [`gb_alloc`](src/gb/alloc): Custom allocator algorithms and
      implementations that can be used with any code using the `gb_base`
      allocator interface.
   *  [`gb_container`](src/gb/container): Additional game-oriented containers
      and container algorithms that go beyond basic STL-equivalent containers.
   *  [`gb_file`](src/gb/file): This is a file system abstraction that allows
      multiple custom backend implementations simultaneously without dependent
      code needing to know about it. Currently, a fully in-memory file system
      and local file system are supported. A custom IFF-style chunk file format
      is also provided.
   *  [`gb_thread`](src/gb/thread): Low level threading API providing additional
      functionality over the standard library (thread names, core pinning,
      fibers, etc.)
   *  [`gb_test`](src/gb/test): Test-only utilities to support unit testing and
      benchmarking code.
   *  [`gb_sdl`](src/gb/sdl): Extension library when using SDL.
   *  [`stb`](src/stb): Family of libraries that convert the header-only
      [STB](https://github.com/nothings/stb) libraries into actual libraries.
      Libraries are made on an as-needed basis. Currently, the following
      libraries are available: `stb_image`, `stb_image_resize`, and
      `stb_perlin`.

*  *Tier 1:* These are lower level game components that can generally be taken
   individually. Dependencies between them are minimized, but not forbidden
   (`gb_config` is built on `gb_parse`).
   *  [`gb_cli`](src/gb/cli): This library defines a program's command line as
      constant data (its commands, flags, positional arguments, and their
      help), parses it into the values given for the command it names, and
      runs that command or writes its help. Paths given on the command line
      are validated and opened in a `gb_file` file system.
   *  [`gb_collide`](src/gb/collide): This library defines collision shapes and
      the intersection tests between them, implemented in terms of libccd.
   *  [`gb_config`](src/gb/config): This library defines a general purpose
      hierarchical configuration value type, along with a reader and writer for
      a text configuration format built on `gb_parse`.
   *  [`gb_game`](src/gb/game): This library defines the game "main" and a
      hierarchical game state machine system.
   *  [`gb_image`](src/gb/image): This library defines general purpose image
      loading and processing.
   *  [`gb_job`](src/gb/job): This library defines a generic "job" system for
      running tasks asynchronously.
   *  [`gb_message`](src/gb/message): This library defines a typed message
      system that supports both publish/subscribe style messages and direct
      endpoint-to-endpoint message passing. It also supports both top-down and
      bottom-up message handling for stack based endpoints (as in a UI system or
      hierarchical state machine). Only in-process messages are currently
      supported.
   *  [`gb_parse`](src/gb/parse): This library defines general purpose text
      parsing code for use with custom asset file syntax, game scripting languages,
      or similar use cases. It supports a general-purpose-but-opinionated
      lexer and source file / error reporting helpers.
   *  [`gb_pilot`](src/gb/pilot): This library defines the interface through
      which a program is driven frame by frame (as by a script), without a
      user: the input it gets each frame, the commands and conditions the
      program names, the GUI items it can press, and the frames it saves.
      `StepPilot` drives a program through a list of steps given to it in
      code, and there is a fake for testing code that takes part.
      *  [`gb_pilot_sdl`](src/gb/pilot/sdl): Library that gives an SDL
         program the pilot's input, as SDL events and keyboard state, and
         keeps the user's real keyboard and mouse from reaching it.
   *  [`gb_profile`](src/gb/profile): A profiler cheap enough to leave on all
      the time in a realtime program. It times scopes in the program and calls
      out to other systems (each charged only for its own time), and records
      counters and values describing the workload.
   *  [`gb_resource`](src/gb/resource): This library defines a generic resource
      system for referencing, retrieving, sharing, and managing the lifecycle
      and dependencies of typed resources. This also supports optional reading
      and writing of general purpose resource files as a chunk file. It supports
      embedding dependent resources in the same file (for instance, a "level"
      resource file could contain all dependent assets directly in the same
      file).

*  *Tier 2:* These are high level libraries that combine functionality across
   Game Bits to provide more complex services commonly needed across games.
   Dependencies within this tier are still minimized, but cross dependencies are
   allowed and so are more common.
   *  [`gb_render`](src/gb/render): Graphics API independent interface to
      support basic rendering, with resource system integration. It reads
      back the frames a pilot saves.
      *  [`gb_render_vulkan`](src/gb/render/vulkan): An implementation of the
         `gb_render` system in terms of Vulkan.
         *  [`gb_render_vulkan_sdl`](src/gb/render/vulkan/sdl): Library that
            implements `gb_render_vulkan` dependencies when using SDL.
      *  [`gb_render_testing`](src/gb/render): Test-only render backend and
         `RenderTest` fixture, for tests of `gb_render` and the libraries that
         draw with it.
   *  [`gb_imgui`](src/gb/imgui): Implementation of
      [Dear ImGui](https://github.com/ocornut/imgui/wiki) that is built in
      terms of the `gb_render` library. It supports context switching, texture
      resource support, font loading, and a rendering implementation.
      *  [`gb_imgui_sdl`](src/gb/imgui/sdl): Library that implements SDL
         integration with the `gb_imgui` implementation.
   *  [`gb_pilot_script`](src/gb/pilot/script): Reads pilot scripts, text
      files of one step per line, into the steps a `StepPilot` runs, and
      creates the pilot from a program's `gb_cli` command line, saving its
      frames as PNGs.

# Getting started

While Game Bits code is designed to be cross platform, it is mainly developed
and maintained on Windows with Visual Studio 2022, building with CMake and the
Ninja generator. It also builds and passes its tests on Linux with clang, and
the RenderCheck example and the templates build there too, though BlockWorld
doesn't yet. CMake 3.15 or later is required; the copy that ships with Visual
Studio is what is normally used. On Windows, programs using Game Bits require
Windows 10 version 1903 or later, and older versions are not supported.

Game Bits works only in UTF-8 (of which ASCII is a part): every `char` string
it takes or returns, including paths and command line arguments, is UTF-8.
Every executable that uses Game Bits must be defined with its CMake commands
(see [GameBitsTargetCommands.cmake](CMake/GameBitsTargetCommands.cmake)). On
Windows, they embed a manifest that makes the process's code page UTF-8, so
`main()`'s arguments are UTF-8. This requires Windows 10 version 1903 or later.
A DLL runs with its host program's code page instead, so beyond `main()`'s
arguments, Game Bits code doesn't rely on the code page, and converts to and
from UTF-16 when it calls Windows.

## Dependencies

Game Bits stores most of its library dependencies in the third_party directory.
All of these dependencies are referenced in via git submodules, and so must be
updated after cloning, or whenever the dependent libraries are updated.

``` 
git submodule update --init --recursive
```

Alternatively, you may maintain your own third party directory by defining the
GB_THIRD_PARTY_DIR in CMake projects that use Game Bits. The caveat being that
it must be a strict superset of what Game Bits requires. See the
`CMakeLists.txt` files in the [templates](templates) directory for more on
this.

### SDL

SDL is an optional dependency of Game Bits, provided as a platform abstraction
layer implementation. There are no direct dependencies on SDL from any Game
Bits library, except those that implement required platform abstraction
interfaces in terms of SDL as a convenience.

SDL 2 is a submodule in `third_party/SDL`, built from source as a static
library for windowing and input (its audio and its own renderer are left out),
so programs need no SDL DLL beside them. On Linux, it needs the system's
windowing headers to open windows (see [CLAUDE.md](CLAUDE.md) for the packages).
CMake files that use Game Bits can link to it directly with the `sdl2` library
target, and `sdl2_main` for SDL's `main()`.

### Vulkan

If you are not using Game Bits for 3D graphics (specifically the render
library), Vulkan is not required. Otherwise, the
[Vulkan SDK](https://vulkan.lunarg.com/sdk/home) must be installed separately.
The renderer requests the Vulkan 1.2 API, and needs a device that supports it,
so any reasonably current driver will do. The SDK should be the version CI
builds with (`VULKAN_SDK_VERSION` in
[.github/workflows/cmake.yml](.github/workflows/cmake.yml)), or newer.

Game Bits finds the SDK through the `VULKAN_SDK` environment variable, and
leaves out its Vulkan libraries where it isn't set. On Windows, the SDK's
installer sets it (such as `VULKAN_SDK=C:\VulkanSDK\1.4.363.1`). On Linux,
[tools/install-vulkan-sdk.sh](tools/install-vulkan-sdk.sh) installs the parts
of the SDK Game Bits uses, and sourcing its `setup-env.sh` sets it, in each
shell (see [CLAUDE.md](CLAUDE.md)). Linux distributions package Vulkan too, but
older than Game Bits needs.

Vulkan may be linked to by depending on the `Vulkan` library target in CMake
files that use Game Bits and have Vulkan installed.

A program can compile its GLSL shaders in its build, and build them into
itself, with `gb_embed_shader` (see
[CMake/GameBitsTargetCommands.cmake](CMake/GameBitsTargetCommands.cmake)), as
RenderCheck does. It compiles them with `glslc` from the Vulkan SDK, which only
a program that calls it needs.

## Building

Game Bits can be built directly from its own CMakeLists.txt file, or more
commonly as part of another CMake driven application project. Either way, the
build uses CMake with the Ninja generator and, on Windows, the Visual Studio
2022 toolchain (both the MSVC and LLVM clang-cl compilers are supported). On
Linux it uses clang (see [CLAUDE.md](CLAUDE.md) for the commands).

If you have your own project (as is most likely), you can start from one of the
[templates](templates) sub-directories: copy it to a new location, and define
the `GB_DIR` environment variable to refer to the root `game-bits` folder. See
documentation in the template directories for more on configuring a dependent
project.

### Using Visual Studio CMake support

Visual Studio has direct support for CMake, which is the primary way Game Bits
is developed. Rather than opening a solution, choose to "open a local folder"
and point it at the repository root. It will pick up the CMake targets using
the configurations defined in [CMakeSettings.json](CMakeSettings.json):
`x64-Debug` and `x64-Release` (note that the latter is a `RelWithDebInfo`
build). When making your own project from a template, change the copy as
desired for your project.

For the best Visual Studio experience using Game Bits, it is recommended to
switch the Solution Explorer view to be the "CMake Targets View", as it will
allow you to navigate more easily to Game Bits source files.

### Building in a devcontainer

[.devcontainer](.devcontainer) defines an Ubuntu 24.04 container with
everything a Linux build needs, including the Vulkan SDK. It can be opened as a
GitHub Codespace (Code > Codespaces), or with Visual Studio Code's Dev
Containers extension on an x86_64 machine (the Vulkan SDK isn't built for ARM
Linux). The submodules are checked out when it is created, and
the Vulkan SDK's environment is set up, so the Linux commands in
[CLAUDE.md](CLAUDE.md) work as they are, without sourcing its `setup-env.sh`.
`GB_DIR` is set to the checkout, and `CC` and `CXX` to clang, so the templates,
and a project made from one that is cloned beside it in `/workspaces`, build
against it without further setup.

It also runs a light desktop, viewed in the browser. Open the forwarded
"Desktop" port (6080) from the Ports panel, choose Connect, and enter the
password `vscode`. The desktop is 1920x1080, so in a smaller browser window,
set noVNC's Scaling mode (in its settings) to Local scaling. A program run from
the container's terminal appears there, and takes the mouse and keyboard from
the browser. Build in VS Code's terminal rather than the desktop's own, which
runs programs but lacks the build environment (the desktop is started through
sudo, which clears it). It renders on the CPU, with Mesa's lavapipe driver, which suits
programs of ImGui's weight such as RenderCheck, but is far too slow for
BlockWorld.

In VS Code, the CMake Tools extension builds, runs, debugs, and tests from its
CMake panel and the status bar. It has two projects: Game Bits itself, whose
unit tests are listed in the Testing view, and RenderCheck. Choose which one is
active with the edit icon on Project Status's Folder entry, and configure it
with the icon on its Configure entry. On its first configure, choose the
[Unspecified] kit, which uses clang (the container sets `CC` and `CXX`). The
Project Outline lists each project once it is configured. Each builds in the same directories
as the commands in [CLAUDE.md](CLAUDE.md) and below. A program run or debugged
from VS Code appears on the desktop, as from the terminal. These settings are in
`devcontainer.json`, so they apply only in the container.

### Building from the command line

The same build can be driven from a shell that has an x64 MSVC developer
environment set up (via `vcvars64.bat`, or `Enter-VsDevShell` in PowerShell).
The build directories match CMakeSettings.json, so Visual Studio and the
command line can share them:

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B out/build/x64-Debug
cmake --build out/build/x64-Debug
ctest --test-dir out/build/x64-Debug --output-on-failure
```

Debug test binaries link the non-redistributable debug CRT, which is not on the
PATH by default and must be added before the tests will run. See
[CLAUDE.md](CLAUDE.md) for that and the rest of the command line build, test,
and formatting commands.

### Running the BlockWorld example

The BlockWorld example is a voxel-style example that uses most of the Game Bits
libraries and is a full example of how the different pieces can fit together.
It is a separate CMake project, which pulls in Game Bits itself, so it is built
either by opening `examples/block-world` as a local folder in Visual Studio, or
from the command line:

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S examples/block-world -B examples/block-world/out/build/x64-Debug
cmake --build examples/block-world/out/build/x64-Debug
```

The resulting executable is written to `bin/block_world.exe`. In order to run
it, you must first build the required shaders, by running the
[assets/shaders/build-shaders.bat](assets/shaders/build-shaders.bat) file
(it requires `glslc` from the Vulkan SDK to be on the PATH).

It starts at the title, or in the state `--state` names (`--state PlayState`
starts playing straight away). It also runs pilot scripts, such as
[examples/block-world/scripts/walk.txt](examples/block-world/scripts/walk.txt),
which walks forward and saves frames before and after:

```
.\tools\run-program.ps1 -Exe bin\block_world.exe -Script examples\block-world\scripts\walk.txt
```

### Running the RenderCheck example

The RenderCheck example exercises the Vulkan backend by hand: window resizing,
binding data churn, bindings left unset, texture and texture array updates
while they are drawn, and GUI meshes up to and past their vertex limit, with
memory use and any logged errors (including the Vulkan validation layer's)
shown on screen. It is built the same way as BlockWorld, and enables
validation in every build, so a release build is the simplest to run (a Debug
build also needs the debug runtime on the PATH; see [CLAUDE.md](CLAUDE.md)):

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S examples/render-check -B examples/render-check/out/build/x64-Release
cmake --build examples/render-check/out/build/x64-Release
```

The resulting executable is written to `bin/render_check.exe`. Its shaders are
compiled in the build and built into it, so it needs no assets.

On Linux, it builds with clang, in a shell with the Vulkan SDK's environment
set up (see [CLAUDE.md](CLAUDE.md)), and is written to `bin/render_check`:

```
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -S examples/render-check -B examples/render-check/out/build/linux-clang-RelWithDebInfo
cmake --build examples/render-check/out/build/linux-clang-RelWithDebInfo
```

Where there is no GPU, Mesa's lavapipe driver renders it on the CPU, and where
there is no display, it runs in a virtual X server, such as `xvfb-run -a -s
'-screen 0 1920x1080x24' bin/render_check`.

The same checks can run without a user, from scripts in
[examples/render-check/scripts](examples/render-check/scripts), one for each
part of the panel. Each presses the panel's buttons and check boxes by their
labels and saves frames before and after, and a check whose result isn't as
expected fails the run. They are run with
[tools/run-program.ps1](tools/run-program.ps1):

```
.\tools\run-program.ps1 -Exe bin\render_check.exe -Script examples\render-check\scripts\textures.txt
```

The frames are saved to `out\run\render_check\`. On Linux, they are run with
[tools/run-program.sh](tools/run-program.sh), which runs the program in a
virtual X server of its own:

```
tools/run-program.sh --exe bin/render_check --script examples/render-check/scripts/textures.txt
```

## Using Game Bits CMake commands

Game Bits has its own funny way of defining CMake targets and tests via various
`gb_*` target commands. All of the Game Bits examples and templates make use of
these to define their own targets, and you are of course welcome to for your
own projects as well. Here is a quick rundown of what they do and how to use
them.

There are four target types defined, where `Target` is the desired target
name. *Note: All Game Bits targets are prefixed with `gb_`. Your targets should
not use this prefix.*

1. `gb_add_library(Target)`: Adds a standard statically linked library of the
   given name.
2. `gb_add_shared_library(Target)`: Adds a dynamically linked library of the
   given name, which is written to the `bin/` output directory.
3. `gb_add_executable(Target)`: Adds a non-windowed executable
   target (aka a command line interface).
4. `gb_add_win_executable(Target)`: Adds a windowed executable target
   (aka there is no console component).

All of them ultimately call the CMake functions `add_library` or `add_executable`,
and then configure them according to other variables prefixed with the name of
the target. If used, these variables *must* be defined before calling
`gb_add_*`.

| Variable Name        | Description |
| -------------------- | --- |
| `VS_FOLDER`          | Set to the visual studio folder the targets in this directory and all subdirectories should be in. |
| `Target_SOURCE`      | Source files to build for the target. This should include headers and implementation files. |
| `Target_FBS`         | FlatBuffer schema files for this target. These are compiled with flatc and the corresponding generated headers are made available to this Target |
| `Target_DEPS`        | Target dependencies. Dependencies are any other CMake target defined in the project. These should always be libraries. Changing a source in a dependency results in this target rebuilding. Dependencies are always linked to the target. |
| `Target_LIBS`        | External libraries for this target. This is for external dependencies (aka Vulkan, or any other externally built library). |
| `Target_DEFINES`     | Additional preprocessor defines for this target. |
| `Target_INCLUDES`    | Additional include directories for this target. |
| `Target_TEST_SOURCE` | Source files to build unit tests. If this is defined, then an additional `Target_test` target will be built that implicitly depends on `Target` and links in Google Test libraries and corresponding `main` function. All test targets are put under a `Test` subfolder in Visual Studio. |
| `Target_TEST_FBS`    | FlatBuffer schema files to build for unit tests |
| `Target_TEST_DEPS`   | Additional unit test target dependencies |
| `Target_TEST_LIBS`   | Additional external libraries for this unit test target |

For the full details, see [CMake/GameBitsTargetCommands.cmake](CMake/GameBitsTargetCommands.cmake).
