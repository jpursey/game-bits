function(gbits_config)
  # These values configure where GBits parts are located or should be put. They
  # can be defined by a game project to be wherever is convenient for building 
  # into their cmake projects.

  # Directory to the root of the gbits repository. If not defined, it is assumed
  # this is the gbits repository or a copy/clone of it.
  if (NOT DEFINED GBITS_DIR)
    set(GBITS_DIR "${CMAKE_CURRENT_LIST_DIR}")
    set(GBITS_DIR "${CMAKE_CURRENT_LIST_DIR}" PARENT_SCOPE)
  endif()

  # Directory to the third party libraries gbits depends on. By default, this is
  # points to gbits own third_party directory, but it can be redirected to any
  # third party directory that has the required libraries.
  if (NOT DEFINED GBITS_THIRD_PARTY_DIR)
    set(GBITS_THIRD_PARTY_DIR "${GBITS_DIR}/third_party" PARENT_SCOPE)
  endif()

  # Directory to where all build output should go. This is where all cmake output
  # is put for gbits, and code that uses the the gbits target utility functions.
  if (NOT DEFINED GBITS_BUILD_DIR)
    set(GBITS_BUILD_DIR "${GBITS_DIR}/build" PARENT_SCOPE)
  endif()

  # Directory to where all final executable output will go.
  if (NOT DEFINED GBITS_BIN_DIR)
    set(GBITS_BIN_DIR "${GBITS_DIR}/bin" PARENT_SCOPE)
  endif()
endfunction()