## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

###############################################################################
## Target generation functions
##
## For a given target name "Target", the following variables can be set
##  VS_FOLDER           Set to the visual studio folder targets should be in
##  Target_SOURCE       Source files to build for the target
##  Target_FBS          FlatBuffer schema files for this target
##  Target_DEPS         Target dependencies
##  Target_LIBS         External libraries for this target
##  Target_DEFINES      Additional preprocessor defines for this target
##  Target_INCLUDES     Additional include directories for this target
##  Target_TEST_SOURCE  Source files to build unit tests
##  Target_TEST_FBS     FlatBuffer schema files to build for unit tests
##  Target_TEST_DEPS    Additional unit test target dependencies
##  Target_TEST_LIBS    Additional external libraries for this unit test target
##
## Every executable these define, and any other given gb_set_target_properties,
## embeds the Game Bits manifest on Windows (see gb_embed_manifest).
###############################################################################

function(gb_set_target_properties NAME)
  if(DEFINED VS_FOLDER)
    set_target_properties(${NAME} PROPERTIES FOLDER ${VS_FOLDER})
  endif()
  get_target_property(_type ${NAME} TYPE)
  if (_type STREQUAL "EXECUTABLE")
    gb_embed_manifest(${NAME})
  endif()
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${NAME} PRIVATE -Wthread-safety)
  endif()
  if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # These are primarily for Abseil
    target_compile_options(${NAME} PUBLIC 
      /wd4244 /wd4267 /wd4800  # 'type1' to 'type2', possible loss of data
    )
  endif()

  target_include_directories(${NAME} PUBLIC "${GB_DIR}/src" ${GB_INCLUDE_DIRS})
  if(DEFINED ${NAME}_INCLUDES)
    target_include_directories(${NAME} PUBLIC ${${NAME}_INCLUDES})
  endif()

  if(DEFINED ${NAME}_DEFINES)
    target_compile_definitions(${NAME} PUBLIC NOMINMAX ${${NAME}_DEFINES})
  else()
    target_compile_definitions(${NAME} PUBLIC NOMINMAX)
  endif()

  if(DEFINED ${NAME}_DEPS)
    add_dependencies(${NAME} ${${NAME}_DEPS})
    target_link_libraries(${NAME} PUBLIC ${${NAME}_DEPS})
  endif()
  if(DEFINED ${NAME}_LIBS)
    target_link_libraries(${NAME} PUBLIC ${${NAME}_LIBS})
  endif()

  if(DEFINED ${NAME}_FBS)
    STRING(REGEX REPLACE "_" "/" ${NAME}_FBS_PREFIX ${NAME})
    foreach(_file IN LISTS ${NAME}_FBS)
      get_filename_component(_path ${_file} ABSOLUTE)
      list(APPEND ${NAME}_FBS_ABSOLUTE ${_path})
    endforeach()
    flatbuffers_generate_headers(
      TARGET ${NAME}_fbs_generated
      INCLUDE_PREFIX ${${NAME}_FBS_PREFIX}
      SCHEMAS ${${NAME}_FBS_ABSOLUTE})
    target_link_libraries(${NAME} PRIVATE ${NAME}_fbs_generated flatbuffers)
  endif()

  # The version of ABSL synced generates some warnings for C++17 in VS2019.
  # This disables those warnings. Since these often come up in headers,
  # visibility must be PUBLIC 
  target_compile_definitions(${NAME} PUBLIC 
    _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
  ) 
endfunction()

# On Windows, embeds the Game Bits manifest in an executable, which makes the
# process's code page UTF-8, so main()'s arguments are UTF-8.
function(gb_embed_manifest NAME)
  if (WIN32)
    target_sources(${NAME} PRIVATE "${GB_DIR}/CMake/GameBits.manifest")
  endif()
endfunction()

# Builds a file's bytes into a target, so the program has them with nothing to
# find or open at run time:
#
#   gb_embed_data(<target> <file> <header> <name>)
#
# For example, gb_embed_data(oz3_ozzy boot_rom.asm oz3/ozzy/boot_rom.h
# oz3::kBootRom) lets the target, and anything that links it, include
# "oz3/ozzy/boot_rom.h", which declares the gb::EmbeddedData oz3::kBootRom
# (see gb/base/embedded_data.h).
#  <file>    The file to embed, relative to the current source directory.
#  <header>  The include path of the generated header: relative, with no "..",
#            and ending in ".h".
#  <name>    The variable's name, qualified by its namespace if it has one.
#
# Call it in the CMakeLists.txt that defines the target. The data is rebuilt
# whenever the file changes, but the header only when <header> or <name> do, so
# code that includes it isn't recompiled when the data changes.
#
# The bytes are the file's as it is on disk, so a text file checked out with
# git's core.autocrlf has CRLF line endings on Windows and LF elsewhere. Give it
# "eol=lf" in .gitattributes where they must be the same everywhere.
function(gb_embed_data TARGET FILE HEADER NAME)
  if (NOT HEADER MATCHES "\\.h$" OR IS_ABSOLUTE "${HEADER}" OR
      HEADER MATCHES "(^|/)\\.\\.(/|$)")
    message(FATAL_ERROR
      "gb_embed_data: header \"${HEADER}\" must be a relative path ending in .h")
  endif()
  set(_identifier "[A-Za-z_][A-Za-z0-9_]*")
  if (NOT NAME MATCHES "^(${_identifier}::)*${_identifier}$")
    message(FATAL_ERROR "gb_embed_data: \"${NAME}\" is not a C++ name")
  endif()
  string(REGEX MATCH "[^:]+$" _variable "${NAME}")
  string(REGEX REPLACE "(::)?[^:]+$" "" _namespace "${NAME}")
  set(_declaration "extern const gb::EmbeddedData ${_variable};\n")
  if (NOT _namespace STREQUAL "")
    set(_declaration "namespace ${_namespace} {\n\n${_declaration}\n}  // namespace ${_namespace}\n")
  endif()

  get_filename_component(_input "${FILE}" ABSOLUTE)
  set(_root "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_embedded")
  string(REGEX REPLACE "\\.h$" ".cc" _source "${_root}/${HEADER}")
  string(MAKE_C_IDENTIFIER "${HEADER}" _guard)
  string(TOUPPER "${_guard}_" _guard)

  file(GENERATE OUTPUT "${_root}/${HEADER}" CONTENT
"// Generated by gb_embed_data from ${_input}

#ifndef ${_guard}
#define ${_guard}

#include \"gb/base/embedded_data.h\"

${_declaration}
#endif  // ${_guard}
")

  set(_script "${GB_DIR}/CMake/GameBitsEmbedData.cmake")
  add_custom_command(
    OUTPUT "${_source}"
    COMMAND "${CMAKE_COMMAND}"
      "-DINPUT=${_input}"
      "-DOUTPUT=${_source}"
      "-DHEADER=${HEADER}"
      "-DNAME=${NAME}"
      -P "${_script}"
    DEPENDS "${_input}" "${_script}"
    COMMENT "Embedding ${FILE} in ${TARGET}"
    VERBATIM)
  target_sources(${TARGET} PRIVATE "${_source}")
  target_include_directories(${TARGET} PUBLIC "${_root}")
  if (NOT "${TARGET}" STREQUAL "gb_base")
    target_link_libraries(${TARGET} PUBLIC gb_base)
  endif()
endfunction()

function(gb_add_test NAME)
  add_executable(${NAME}_test ${${NAME}_TEST_SOURCE})
  gb_embed_manifest(${NAME}_test)

  if(DEFINED ${NAME}_TEST_FBS)
    string(REGEX REPLACE "_" "/" ${NAME}_TEST_FBS_PREFIX ${NAME})
    foreach(_file IN LISTS ${NAME}_TEST_FBS)
      get_filename_component(_path ${_file} ABSOLUTE)
      list(APPEND ${NAME}_TEST_FBS_ABSOLUTE ${_path})
    endforeach()
    flatbuffers_generate_headers(
      TARGET ${NAME}_test_fbs_generated
      INCLUDE_PREFIX ${${NAME}_TEST_FBS_PREFIX}
      SCHEMAS ${${NAME}_TEST_FBS_ABSOLUTE})
    target_link_libraries(${NAME}_test PRIVATE ${NAME}_test_fbs_generated flatbuffers)
  endif()

  add_dependencies(${NAME}_test ${NAME} gmock_main)
  target_link_libraries(${NAME}_test PRIVATE
    ${NAME}
    gtest
    gmock_main
  )
  if(DEFINED ${NAME}_TEST_DEPS)
    add_dependencies(${NAME}_test ${${NAME}_TEST_DEPS})
    target_link_libraries(${NAME}_test PRIVATE
      ${${NAME}_TEST_DEPS}
    )
  endif()
  if(DEFINED ${NAME}_TEST_LIBS)
    target_link_libraries(${NAME}_test PRIVATE ${${NAME}_TEST_LIBS})
  endif()

  if(DEFINED VS_FOLDER)
    set_target_properties(${NAME}_test PROPERTIES FOLDER ${VS_FOLDER}/Tests)
  else()
    set_target_properties(${NAME}_test PROPERTIES FOLDER Tests)
  endif()

  add_test(${NAME}_test ${NAME}_test)
endfunction()

function(gb_add_library NAME)
  add_library(${NAME} ${${NAME}_SOURCE})
  gb_set_target_properties(${NAME})
  if(DEFINED ${NAME}_TEST_SOURCE)
    if (${GB_BUILD_TESTS} OR NOT (${NAME} MATCHES ^gb_.*))
      gb_add_test(${NAME})
    endif()
  endif()
endfunction()

function(gb_add_shared_library NAME)
  add_library(${NAME} SHARED ${${NAME}_SOURCE})
  gb_set_target_properties(${NAME})
  if(DEFINED ${NAME}_TEST_SOURCE)
    gb_add_test(${NAME})
  endif()
  set_target_properties(${NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GB_BIN_DIR}")
endfunction()

function(gb_add_executable NAME)
  add_executable(${NAME} ${${NAME}_SOURCE})
  gb_set_target_properties(${NAME})
  if(DEFINED ${NAME}_TEST_SOURCE)
    gb_add_test(${NAME})
  endif()
  set_target_properties(${NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GB_BIN_DIR}")
  set_property(TARGET ${NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${GB_BIN_DIR}")
endfunction()

function(gb_add_win_executable NAME)
  add_executable(${NAME} WIN32 ${${NAME}_SOURCE})
  gb_set_target_properties(${NAME})
  if(DEFINED ${NAME}_TEST_SOURCE)
    gb_add_test(${NAME})
  endif()
  set_target_properties(${NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GB_BIN_DIR}")
  set_property(TARGET ${NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${GB_BIN_DIR}")
endfunction()
