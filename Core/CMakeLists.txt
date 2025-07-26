cmake_minimum_required(VERSION 3.16)
project(MusicPlayerCore)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Multimedia)

file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp
)

file(GLOB_RECURSE HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/Public/*.h"
)

foreach(header ${HEADERS})
    get_filename_component(dir ${header} DIRECTORY)
    list(APPEND INCLUDE_DIRS ${dir})
endforeach()

list(REMOVE_DUPLICATES INCLUDE_DIRS)

add_library(MusicPlayerCore SHARED ${SOURCES} ${HEADERS})

target_link_libraries(MusicPlayerCore
        PUBLIC
        Qt6::Core
        Qt6::Multimedia
        Qt6::CorePrivate
)

target_include_directories(MusicPlayerCore
        PUBLIC
        ${INCLUDE_DIRS}
)

if(PLATFORM_WINDOWS)
    target_link_libraries(MusicPlayerCore PRIVATE
            winmm
            ole32
            user32
            advapi32
    )

    target_compile_definitions(MusicPlayerCore
            PRIVATE
            MusicPlayerCore_EXPORTS
            QT_WIDGETS_LIB
            INTERFACE
            MusicPlayerCore_IMPORTS
    )

    set_target_properties(MusicPlayerCore PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

elseif(PLATFORM_ANDROID)
    target_link_libraries(MusicPlayerCore PRIVATE
            android
            log
            OpenSLES
    )

    target_compile_definitions(MusicPlayerCore PRIVATE
            ANDROID_PLATFORM
            QT_ANDROID
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)
