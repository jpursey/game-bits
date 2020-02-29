function(gbits_external_libraries)
  ####### SDL2

  add_library(sdl2 SHARED IMPORTED)
  target_include_directories(sdl2 INTERFACE 
    "${GBITS_THIRD_PARTY_DIR}/SDL2-2.0.10/include"
  )
  set_target_properties(sdl2 PROPERTIES
    IMPORTED_IMPLIB
      "${GBITS_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2.lib"
  )

  add_library(sdl2_main STATIC IMPORTED)
  set_target_properties(sdl2_main PROPERTIES
    IMPORTED_LOCATION
      "${GBITS_THIRD_PARTY_DIR}/SDL2-2.0.10/lib/x64/SDL2main.lib"
  )

  ####### SDL2_image

  add_library(sdl2_image SHARED IMPORTED)
  target_include_directories(sdl2_image INTERFACE
    "${GBITS_THIRD_PARTY_DIR}/SDL2_image-2.0.5/include"
  )
  set_target_properties(sdl2_image PROPERTIES
    IMPORTED_IMPLIB
      "${GBITS_THIRD_PARTY_DIR}/SDL2_image-2.0.5/lib/x64/SDL2_image.lib"
  )

  ####### glog

  add_library(glog STATIC IMPORTED)
  target_include_directories(glog INTERFACE
    "${GBITS_BUILD_DIR}/third_party/glog"
    "${GBITS_THIRD_PARTY_DIR}/glog/src/windows"
  )
  target_compile_definitions(glog INTERFACE
    # This is required as glog is built as a static library. 
    GOOGLE_GLOG_DLL_DECL=
    # This is required to be compatable with includes of windows.h
    GLOG_NO_ABBREVIATED_SEVERITIES=
  )
  set_target_properties(glog PROPERTIES
    IMPORTED_LOCATION
      "${GBITS_BUILD_DIR}/third_party/glog/$(Configuration)/glog.lib" 
    IMPORTED_LOCATION_DEBUG
      "${GBITS_BUILD_DIR}/third_party/glog/$(Configuration)/glogd.lib" 
  )
endfunction()