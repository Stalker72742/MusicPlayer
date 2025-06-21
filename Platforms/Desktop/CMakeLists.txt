cmake_minimum_required(VERSION 3.21)
project(DesktopMusicPlayerUI VERSION 0.1 LANGUAGES CXX)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB_RECURSE HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

file(GLOB_RECURSE UI_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(DesktopMusicPlayerUI SHARED
        ${SOURCES}
        ${HEADERS}
        ${UI_FILES}
)

foreach(header ${HEADERS})
    get_filename_component(dir ${header} DIRECTORY)
    list(APPEND INCLUDE_DIRS ${dir})
endforeach()

list(REMOVE_DUPLICATES INCLUDE_DIRS)

target_include_directories(DesktopMusicPlayerUI
        PUBLIC
        ${INCLUDE_DIRS}
)

target_link_libraries(DesktopMusicPlayerUI
        PUBLIC
        MusicPlayerCore
        Qt6::Core
        Qt6::Widgets
)

target_compile_definitions(DesktopMusicPlayerUI
        PRIVATE
        DesktopMusicPlayerUI_EXPORTS
        QT_WIDGETS_LIB
        INTERFACE
        DesktopMusicPlayerUI_IMPORTS
)