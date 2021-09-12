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

*  *Platform independent:* While Game Bits currently only supports Windows,
   everything is built with cross-platform support in mind. All dependendencies
   on libraries other than the standard C/C++ library are relegated to platform
   specific implementations of a generic interface.

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

*  *Tier 1:* These are lower level game components that can be taken
   individually, and do not depend on each other (except potentially via
   sub-library extension).
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
      support basic rendering, with resource system integration.
      *  [`gb_render_vulkan`](src/gb/render/vulkan): An implementation of the
         `gb_render` system in terms of Vulkan.
         *  [`gb_render_vulkan_sdl`](src/gb/render/vulkan/sdl): Library that
            implements `gb_render_vulkan` dependencies when using SDL.
   *  [`gb_imgui`](src/gb/imgui): Implementation of
      [Dear ImGui](https://github.com/ocornut/imgui/wiki) that is built in
      terms of the `gb_render` library. It supports context switching, texture
      resource support, font loading, and a rendering implementation.
      *  [`gb_imgui_sdl`](src/gb/imgui/sdl): Library that implements SDL
         integration with the `gb_imgui` implementation.

# Getting started

While Game Bits code is designed to be cross platform, it is currently only
developed and maintained on Windows with Visual Studio 2019 using CMake 3.14.0
(although many earlier versions of CMake are likely to work).

## Dependencies

Game Bits stores most of its library dependencies in the third_party directory.
All of these dependencies except SDL are referenced in via git submodules, and
so must be updated after cloning, or whenever the dependent libraries are
updated.

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

The Windows build of SDL 2.10.0 and SDL2_image 2.0.5 are included in
`third_party/` and `bin/` folders as a convenience. They can be linked to
directly via the `sdl` and `sdl2_image` library targets in CMake files that use
Game Bits.

### Vulkan

If you are not using Game Bits for 3D graphics (specifically the render
library), Vulkan is not required. However, for 3D graphics Game Bits uses
Vulkan 1.2. This must be installed separately from the
[Vulkan SDK](https://vulkan.lunarg.com/sdk/home). Installing the Vulkan SDK
should set the `VULKAN_SDK` environment variable, which Game Bits depends on
to locate the installed version. For example, at the time of this writing,
Game Bits was built with Vulkan 1.2.162.0, where VULKAN_SDK is defined as
`VULKAN_SDK=C:\VulkanSDK\1.2.162.0`

Vulkan may be linked to by depending on the `Vulkan` library target in CMake
files that use Game Bits and have Vulkan installed.

## Building

Game Bits can be built directly from its own CMakeLists.txt file, or more
commonly as part of another CMake driven application project. In either case,
Game Bits supports Visual Studio 2019 in two ways: 

1. Generating a Visual Studio project via CMake. This provides an easily
   browsable solution and project files using standard Visual Studio
   integration.
2. Using CMake support directly built into Visual Studio. This supports both
   Visual Studio and LLVM clang compilers.

### Generating a Visual Studio project from CMake

A project can be generated is done by running the
[build_vs2019.bat](build_vs2019.bat). This builds Visual Studio projects for
both the Game Bits libaries, and the BlockWorld example. The solution files
are located under a `build/` subdirectory, which is created if it does not
already exist. Specifically, `build/game-bits.sln` and
`build/examples/block-world/BlockWorld.sln`.

If you have your own project (as is most likely), you can also follow this
pattern by copying one of the [templates](templates) sub-directories to a new
location, defining the `GB_DIR` environment variable to refer to the root
`game-bits` folder, and then run that version of `build_vs2019.bat`. See
documentation in the template directories for more on configuring a dependent
project.

### Using Visual Studio CMake support directly

Alternatively, Visual Studio now has direct support for CMake, which provides
more direct control over configurations and removes the need to generate
solutions. It supports multiple toolchains (Game Bits currently supports both
the Visual Studio 2019 and LLVM clang 11 toolchains).

To build this way, simply choose to "open a local folder" instead of a solution.
It will pick up the CMake targets using the configurations defined in
[CMakeSettings.json](CMakeSettings.json). When making your own project from a
template, change the copy as desired for your project.

For the best Visual Studio experience using Game Bits, it is recommended to
switch the Solution Explorer view to be the "CMake Targets View", as it will
allow you to navigate more easily to Game Bits source files.

### Running the BlockWorld example

The BlockWorld example is a voxel-style example that uses most of the Game Bits
libraries and is a full example of how the different pieces can fit together.
However, in order to run the example, you must first build the required
shaders, by running the
[assets/shaders/build-shaders.bat](assets/shaders/build-shaders.bat) file
(it must be run from the `assets/shaders` directory).

## Using Game Bits CMake commands

Game Bits has its own funny way of defining CMake targets and tests via various
`gb_*` target commands. All of the Game Bits examples and templates make use of
these to define their own targets, and you are of course welcome to for your
own projects as well. Here is a quick rundown of what they do and how to use
them.

There are three target types defined, where `Target` is the desired target
name. *Note: All Game Bits targets are prefixed with `gb_`. Your targets should
not use this prefix.*

1. `gb_add_library(Target)`: Adds a standard statically linked library of the
   given name.
2. `gb_add_executable(Target)`: Adds a non-windowed executable
   target (aka a command line interface).
3. `gb_add_win_executable(Target)`: Adds a windowed executable target
   (aka there is no console component).

All three ultimately call the CMake functions `add_library` or `add_executable`,
and then configure them according to other variables prefixed with the name of
the target. If used, these variables *must* be defined before calling
`gb_add_*`.

| Variable Name        | Description |
| -------------------- | --- |
| `VS_FOLDER`          | Set to the visual studio folder the targets in this directory and all subdirectories should be in. |
| `Target_SOURCE`      | Source files to build for the target. This should include headers and implementation files. |
| `Target_FBS`         | FlatBuffer schema files for this target. These are compiled with flatc and the corresponding generated headers are made available to this Target |
| `Target_DEPS`        | Target dependencies. Dependencies are any other CMake target defined in the project. These should always be libraries. Changing a source in a dependency results in this target rebuilding. Dependencies are always linked to the target. |
| `Target_LIBS`        | External libraries for this target. This is for external dependencies (aka Vulkan, SDL, or any other externally built library). |
| `Target_DEFINES`     | Additional preprocessor defines for this target. |
| `Target_INCLUDES`    | Additional include directories for this target. |
| `Target_TEST_SOURCE` | Source files to build unit tests. If this is defined, then an additional `Target_test` target will be built that implicitly depends on `Target` and links in Google Test libraries and corresponding `main` function. All test targets are put under a `Test` subfolder in Visual Studio. |
| `Target_TEST_FBS`    | FlatBuffer schema files to build for unit tests |
| `Target_TEST_DEPS`   | Additional unit test target dependencies |
| `Target_TEST_LIBS`   | Additional external libraries for this unit test target |

For the full details, see [CMake/GameBitsTargetCommands.cmake](CMake/GameBitsTargetCommands.cmake).
