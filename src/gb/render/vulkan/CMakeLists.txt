## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

add_subdirectory(sdl)

set(gb_render_vulkan_SOURCE
  ${GB_THIRD_PARTY_DIR}/VulkanMemoryAllocator/include/vk_mem_alloc.h
  vulkan_allocator.cc vulkan_allocator.h
  vulkan_backend.cc vulkan_backend.h
  vulkan_binding_data.cc vulkan_binding_data.h
  vulkan_binding_data_factory.cc vulkan_binding_data_factory.h
  vulkan_binding_layout.cc vulkan_binding_layout.h
  vulkan_buffer.cc vulkan_buffer.h
  vulkan_descriptor_pool.cc vulkan_descriptor_pool.h
  vulkan_format.h
  vulkan_garbage_collector.cc vulkan_garbage_collector.h
  vulkan_image.cc vulkan_image.h
  vulkan_mesh_pool_buffers.cc vulkan_mesh_pool_buffers.h
  vulkan_pass.cc vulkan_pass.h
  vulkan_render_buffer.cc vulkan_render_buffer.h
  vulkan_render_pipeline.cc vulkan_render_pipeline.h
  vulkan_render_state.h
  vulkan_render_target.cc vulkan_render_target.h
  vulkan_shader_code.cc vulkan_shader_code.h
  vulkan_texels.cc vulkan_texels.h
  vulkan_texture.cc vulkan_texture.h
  vulkan_texture_array.cc vulkan_texture_array.h
  vulkan_types.h
  vulkan_view.cc vulkan_view.h
  vulkan_volume_texture.cc vulkan_volume_texture.h
  vulkan_window.h
)

set(gb_render_vulkan_INCLUDES
  ${GB_THIRD_PARTY_DIR}/concurrentqueue
  ${GB_THIRD_PARTY_DIR}/VulkanMemoryAllocator/include
)

set(gb_render_vulkan_DEFINES
  VULKAN_HPP_ASSERT=RENDER_ASSERT
)

set(gb_render_vulkan_DEPS
  absl::any_invocable
  absl::memory
  absl::time
  gb_base
  gb_render
)

set(gb_render_vulkan_LIBS
  absl::check
  absl::log
  Vulkan
)

gb_add_library(gb_render_vulkan)
