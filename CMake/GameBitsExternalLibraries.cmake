## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

# Defines the targets for libraries that aren't built from source, where they
# are available: sdl2 and sdl2_main, and Vulkan. Modules that need one are only
# added when its target exists.
function(gb_external_libraries)
  ####### SDL2

  # Only on Windows, as third_party holds SDL2's prebuilt Windows binaries.
  if (WIN32)
    add_library(sdl2 SHARED IMPORTED)
    target_include_directories(sdl2 INTERFACE 
      "${GB_THIRD_PARTY_DIR}/SDL2-2.0.10/include"
    )
    set_target_properties(sdl2 PROPERTIES
      IMPORTED_IMPLIB
        "${GB_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2.lib"
      IMPORTED_LOCATION
        "${GB_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2.dll"
    )

    add_library(sdl2_main STATIC IMPORTED)
    set_target_properties(sdl2_main PROPERTIES
      IMPORTED_LOCATION
        "${GB_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2main.lib"
    )
  endif()

  ####### Vulkan

  # Only on Windows with the Vulkan SDK installed, as the paths follow the
  # Windows Vulkan SDK's layout.
  if(WIN32 AND DEFINED ENV{VULKAN_SDK})
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
