cmake_minimum_required(VERSION 3.16)
project(waller LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets SpatialAudio)

set(SOURCES
    main.cpp
    editor.cpp
    walker.cpp
    mainwindow.cpp
    renderwindow.cpp
    wdgmapeditor.cpp
    wdgmapview.cpp
    wdgsurfaceedit.cpp
    wdgtextureselector.cpp
    wdgtextureview.cpp
    wdgutilities.cpp
    ../common/engine/renderer.cpp
    ../common/engine/renderer_config.cpp
    ../common/engine/audio.cpp
    ../common/engine/map.cpp
    ../common/engine/map_io.cpp
    ../common/engine/tags.cpp
    ../common/engine/env.cpp
    ../common/engine/workerpool.cpp
    ../common/engine/gamepad.cpp
    mainwindow.ui
    renderwindow.ui
    resources.qrc
)

if(WIN32)
    list(APPEND SOURCES editor.rc)
endif()

qt_add_executable(waller WIN32 ${SOURCES}
    wdgsurfaceedit.h wdgsurfaceedit.ui
    wdgutilities.h)

# Build version header (buildinfo.h), read from git on every build
find_package(Git QUIET)
set(BUILDINFO_HEADER ${CMAKE_CURRENT_BINARY_DIR}/buildinfo.h)
add_custom_target(waller_buildinfo
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -DOUTPUT=${BUILDINFO_HEADER}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/../common/version.cmake
    BYPRODUCTS ${BUILDINFO_HEADER}
    COMMENT "Reading the build version from git"
    VERBATIM)
add_dependencies(waller waller_buildinfo)

target_include_directories(waller PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ../common/engine
)

target_link_libraries(waller PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::SpatialAudio
)

if(WIN32)
    target_link_libraries(waller PRIVATE xinput winmm ws2_32)
endif()

if(MSVC)
    target_compile_options(waller PRIVATE /W4 /MP /WX-)
    target_compile_options(waller PRIVATE $<$<CONFIG:Release>:/O2>)
else()
    target_compile_options(waller PRIVATE -msse4 -save-temps -Wall -Wextra)
    target_compile_options(waller PRIVATE $<$<CONFIG:Release>:-O2>)
    target_link_options(waller PRIVATE $<$<CONFIG:Release>:-s>)
endif()
