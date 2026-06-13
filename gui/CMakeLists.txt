cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "osx deployment target")
project(rin-gui LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(suzuri)
find_package(SQLite3 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Svg)

set(SOURCES #!BEGIN: SOURCE LIST
  src/window.cpp
  src/main.cpp
  src/tagger/tag_panel.cpp
  src/tagger/tagger.cpp
  src/view/delegate.cpp
  src/view/view.cpp
  src/widgets/panel.cpp
  src/widgets/resizing_textedit.cpp
  src/widgets/details_panel.cpp
  src/widgets/navigationpanel.cpp
  src/widgets/iconsizer.cpp
  src/widgets/tag_listing.cpp
  src/widgets/dialogs/settings.cpp
  src/widgets/dialogs/sidecar.cpp
  src/widgets/dialogs/domain.cpp
  src/model/entitymodel.cpp
  src/model/common.cpp
  src/model/datamanager.cpp
  src/model/thumbnailmanager.cpp
  src/model/historymanager.cpp
  src/model/tree.cpp
  src/window.hpp
  src/rin.hpp
  src/tagger/tag_panel.hpp
  src/tagger/tagger.hpp
  src/view/delegate.hpp
  src/view/view.hpp
  src/widgets/panel.hpp
  src/widgets/details_panel.hpp
  src/widgets/resizing_textedit.hpp
  src/widgets/navigationpanel.hpp
  src/widgets/iconsizer.hpp
  src/widgets/tag_listing.hpp
  src/widgets/dialogs/settings.hpp
  src/widgets/dialogs/sidecar.hpp
  src/widgets/dialogs/domain.hpp
  src/utility/painting.hpp
  src/utility/sizing.hpp
  src/utility/bsp.hpp
  src/model/datamanager.hpp
  src/model/entitymodel.hpp
  src/model/entity.hpp
  src/model/common.hpp
  src/model/thumbnailmanager.hpp
  src/model/tree.hpp
  src/model/historymanager.hpp
) #!END: SOURCE LIST

add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

if(NOT MSVC)
  target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
else()
  set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 23)
endif()

target_link_libraries(${PROJECT_NAME}
    suzuri
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Svg
)
target_link_libraries(${PROJECT_NAME} SQLite::SQLite3)

target_include_directories(${PROJECT_NAME} PRIVATE "src/")

install(TARGETS ${PROJECT_NAME} DESTINATION bin)