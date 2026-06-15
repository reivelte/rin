cmake_minimum_required(VERSION 3.14)

# must be set before call to project() (https://cmake.org/cmake/help/latest/variable/CMAKE_OSX_DEPLOYMENT_TARGET.html)
set(CMAKE_OSX_DEPLOYMENT_TARGET
    "15.0"
    CACHE STRING "osx deployment target")

project(
  suzuri
  VERSION 1.0.0
  DESCRIPTION "General purpose library designed to assist in creating data management and real-time applications in C++"
  LANGUAGES CXX
)

#----------------------------------------------------------------------------------------------------------------------
# general settings and options
#----------------------------------------------------------------------------------------------------------------------
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/utils.cmake)
include(GNUInstallDirs)

string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level) # or use PROJECT_IS_TOP_LEVEL for CMake 3.21+

# SZ_SHARED_LIBS determines static/shared build when defined
option(SZ_SHARED_LIBS "Build ${PROJECT_NAME} as a shared library" ON)
option(SZ_BUILD_TESTS "Build ${PROJECT_NAME} tests" OFF)
option(SZ_BUILD_EXAMPLES "Build ${PROJECT_NAME} examples" OFF)
option(SZ_BUILD_DOCS "Build documentation" OFF)
option(SZ_INSTALL "Generate target for installing ${PROJECT_NAME}" ${is_top_level})
set_if_undefined(SZ_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}" CACHE STRING "Install path for ${PROJECT_NAME} package-related CMake files")

if(SZ_SHARED_LIBS)
  set(BUILD_SHARED_LIBS ${SZ_SHARED_LIBS})
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# disable exporting all symbols by default for GCC and Clang (MSVC exports nothing by default)
set_if_undefined(CMAKE_CXX_VISIBILITY_PRESET hidden)
set_if_undefined(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

add_library(${PROJECT_NAME})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

#----------------------------------------------------------------------------------------------------------------------
# dependencies
#----------------------------------------------------------------------------------------------------------------------
find_package(SQLite3 REQUIRED)
find_package(botan REQUIRED)
#----------------------------------------------------------------------------------------------------------------------
# sources
#----------------------------------------------------------------------------------------------------------------------
include(GenerateExportHeader)
set(export_file_name "sz_export.hpp")
generate_export_header(
  ${PROJECT_NAME}
  BASE_NAME "SZ"
  EXPORT_FILE_NAME src/${PROJECT_NAME}/${export_file_name}
  EXPORT_MACRO_NAME "SZ_API"
  DEPRECATED_MACRO_NAME "SZ_DEPRECATED_API"
)

set(public_headers #!BEGIN: HEADER LIST
  src/${PROJECT_NAME}/exception.hpp
  src/${PROJECT_NAME}/types.hpp
  src/${PROJECT_NAME}/sz_export.hpp
  src/${PROJECT_NAME}/basic_vectors.hpp
  src/${PROJECT_NAME}/result.hpp
  src/${PROJECT_NAME}/entity/database.hpp
  src/${PROJECT_NAME}/entity/entity.hpp
  src/${PROJECT_NAME}/entity/common.hpp
  src/${PROJECT_NAME}/entity/registry.hpp
  src/${PROJECT_NAME}/sqlite/common.hpp
  src/${PROJECT_NAME}/sqlite/query.hpp
  src/${PROJECT_NAME}/sqlite/database.hpp
  src/${PROJECT_NAME}/utility/files.hpp
  src/${PROJECT_NAME}/utility/hash.hpp
  src/${PROJECT_NAME}/utility/string.hpp
  src/${PROJECT_NAME}/utility/domain.hpp
  src/${PROJECT_NAME}/utility/config_utils.hpp
  src/${PROJECT_NAME}/utility/json.hpp
  src/${PROJECT_NAME}/metadata/tags.hpp
) #!END: HEADER LIST

set(sources #!BEGIN: SOURCE LIST
  ${public_headers}
  src/${PROJECT_NAME}/entity/database.cpp
  src/${PROJECT_NAME}/entity/common.cpp
  src/${PROJECT_NAME}/entity/registry.cpp
  src/${PROJECT_NAME}/sqlite/database.cpp
  src/${PROJECT_NAME}/sqlite/query.cpp
  src/${PROJECT_NAME}/utility/string.cpp
  src/${PROJECT_NAME}/utility/files.cpp
  src/${PROJECT_NAME}/utility/hash.cpp
  src/${PROJECT_NAME}/utility/domain.cpp
  src/${PROJECT_NAME}/utility/config_utils.cpp
  src/${PROJECT_NAME}/utility/json.cpp
  src/${PROJECT_NAME}/metadata/tags.cpp
)#!END: SOURCE LIST

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${sources}) # for properly displying directory structure in IDEs

list(APPEND sources "${CMAKE_CURRENT_BINARY_DIR}/src/${PROJECT_NAME}/${export_file_name}")

#----------------------------------------------------------------------------------------------------------------------
# main target
#----------------------------------------------------------------------------------------------------------------------
include(CMakePackageConfigHelpers)

target_sources(${PROJECT_NAME} PRIVATE ${sources})
target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    "$<$<NOT:$<BOOL:${BUILD_SHARED_LIBS}>>:SZ_STATIC_DEFINE>"
)

if(NOT MSVC)
  target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
else()
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
endif()

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    SQLite::SQLite3
    botan::botan
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/src/${PROJECT_NAME}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ext/nlohmann/json/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ext/ToruNiina/toml11/include>
    $<BUILD_INTERFACE:${Botan_INCLUDE_DIRS}>
    $<BUILD_INTERFACE:${SQLite3_INCLUDE_DIRS}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  #PRIVATE
  #    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  #PUBLIC_HEADER "${public_headers}"
  SOVERSION ${PROJECT_VERSION_MAJOR}
  VERSION ${PROJECT_VERSION}
)

if(${SZ_INSTALL} AND NOT CMAKE_SKIP_INSTALL_RULES)
  configure_package_config_file(cmake/${PROJECT_NAME}-config.cmake.in ${PROJECT_NAME}-config.cmake INSTALL_DESTINATION "${SZ_INSTALL_CMAKEDIR}")

  write_basic_package_version_file(${PROJECT_NAME}-config-version.cmake COMPATIBILITY SameMajorVersion)

  install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}_export
    RUNTIME COMPONENT ${PROJECT_NAME}
    LIBRARY COMPONENT ${PROJECT_NAME} NAMELINK_COMPONENT ${PROJECT_NAME}-dev
    ARCHIVE COMPONENT ${PROJECT_NAME}-dev
    #PUBLIC_HEADER COMPONENT ${PROJECT_NAME}-dev DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )

  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/src/${PROJECT_NAME}/${export_file_name}"
    COMPONENT ${PROJECT_NAME}-dev
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  )

  foreach(header ${public_headers})
    string(REPLACE src include DEST_FILE ${header})         # replace 'src' with 'include'
    string(FIND ${DEST_FILE} / LAST_SLASH REVERSE)          # last occurrence of '/' in path
    string(SUBSTRING ${DEST_FILE} 0 ${LAST_SLASH} DEST_DIR) # chop out the filename
    message("Installing: ${header} to ${DEST_DIR}")
    install(FILES ${header} COMPONENT ${PROJECT_NAME}-dev DESTINATION ${DEST_DIR})
  endforeach()

  # TODO: automated way to install all vendor local (not from FindPackage) includes
  install(FILES ext/nlohmann/json/include/nlohmann/json.hpp DESTINATION include/nlohmann)
  install(FILES ext/ToruNiina/toml11/include/toml.hpp DESTINATION include/)

  set(targets_file "${PROJECT_NAME}-static-targets.cmake")
  if(BUILD_SHARED_LIBS)
    set(targets_file "${PROJECT_NAME}-shared-targets.cmake")
  endif()

  install(EXPORT ${PROJECT_NAME}_export
    COMPONENT ${PROJECT_NAME}-dev
    FILE "${targets_file}"
    DESTINATION "${SZ_INSTALL_CMAKEDIR}"
    NAMESPACE ${PROJECT_NAME}::
  )

  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake"
    COMPONENT ${PROJECT_NAME}-dev
    DESTINATION "${SZ_INSTALL_CMAKEDIR}"
  )

  if(MSVC)
    set(pdb_file "")
    set(pdb_file_destination "")

    if(BUILD_SHARED_LIBS)
        set(pdb_file "$<TARGET_PDB_FILE:${PROJECT_NAME}>")
        set(pdb_file_destination "${CMAKE_INSTALL_BINDIR}")
    else()
        # TARGET_PDB_FILE does not work for pdb file generated by the compiler during static library builds--determine it another way
        set(pdb_file "$<TARGET_FILE_DIR:${PROJECT_NAME}>/$<TARGET_FILE_PREFIX:${PROJECT_NAME}>$<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.pdb")
        set(pdb_file_destination "${CMAKE_INSTALL_LIBDIR}")
    endif()

    install(FILES "${pdb_file}"
        COMPONENT ${PROJECT_NAME}-dev
        CONFIGURATIONS Debug RelWithDebInfo
        DESTINATION "${pdb_file_destination}"
        OPTIONAL
    )
  endif()

endif()

#----------------------------------------------------------------------------------------------------------------------
# other targets
#----------------------------------------------------------------------------------------------------------------------

if(SZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

if(SZ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(SZ_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    doxygen_add_docs(docs include)
endif()