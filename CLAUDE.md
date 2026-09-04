# Game Bits

This is a shared C++ library defining common functionality used for desktop or command line applications. It is designed as a cross-platform library, with platform-specific details separated from public interfaces and headers.

This library is almost entirely self contained with all dependencies being brought in via submodules, with the exception being a dependency on the Vulkan SDK for graphics which must be installed on the machine. It currently only works on Windows, and is built with Visual Studio 2022 Community and CMake.

It is used by other projects via direct inclusion based on the "GB_DIR" environment variable being set to this directory, and Game Bits specific CMake commands (see CMake/GameBitsTargetCommands.cmake for details)

See README.md for full context.

## Directory Structure

This is a CMake project, starting at the root. The directory structure is as follows:
```
  src/          -- All source code for the library itself is under here
    gb/         -- All Game Bits original source code is in here
    stb/        -- Implementation files for the header-only STB library
  third_party/  -- Third-party code used by GameBits. These are either manually 
                   copied or unaltered git submodules to the external repository
                   (forks are avoided)
  examples/     -- Full runnable sample projects
  templates/    -- Starter projects that can be copied to start something new
  bin/          -- Compiled binary files used by compilation or execution
  out/          -- Generated output from building locally. This is transient
                   and can get deleted at any time.
  assets/       -- Assets used at runtime by examples
  CMake/        -- Custom CMake rules used by all Game Bits modules in src/
```

## Commands

- Build: TODO: Some CMake command most likely?
- Test: TODO: Some CMake command most likely?
- Format: TODO: clang-format using src/.clang-format

## Conventions

- Coding guidelines: Generally follows the Google C++ style guide (https://google.github.io/styleguide/cppguide.html).
- Formatting strictly driven by clang-format in Google style via src/.clang-format
- All Game Bits code is in the "gb" namespace.
- Sections in a file are separated by //===== blocks (extending to column 80) surrounding descriptive text: One line section description, and if necessary further description in additional paragraphs.
- Sections within a class or between groups of related functions are separated by //---- blocks (otherwise the same as above).
- All comments are // style (not /// or /*...*/)

## Don't
- Don't add new dependencies without asking.
- Don't add or modify code outside src/gb/ without asking.
- 