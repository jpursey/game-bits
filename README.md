# Game Bits

This is a collection of libraries used to make games... or really anything else. This is an unapologetically opinionated "framework", which one day hopes to grow up into a full featured Game Engine geared at hobbyist game programmers.

# Getting started

While Game Bits code is designed to be cross platform, it is currently only developed and maintained on Windows with Visual Studio 2019 using CMake 3.14.0 (although many earlier versions of CMake are likely to work). The instructions here are for

## Dependencies

Game Bits stores most of its library dependencies in the third_party directory. All of these dependencies except SDL are referenced in via git submodules, and so must be updated after cloning, or whenever the dependent libraries are updated.

```
git submodule update --init --recursive
```

Alternatively, you may maintain your own third party directory by defining the GB_THIRD_PARTY_DIR in CMake projects that use Game Bits. The caveat being, that it must be a strict superset of what Game Bits requires. See the `templates/` directory for examples.

### SDL

SDL is an optional dependency of Game Bits, provided as a platform abstraction layer implementation. There are no direct dependencies on SDL from any Game Bits library, except those that implement required platform abstraction interfaces in terms of SDL as a convenience.

The Windows build of SDL 2.10.0 and SDL2_image 2.0.5 are included in `third_party/` and `bin/` folders as a convenience. They can be linked to directly via the `sdl` and `sdl2_image` library targets in CMake files that use Game Bits.

### Vulkan

If you are not using Game Bits for 3D graphics (specifically the render library), Vulkan is not required. However, for 3D graphics Game Bits uses Vulkan 1.2. This must be installed separately from the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home). Installing the Vulkan SDK should set the `VULKAN_SDK` environment variable, which Game Bits depends on to locate the installed version. For example, at the time of this writing, Game Bits was built with Vulkan 1.2.162.0, where VULKAN_SDK is defined as `VULKAN_SDK=C:\VulkanSDK\1.2.162.0`

Vulkan may be linked to by depending on the `Vulkan` library target in CMake files that use Game Bits and have Vulkan installed.

## Building

Game Bits can be built directly from its own CMakeLists.txt file, or more commonly as part of another CMake driven application project. In either case, Game Bits supports Visual Studio 2019 in two ways: 

1. Generating a Visual Studio project via CMake. This provides an easily browsable solution and project files using standard Visual Studio integration.
2. Using CMake support directly built into Visual Studio. This supports both Visual Studio and LLVM clang compilers.

### Generating a Visual Studio project from CMake

A project can be generated is done by running the [build_vs2019.bat](build_vs2019.bat). This builds Visual Studio projects for both the Game Bits libaries, and the BlockWorld example. The solution files are located under a `build/` subdirectory, which is created if it does not already exist. Specifically, `build/game-bits.sln` and `build/examples/block-world/BlockWorld.sln`.

If you have your own project (as is most likely), you can also follow this pattern by copying one of the `templates/` sub-directories to a new location, defining the `GB_DIR` environment variable to refer to the root `game-bits` folder, and then run that version of `build_vs2019.bat`. See documentation in the template directories for more on configuring a dependent project.

### Using Visual Studio CMake support directly

Alternatively, Visual Studio now has direct support for CMake, which provides more direct control over configurations and removes the need to generate solutions. It supports multiple toolchains (Game Bits currently supports both the Visual Studio 2019 and LLVM clang 11 toolchains).

To build this way, simply choose to "open a local folder" instead of a solution. It will pick up the CMake targets using the configurations defined in CMakeSettings.json. When making your own project from a template, change this as desired for your project.

For the best Visual Studio experience using Game Bits, it is recommended to switch the Solution Explorer view to be the "CMake Targets View". 

### Running the BlockWorld example

The BlockWorld example is a voxel-style example that uses most of the Game Bits libraries and is a full example of how the different pieces can fit together. However, in order to run the example, you must first build the required shaders, by running the [assets/shaders/build-shaders.bat](assets/shaders/build-shaders.bat) file (it must be run from the `assets/shaders` directory).

## Using Game Bits CMake commands

Game Bits has its own funny way of defining CMake targets and tests via various gb_* target commands. All of the Game Bits examples and templates make use of these to define their own targets, and you are of course welcome to for your own projects as well. Here is a quick rundown of what they do and how to use them.

There are three target types defined, where `Target` is the desired target name. *Note: All Game Bits targets are prefixed with `gb_`. Your targets should not use this prefix.*

1. `gb_add_library(Target)`: Adds a standard statically linked library of the given name.
2. `gb_add_executable(Target)`: Adds a non-windowed executable target (aka a command line interface).
3. `gb_add_win_executable(Target)`: Adds a windowed executable target (aka there is no console component).

All three ultimately call the CMake functions `add_library` or `add_executable`, and then configure them according to other variables prefixed with the name of the target. If used, these variables *must* be defined before calling `gb_add_*`.

| Variable Name        | Description |
| -------------------- | --- |
| `VS_FOLDER`          | Set to the visual studio folder the targets in this directory and all subdirectories should be in. |
| `Target_SOURCE`      | Source files to build for the target. This should include headers and implementation files. |
| `Target_FBS`         | FlatBuffer schema files for this target. These are compiled with flatc and the corresponding generated headers are made available to this Target |
| `Target_DEPS`        | Target dependencies. Dependencies are any other CMake target defined in the project. These should always be libraries. Changing a source in a dependency results in this target rebuilding. Dependencies are always linked to the target. |
| `Target_LIBS`        | External libraries for this target. This is for external dependencies (aka Vulkan, SDL, or any other externally built library). |
| `Target_TEST_SOURCE` | Source files to build unit tests. If this is defined, then an additional `Target_test` target will be built that implicitly depends on `Target` and links in Google Test libraries and corresponding `main` function. All test targets are put under a `Test` subfolder in Visual Studio. |
| `Target_TEST_FBS`    | FlatBuffer schema files to build for unit tests |
| `Target_TEST_DEPS`   | Additional unit test target dependencies |
| `Target_TEST_LIBS`   | Additional external libraries for this unit test target |

For the full details, see [CMake/GameBitsTargetCommands.cmake](CMake/GameBitsTargetCommands.cmake).