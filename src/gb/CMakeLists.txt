## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(VS_FOLDER GameBits)

# Enable warnings as errors for the Game Bits libraries themselves
if (MSVC)
  add_compile_options(/WX)
else()
  add_compile_options(-Werror)
endif()

add_subdirectory(alloc)
add_subdirectory(base)
add_subdirectory(file)
add_subdirectory(game)
add_subdirectory(imgui)
add_subdirectory(job)
add_subdirectory(message)
add_subdirectory(render)
add_subdirectory(resource)
add_subdirectory(sdl)
add_subdirectory(test)
add_subdirectory(thread)
