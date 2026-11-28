## Copyright (c) 2026 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(gb_render_webgpu_SOURCE
  webgpu_backend.cc webgpu_backend.h
  webgpu_format.cc webgpu_format.h
  webgpu_render_buffer.cc webgpu_render_buffer.h
  webgpu_texels.cc webgpu_texels.h
  webgpu_texture.cc webgpu_texture.h
  webgpu_texture_array.cc webgpu_texture_array.h
  webgpu_volume_texture.cc webgpu_volume_texture.h
)

set(gb_render_webgpu_TEST_SOURCE
  webgpu_format_test.cc
)

set(gb_render_webgpu_DEPS
  absl::flat_hash_set
  absl::log
  absl::memory
  absl::span
  absl::synchronization
  gb_base
  gb_image
  gb_render
)

gb_add_library(gb_render_webgpu)

# Dawn's WebGPU bindings for Emscripten, which Emscripten downloads into its
# cache the first time. Whatever links the backend gets them too.
target_compile_options(gb_render_webgpu PUBLIC --use-port=emdawnwebgpu)
target_link_options(gb_render_webgpu PUBLIC --use-port=emdawnwebgpu)
