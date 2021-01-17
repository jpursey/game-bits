cmake_minimum_required (VERSION 3.8)

################################################################################
## Set project configuration
################################################################################

project(sdl-template CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

################################################################################
# Game Bits configuration
################################################################################

# GB_DIR defines the shared directory where the game bits source code exists.
# If this is embedded in another CMake project it can be defined explicitly,
# otherwise it must be present in the environment.
if(NOT DEFINED GB_DIR)
  if(NOT DEFINED ENV{GB_DIR})
    message(FATAL_ERROR "GB_DIR environment variable not defined.")
  endif()
  set(GB_DIR "$ENV{GB_DIR}")
endif()

# Normalize backslashes in paths to be forward slash for consistency when
# building paths.
STRING(REGEX REPLACE "\\\\" "/" CURRENT_DIR ${CMAKE_CURRENT_LIST_DIR})
STRING(REGEX REPLACE "\\\\" "/" GB_DIR ${GB_DIR})

# Set your binary output directory. This should pretty much always be set, as
# otherwise binary output will go into the Game Bits directory.
set(GB_BIN_DIR "${CURRENT_DIR}/bin")

# Optionally set a different third_party source directory. This is not
# recommended, unless you want to control the version of libraries used (the
# caveat being that Game Bits is only tested with its own version). If set, it
# must be a superset of what Game Bits needs. Game Bits third_party directories
# are unmodified from their source locations (most are submodules), which makes
# this a bit more maintainable.
# set(GB_THIRD_PARTY_DIR "${GB_DIR}/third_party")

# Add the Game Bits libraries and tools. This also makes the gb_* commands
# available.
add_subdirectory("${GB_DIR}" gb)

# Set any remaining configuration needed for using gb_commands in this project.
# This is only needed if any gb_* functions are used in this project.
gb_config()

# Add the external libraries that are not built by Game Bits. This is necessary,
# as external libraries are imported and otherwise not available within this
# project. This is optional, but without this any imported libraries that
# Game Bits uses cannot be used directly by targets in this project.
gb_external_libraries()

################################################################################
## Sub Directories
################################################################################

add_subdirectory(src)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT sdl_template)
