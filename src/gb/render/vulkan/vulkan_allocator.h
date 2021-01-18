// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_VULKAN_VULKAN_ALLOCATOR_H_
#define GB_RENDER_VULKAN_VULKAN_ALLOCATOR_H_

#include "gb/render/vulkan/vulkan_types.h"

// MUST be after vulkan_types.h
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnullability-completeness"
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "vk_mem_alloc.h"

#ifdef __clang__
#pragma GCC diagnostic pop
#endif

#endif  // GB_RENDER_VULKAN_VULKAN_ALLOCATOR_H_
