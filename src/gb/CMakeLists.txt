## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

set(VS_FOLDER GameBits)

# Extra warnings and errors for the Game Bits libraries themselves
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Werror)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  add_compile_options(/WX /Zc:__cplusplus)
endif()

add_subdirectory(alloc)
add_subdirectory(base)
add_subdirectory(container)
add_subdirectory(file)
add_subdirectory(game)
add_subdirectory(image)
add_subdirectory(imgui)
add_subdirectory(job)
add_subdirectory(message)
add_subdirectory(render)
add_subdirectory(resource)
add_subdirectory(sdl)
add_subdirectory(test)
add_subdirectory(thread)
