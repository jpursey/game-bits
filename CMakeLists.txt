cmake_minimum_required(VERSION 3.8)

###############################################################################
## Set project configuration
###############################################################################

project(gbits CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

###############################################################################
# GBits configuration
###############################################################################

include(CMake/GBitsConfig.cmake)
gbits_config()

###############################################################################
# GBits initialization
###############################################################################

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${GBITS_BIN_DIR}")
enable_testing()
add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

list(APPEND CMAKE_MODULE_PATH
  "${GBITS_DIR}/CMake"
)
include(GBitsTargetCommands)
include(GBitsExternalLibraries)
gbits_external_libraries()

####### absl

# Disable Testing for abseil
set(BUILD_TESTING OFF) 
add_subdirectory("${GBITS_THIRD_PARTY_DIR}/abseil-cpp")
set(BUILD_TESTING ON) 

###############################################################################
## Add gbits source
###############################################################################

add_subdirectory(src)
