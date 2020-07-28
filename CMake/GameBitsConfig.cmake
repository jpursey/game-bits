function(gb_config)
  # These values configure where Game Bits parts are located or should be put.
  # They can be defined by a game project to be wherever is convenient for
  # building into their cmake projects.

  # Directory to the root of the game-bits repository. If not defined, it is
  # assumed this is the game-bits repository or a copy/clone of it.
  if (NOT DEFINED GB_DIR)
    set(GB_DIR "${CMAKE_CURRENT_LIST_DIR}")
    set(GB_DIR "${CMAKE_CURRENT_LIST_DIR}" PARENT_SCOPE)
  endif()

  # Directory to the third party libraries Game Bits depends on. By default,
  # this points to Game Bits' own third_party directory, but it can be
  # redirected to any third party directory that has the required libraries.
  if (NOT DEFINED GB_THIRD_PARTY_DIR)
    set(GB_THIRD_PARTY_DIR "${GB_DIR}/third_party" PARENT_SCOPE)
  endif()

  # Directory to where all build output should go. This is where all cmake
  # output is put for Game Bits, and code that uses the the Game Bits target
  # utility functions.
  if (NOT DEFINED GB_BUILD_DIR)
    set(GB_BUILD_DIR "${GB_DIR}/build" PARENT_SCOPE)
  endif()

  # Directory to where all final executable output will go.
  if (NOT DEFINED GB_BIN_DIR)
    set(GB_BIN_DIR "${GB_DIR}/bin" PARENT_SCOPE)
  endif()
endfunction()