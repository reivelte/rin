cmake_minimum_required(VERSION 3.22)
# not needed when using llvmPackages.libcxxStdenv versus llvmPackages.stdenv?
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "osx deployment target")
project(rin-project VERSION 1.0 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(as_subproject suzuri rin)

# This macro originates from Daniel Pfeifer's talk at C++Now 2017:
# https://www.youtube.com/watch?v=bsXLMQ6WgIk
macro(find_package)
  if(NOT "${ARGV0}" IN_LIST as_subproject)
    _find_package(${ARGV})
  endif()
endmacro()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add -O0 to remove optimizations when using gcc
# breakpoints made in vscode don't work when debugging the cli otherwise
IF(UNIX) # we assume either gcc or clang is being used to compile
    set(CMAKE_CXX_FLAGS_DEBUG "-g -U_FORTIFY_SOURCE -O0 -Wno-cpp -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion -Wfatal-errors")
    set(CMAKE_C_FLAGS_DEBUG   "-g -U_FORTIFY_SOURCE -O0 -Wno-cpp -Wall -Wextra -Wpedantic -Wconversion -Wno-sign-conversion -Wfatal-errors")
ENDIF(UNIX)

add_subdirectory(lib)   # suzuri (libsuzuri)
add_subdirectory(cli)   # rin
add_subdirectory(gui)   # rin-gui