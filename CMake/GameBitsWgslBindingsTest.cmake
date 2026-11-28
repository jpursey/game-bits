## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

###############################################################################
## Tests GameBitsWgslBindings.cmake, as ctest's gb_wgsl_bindings_test
##
## Run with cmake -P, with these set:
##  SCRIPT  GameBitsWgslBindings.cmake
##  DIR     A folder to write the test's files in
###############################################################################

file(MAKE_DIRECTORY "${DIR}")

# Renumbers `input` with the script, setting `result` to its exit code,
# `error` to what it wrote to stderr, and `output` to the WGSL it wrote.
function(run_script NAME INPUT)
  file(WRITE "${DIR}/${NAME}.in.wgsl" "${INPUT}")
  file(REMOVE "${DIR}/${NAME}.out.wgsl")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DINPUT=${DIR}/${NAME}.in.wgsl"
      "-DOUTPUT=${DIR}/${NAME}.out.wgsl" -P "${SCRIPT}"
    RESULT_VARIABLE _result
    OUTPUT_QUIET
    ERROR_VARIABLE _error)
  set(_output "")
  if (EXISTS "${DIR}/${NAME}.out.wgsl")
    file(READ "${DIR}/${NAME}.out.wgsl" _output)
  endif()
  set(result "${_result}" PARENT_SCOPE)
  set(error "${_error}" PARENT_SCOPE)
  set(output "${_output}" PARENT_SCOPE)
endfunction()

# Fails unless renumbering `input` gives `expected`.
function(expect_bindings NAME INPUT EXPECTED)
  run_script(${NAME} "${INPUT}")
  if (NOT result EQUAL 0)
    message(SEND_ERROR "${NAME}: the script failed: ${error}")
  elseif (NOT output STREQUAL "${EXPECTED}")
    message(SEND_ERROR
      "${NAME}: expected\n${EXPECTED}\nbut the script wrote\n${output}")
  endif()
endfunction()

# Fails unless renumbering `input` fails.
function(expect_failure NAME INPUT)
  run_script(${NAME} "${INPUT}")
  if (result EQUAL 0)
    message(SEND_ERROR "${NAME}: the script succeeded, but should have failed")
  endif()
endfunction()

# Each kind of binding naga writes, in several sets, as it writes them: the
# binding's attributes on one line, and its var on the next.
expect_bindings(every_kind
"struct ViewData {
    s_pixel_to_clip: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> unnamed: ViewData;
@group(1) @binding(0)
var tex_sampler: sampler;
@group(1) @binding(0)
var tex_image: texture_2d<f32>;
@group(1) @binding(1)
var tex_array_image: texture_2d_array<f32>;
@group(1) @binding(1)
var tex_array_sampler: sampler;
@group(1) @binding(2)
var volume_image: texture_3d<f32>;
@group(1) @binding(2)
var volume_sampler: sampler;
@group(1) @binding(3)
var<storage> colors: QuadColors;
@group(2) @binding(1)
var<storage, read> offsets: Offsets;
@group(2) @binding(12)
var shadow_sampler: sampler_comparison;
@group(2) @binding(12)
var shadow_image: texture_depth_2d;

fn main_1() {
    let c = textureSample(tex_image, tex_sampler, vec2(0f));
    return;
}
"
"struct ViewData {
    s_pixel_to_clip: vec4<f32>,
}

@group(0) @binding(0)
var<uniform> unnamed: ViewData;
@group(1) @binding(1)
var tex_sampler: sampler;
@group(1) @binding(0)
var tex_image: texture_2d<f32>;
@group(1) @binding(2)
var tex_array_image: texture_2d_array<f32>;
@group(1) @binding(3)
var tex_array_sampler: sampler;
@group(1) @binding(4)
var volume_image: texture_3d<f32>;
@group(1) @binding(5)
var volume_sampler: sampler;
@group(1) @binding(6)
var<storage> colors: QuadColors;
@group(2) @binding(2)
var<storage, read> offsets: Offsets;
@group(2) @binding(25)
var shadow_sampler: sampler_comparison;
@group(2) @binding(24)
var shadow_image: texture_depth_2d;

fn main_1() {
    let c = textureSample(tex_image, tex_sampler, vec2(0f));
    return;
}
")

# A binding and its var on one line is numbered too.
expect_bindings(one_line
"@group(1) @binding(3) var s: sampler;
@group(1) @binding(3) var t: texture_2d<f32>;
"
"@group(1) @binding(7) var s: sampler;
@group(1) @binding(6) var t: texture_2d<f32>;
")

# A shader without bindings is unchanged.
expect_bindings(no_bindings
  "@fragment\nfn main() {\n    return;\n}\n"
  "@fragment\nfn main() {\n    return;\n}\n")

# A binding not followed by a var can't be numbered, so the script fails
# rather than leave it as it was.
expect_failure(no_var "@group(0) @binding(0)\nfn f() {}\n")

# An array of textures, and so of samplers, fails, as WebGPU has none.
expect_failure(binding_array
"@group(1) @binding(1) 
var tex_sampler: binding_array<sampler, 4>;
@group(1) @binding(1) 
var tex_image: binding_array<texture_2d<f32>, 4>;
")
