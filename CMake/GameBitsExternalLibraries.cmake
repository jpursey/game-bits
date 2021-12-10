## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

function(gb_external_libraries)
  ####### SDL2

  add_library(sdl2 SHARED IMPORTED)
  target_include_directories(sdl2 INTERFACE 
    "${GB_THIRD_PARTY_DIR}/SDL2-2.0.10/include"
  )
  set_target_properties(sdl2 PROPERTIES
    IMPORTED_IMPLIB
      "${GB_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2.lib"
  )

  add_library(sdl2_main STATIC IMPORTED)
  set_target_properties(sdl2_main PROPERTIES
    IMPORTED_LOCATION
      "${GB_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2main.lib"
  )

  ####### Vulkan

  if(DEFINED ENV{VULKAN_SDK})
    add_library(Vulkan SHARED IMPORTED)
    target_include_directories(Vulkan INTERFACE "$ENV{VULKAN_SDK}/Include")
    set_target_properties(Vulkan PROPERTIES 
      IMPORTED_IMPLIB "$ENV{VULKAN_SDK}/Lib/vulkan-1.lib"
    )
    target_compile_definitions(Vulkan INTERFACE
      VULKAN_HPP_NO_EXCEPTIONS
    )
  endif()

endfunction()
