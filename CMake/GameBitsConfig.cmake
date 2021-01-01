## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

function(gb_config)
  # These values configure where Game Bits parts are located or should be put.
  # They can be defined by a game project to be wherever is convenient for
  # building into their cmake projects.

  # Directory to the root of the game-bits repository. If not defined, it is
  # assumed this is the game-bits repository or a copy/clone of it.
  if (NOT DEFINED GB_DIR)
    # Define a local variable, and ensure it has only forward slash directory
    # separators for safe string composition.
    set(GB_DIR "${CMAKE_CURRENT_LIST_DIR}")
    STRING(REGEX REPLACE "\\\\" "/" GB_DIR ${GB_DIR})

    # Export GB_DIR to the caller (directory or function).
    set(GB_DIR "${GB_DIR}" PARENT_SCOPE)
  endif()

  # Directory to the third party libraries Game Bits depends on. By default,
  # this points to Game Bits' own third_party directory, but it can be
  # redirected to any third party directory that has the required libraries.
  if (NOT DEFINED GB_THIRD_PARTY_DIR)
    set(GB_THIRD_PARTY_DIR "${GB_DIR}/third_party" PARENT_SCOPE)
  endif()

  # Directory where all third party library build output is generated.
  # By default, it is expected that third party libraries are built as part of
  # a separate Game Bits installation. However, if Game Bits is being used as a
  # submodule or the third party directory is overridden, this may be
  # overridden.
  if (NOT DEFINED GB_THIRD_PARTY_BUILD_DIR)
    set(GB_THIRD_PARTY_BUILD_DIR "${GB_DIR}/build/third_party" PARENT_SCOPE)
  endif()

  # Directory to where all final executable output will go.
  if (NOT DEFINED GB_BIN_DIR)
    set(GB_BIN_DIR "${GB_DIR}/bin" PARENT_SCOPE)
  endif()
endfunction()