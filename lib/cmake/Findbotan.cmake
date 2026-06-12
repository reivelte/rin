#from: https://stackoverflow.com/questions/72322755/how-do-i-link-botan-in-cmake
if(NOT MSVC)
  find_package(PkgConfig REQUIRED)

  if (NOT TARGET botan::botan)
    pkg_check_modules(botan IMPORTED_TARGET botan-3)
    #set(Botan_INCLUDE_DIRS ${Botan_INCLUDE_DIRS}/botan)
    if (TARGET PkgConfig::botan)
      add_library(botan::botan ALIAS PkgConfig::botan)
      set_target_properties(
      PkgConfig::botan
      PROPERTIES
      IMPORTED_LOCATION "${botan_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${botan_INCLUDE_DIRS}"
      )
    endif ()
  endif ()

  if (NOT TARGET botan::botan)
    find_path(botan_INCLUDE_DIRS NAMES botan/botan.h
              PATH_SUFFIXES botan-3
              DOC "The Botan include directory")

    find_library(botan_LIBRARIES NAMES botan botan-3
                DOC "The Botan library")

    mark_as_advanced(botan_INCLUDE_DIRS botan_LIBRARIES)
    
    add_library(botan::botan IMPORTED UNKNOWN)
    set_target_properties(
      botan::botan
      PROPERTIES
      IMPORTED_LOCATION "${botan_LIBRARIES}"
      INTERFACE_INCLUDE_DIRECTORIES "${botan_INCLUDE_DIRS}"
    )
  endif ()

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    botan
    REQUIRED_VARS botan_LIBRARIES botan_INCLUDE_DIRS
  )
endif()