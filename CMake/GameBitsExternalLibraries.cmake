## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

# Defines the imported Vulkan target where the Vulkan SDK is available. Modules
# that need it are only added when the target exists.
function(gb_external_libraries)
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
