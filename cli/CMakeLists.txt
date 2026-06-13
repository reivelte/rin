cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "osx deployment target")

project(rin VERSION 1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(suzuri REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(botan REQUIRED)

set(HEADERS #!BEGIN: HEADER LIST
  src/config.hpp
  src/args.hpp
)#!END: HEADER LIST

set(SOURCES #!BEGIN: SOURCE LIST
  ${HEADERS}
  src/main.cpp
  src/config.cpp
)#!END: SOURCE LIST

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES}) # To display files properly in Visual Studio

if(NOT MSVC)
  target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
else()
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
endif()

target_link_libraries(${PROJECT_NAME} suzuri)
target_link_libraries(${PROJECT_NAME} SQLite::SQLite3 botan::botan)

target_include_directories(${PROJECT_NAME} PRIVATE "src/")
target_include_directories(${PROJECT_NAME} PRIVATE "ext/argparse/include/")

install(TARGETS ${PROJECT_NAME} DESTINATION bin)