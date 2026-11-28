## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

###############################################################################
## Numbers the bindings of a shader's WGSL for gb_embed_shader in a web build
## (see GameBitsTargetCommands.cmake)
##
## The WGSL is what naga writes, without validation, from SPIR-V whose combined
## image samplers spirv-opt split, so a texture and its sampler still share
## GLSL's binding number. Each binding n becomes 2n + 1 for a sampler, and 2n
## for anything else (a texture, constants, or a storage buffer), so a binding's
## number depends only on its own GLSL number, as the WebGPU backend expects.
##
## Run with cmake -P, with these set:
##  INPUT   The WGSL naga wrote
##  OUTPUT  The WGSL to write
###############################################################################

file(READ "${INPUT}" _text)

# naga writes each binding as "@group(g) @binding(n) " followed by a line
# "var<...> name: type". The type's first word says whether it is a sampler.
set(_binding
  "@binding\\(([0-9]+)\\)[ \t\r\n]*var(<[^>]*>)?[ \t]+[A-Za-z0-9_]+[ \t]*:[ \t]*([A-Za-z0-9_]+)")

set(_output "")
set(_count 0)
while (1)
  string(REGEX MATCH "${_binding}" _match "${_text}")
  if (_match STREQUAL "")
    break()
  endif()
  set(_number "${CMAKE_MATCH_1}")
  set(_type "${CMAKE_MATCH_3}")
  if (_type STREQUAL "sampler" OR _type STREQUAL "sampler_comparison")
    math(EXPR _number "2 * ${_number} + 1")
  else()
    math(EXPR _number "2 * ${_number}")
  endif()
  string(REGEX REPLACE "^@binding\\([0-9]+\\)" "@binding(${_number})"
    _replacement "${_match}")

  string(FIND "${_text}" "${_match}" _at)
  string(LENGTH "${_match}" _length)
  string(SUBSTRING "${_text}" 0 ${_at} _before)
  math(EXPR _after "${_at} + ${_length}")
  string(SUBSTRING "${_text}" ${_after} -1 _text)
  string(APPEND _output "${_before}${_replacement}")
  math(EXPR _count "${_count} + 1")
endwhile()
string(APPEND _output "${_text}")

# Every binding must have been renumbered, or the shader would disagree with
# the backend's layout.
string(REGEX MATCHALL "@binding\\(" _all "${_output}")
list(LENGTH _all _total)
if (NOT _total EQUAL _count)
  message(FATAL_ERROR
    "${INPUT}: ${_total} bindings, but only ${_count} are followed by a var "
    "with a type, so the rest can't be numbered")
endif()

file(WRITE "${OUTPUT}" "${_output}")
