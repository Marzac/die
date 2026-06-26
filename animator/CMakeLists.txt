cmake_minimum_required(VERSION 3.16)
project(rigger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(SOURCES
    main.cpp
    rigger.cpp
    mainwindow.cpp
    wdgrigeditor.cpp
    ../common/engine/rig.cpp
    ../common/engine/rig_io.cpp
    mainwindow.ui
)

#if(WIN32)
#    list(APPEND SOURCES rigger.rc)
#endif()

qt_add_executable(rigger WIN32 ${SOURCES})

target_include_directories(rigger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../common/engine
)

target_link_libraries(rigger PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

#if(WIN32)
#    target_link_libraries(rigger PRIVATE xinput winmm ws2_32)
#endif()

if(MSVC)
    target_compile_options(rigger PRIVATE /W4 /MP /WX-)
    target_compile_options(rigger PRIVATE $<$<CONFIG:Release>:/O2>)
else()
    target_compile_options(rigger PRIVATE -msse4 -save-temps -Wall -Wextra)
    target_compile_options(rigger PRIVATE $<$<CONFIG:Release>:-O2>)
    target_link_options(rigger PRIVATE $<$<CONFIG:Release>:-s>)
endif()
