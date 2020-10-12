## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

if(DEFINED ENV{VULKAN_SDK})
  add_subdirectory(vulkan)
endif()

set(gb_render_SOURCE
  binding.h
  binding_data.h
  draw_list.cc draw_list.h
  local_binding_data.cc local_binding_data.h
  material.cc material.h
  material_config.h
  material_type.cc material_type.h
  mesh.cc mesh.h
  mesh_view.cc mesh_view.h
  pixel_colors.h
  render_assert.cc render_assert.h
  render_backend.h
  render_buffer.cc render_buffer.h
  render_buffer_view.h
  render_pipeline.h
  render_resource_chunks.h
  render_scene.h
  render_scene_type.cc render_scene_type.h
  render_system.cc render_system.h
  render_types.h
  sampler_options.h
  shader.h
  shader_code.h
  texture.cc texture.h
  texture_view.cc texture_view.h
)

set(gb_render_TEST_SOURCE
  binding_data_test.cc
  local_binding_data_test.cc
  material_test.cc
  material_type_test.cc
  mesh_test.cc
  mesh_view_test.cc
  render_buffer_test.cc
  render_buffer_view_test.cc
  render_scene_test.cc
  render_scene_type_test.cc
  render_system_test.cc
  render_test.cc render_test.h
  shader_test.cc
  test_binding_data.cc test_binding_data.h
  test_render_backend.cc test_render_backend.h
  test_render_buffer.cc test_render_buffer.h
  test_render_pipeline.cc test_render_pipeline.h
  test_render_scene.h
  test_render_scene_type.h
  test_shader_code.cc test_shader_code.h
  test_texture.cc test_texture.h
  texture_test.cc
  texture_view_test.cc
)

set(gb_render_DEPS
  absl::flat_hash_map
  absl::inlined_vector
  absl::memory
  absl::span
  absl::strings
  gb_base
  gb_file
  gb_resource
  gb_resource_file
  stb_image
)

set(gb_render_LIBS
  glog
)

gb_add_library(gb_render)
