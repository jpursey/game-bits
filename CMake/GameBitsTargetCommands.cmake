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
## embeds the Game Bits manifest on Windows (see gb_embed_manifest). In a web
## build, a windowed one (gb_add_win_executable) is a page for a browser (see
## gb_set_web_page_properties), and any other is a program for Node (see
## gb_set_web_program_properties).
###############################################################################

# -Wcharacter-conversion (between char8_t, char16_t, and char32_t) is new in
# clang 21, so gb_set_target_properties only turns it off where clang has it.
# The result is cached, so it is visible to projects that add Game Bits too.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-Wcharacter-conversion GB_HAS_CHARACTER_CONVERSION)
endif()

function(gb_set_target_properties NAME)
  if(DEFINED VS_FOLDER)
    set_target_properties(${NAME} PROPERTIES FOLDER ${VS_FOLDER})
  endif()
  get_target_property(_type ${NAME} TYPE)
  if (_type STREQUAL "EXECUTABLE")
    gb_embed_manifest(${NAME})
    get_target_property(_windowed ${NAME} WIN32_EXECUTABLE)
    if (_windowed)
      gb_set_web_page_properties(${NAME})
    else()
      gb_set_web_program_properties(${NAME})
    endif()
  endif()
  # Implicit conversion warnings are off for every target that uses Game Bits,
  # primarily for Abseil and other third-party headers. Clang's are the
  # counterparts to MSVC's, for clang and clang-cl. Most are off by default, but
  # are listed so that a newer clang turning one on doesn't fail the build in a
  # third-party header.
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${NAME} PRIVATE -Wthread-safety)
    target_compile_options(${NAME} PUBLIC
      -Wno-shorten-64-to-32
      -Wno-implicit-int-conversion
      -Wno-implicit-float-conversion
      -Wno-float-conversion
    )
    if (GB_HAS_CHARACTER_CONVERSION)
      target_compile_options(${NAME} PUBLIC -Wno-character-conversion)
    endif()
  endif()

  # Abseil's headers use Emscripten's old version macros, which Emscripten 7
  # deprecates. With threads, whatever uses Game Bits is built with -pthread,
  # as Game Bits is.
  if (EMSCRIPTEN)
    target_compile_options(${NAME} PUBLIC -Wno-deprecated-pragma)
    if (GB_WEB_THREADS)
      target_compile_options(${NAME} PUBLIC -pthread)
      target_link_options(${NAME} PUBLIC -pthread)
    endif()
  endif()

  if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${NAME} PUBLIC
      /wd4244 /wd4267 /wd4800  # 'type1' to 'type2', possible loss of data
    )
  endif()

  # CMake 3.15 leaves MSVC's warning level out of its default flags (policy
  # CMP0092), which would leave targets at MSVC's /W1.
  if (MSVC)
    target_compile_options(${NAME} PRIVATE /W3)
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

# In a web build, gives an executable what every web executable has, whether a
# program for Node or a page: it exits with main()'s result, its memory grows as
# it needs, and its stacks are 1 MiB.
#
# Emscripten's stacks are 64 KiB by default, for main() and each thread. They
# are 1 MiB here, Windows' default and the smaller of the other platforms
# Game Bits builds on, so code that fits there fits here. Each thread gets the
# same as main(), as std::thread takes no size. Wasm keeps most locals on a
# stack of its own, so this one holds less than a native stack does.
#
# Emscripten warns that with threads, memory that grows slows JavaScript's
# access to it. Programs here spend their time in wasm, which it doesn't slow.
function(gb_set_web_executable_properties NAME)
  if (EMSCRIPTEN)
    target_link_options(${NAME} PRIVATE
      -sEXIT_RUNTIME=1
      -sALLOW_MEMORY_GROWTH=1
      -sSTACK_SIZE=1MB
    )
    if (GB_WEB_THREADS)
      target_link_options(${NAME} PRIVATE -Wno-pthreads-mem-growth)
    endif()
  endif()
endfunction()

# In a web build, makes an executable a program for Node (node <name>.js), as a
# command line program is on any other platform: it uses the machine's own
# files and environment (see also gb_set_web_executable_properties). With
# threads, main() runs on a thread of its own, so it can start threads and wait
# for them.
function(gb_set_web_program_properties NAME)
  if (EMSCRIPTEN)
    gb_set_web_executable_properties(${NAME})
    target_link_options(${NAME} PRIVATE -sNODERAWFS=1)
    if (GB_WEB_THREADS)
      target_link_options(${NAME} PRIVATE -sPROXY_TO_PTHREAD)
    endif()
  endif()
endfunction()

# In a web build, makes a windowed executable a page for a browser:
# <name>.html, beside its <name>.js and <name>.wasm, from Game Bits' shell
# (CMake/GameBitsShell.html), which says what the page takes. Its file system
# is in memory (see also gb_set_web_executable_properties). It is linked with
# JSPI, so main() can wait for the browser (see gb/base/frame_wait.h), which a
# program for Node can't, as Node needs a flag to run JSPI. FS is exported, so
# a tool can read files back after main() returns.
#
# With threads, main() stays on the browser's main thread, which SDL's window
# needs.
function(gb_set_web_page_properties NAME)
  if (EMSCRIPTEN)
    gb_set_web_executable_properties(${NAME})
    set(_shell "${GB_DIR}/CMake/GameBitsShell.html")
    set_target_properties(${NAME} PROPERTIES SUFFIX ".html")
    set_property(TARGET ${NAME} APPEND PROPERTY LINK_DEPENDS "${_shell}")
    target_link_options(${NAME} PRIVATE
      "SHELL:--shell-file \"${_shell}\""
      -sJSPI
      -sENVIRONMENT=web
      -sEXPORTED_RUNTIME_METHODS=FS
    )
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

# Compiles a GLSL shader to SPIR-V and builds it into a target, as
# gb_embed_data does with a file:
#
#   gb_embed_shader(<target> <file> <header> <name>)
#
# For example, gb_embed_shader(render_check shaders/quad.vert
# shaders/quad_vert.h kQuadVertexShader) lets the target include
# "shaders/quad_vert.h", which declares the gb::EmbeddedData kQuadVertexShader,
# whose bytes are the SPIR-V to pass to RenderSystem::CreateShaderCode().
#  <file>    The shader, relative to the current source directory. Its extension
#            (.vert, .frag, .comp, ...) gives its stage.
#  <header>  As for gb_embed_data.
#  <name>    As for gb_embed_data.
#
# The shader is compiled again whenever it or a file it includes changes, and
# the build fails on any error or warning in it. It is compiled with glslc,
# found in the Vulkan SDK or on the PATH, which only a project that calls this
# needs.
function(gb_embed_shader TARGET FILE HEADER NAME)
  # FindVulkan has usually found it already. The hint is for CMake before 3.19,
  # whose FindVulkan doesn't look for it.
  find_program(Vulkan_GLSLC_EXECUTABLE glslc HINTS "$ENV{VULKAN_SDK}/bin")
  if (NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR
      "gb_embed_shader: glslc was not found in the Vulkan SDK or on the PATH")
  endif()

  get_filename_component(_input "${FILE}" ABSOLUTE)
  string(REGEX REPLACE "\\.h$" ".spv" _output
    "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_shaders/${HEADER}")
  add_custom_command(
    OUTPUT "${_output}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -Werror -MD -MF "${_output}.d"
      -o "${_output}" "${_input}"
    DEPENDS "${_input}"
    DEPFILE "${_output}.d"
    COMMENT "Compiling ${FILE} for ${TARGET}"
    VERBATIM)
  gb_embed_data(${TARGET} "${_output}" "${HEADER}" "${NAME}")
endfunction()

function(gb_add_test NAME)
  add_executable(${NAME}_test ${${NAME}_TEST_SOURCE})
  gb_embed_manifest(${NAME}_test)
  gb_set_web_program_properties(${NAME}_test)

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

  # Naming the target, rather than a file, runs the test through the
  # toolchain's emulator (Node, in a web build) when cross-compiling.
  add_test(NAME ${NAME}_test COMMAND ${NAME}_test)

  # SDL's web video driver needs a browser, so a test that resizes a window
  # fails under Node. SDL's dummy video driver serves it instead.
  if (EMSCRIPTEN)
    set_property(TEST ${NAME}_test APPEND PROPERTY ENVIRONMENT SDL_VIDEODRIVER=dummy)
  endif()
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
