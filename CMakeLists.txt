cmake_minimum_required(VERSION 3.4)

###############################################################################
## Set global configuration
###############################################################################

project(GBits)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(GBITS_DIR ${CMAKE_CURRENT_LIST_DIR})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${GBITS_DIR}/build/bin)
enable_testing()
add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

###############################################################################
## Sub Directories
###############################################################################

include(Dependencies.cmake)
include(TargetCommands.cmake)
add_subdirectory(src)
