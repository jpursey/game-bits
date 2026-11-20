## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

# Defines the Vulkan target where the Vulkan SDK is available. Modules that
# need it are only added when the target exists.
function(gb_external_libraries)
  ####### Vulkan

  # Only from the Vulkan SDK (VULKAN_SDK is set by its installer on Windows,
  # and by its setup-env.sh on Linux), as the code needs a newer Vulkan than
  # Linux distributions package.
  if(DEFINED ENV{VULKAN_SDK})
    find_package(Vulkan REQUIRED)
    add_library(Vulkan INTERFACE IMPORTED)
    target_link_libraries(Vulkan INTERFACE Vulkan::Vulkan)
    target_compile_definitions(Vulkan INTERFACE
      VULKAN_HPP_NO_EXCEPTIONS
    )
  endif()

endfunction()
