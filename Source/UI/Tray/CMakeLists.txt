cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "Tray")
project(${PROJECT_NAME})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

add_library(${PROJECT_NAME} SHARED

)

set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        PREFIX ""
        OUTPUT_NAME "${PROJECT_NAME}"
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        MusicPlayerCore
        Qt6::Core
        Qt6::Widgets
)

target_include_directories(${PROJECT_NAME}
        PUBLIC

)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dwmapi)
    set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

#find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
#
#add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#        COMMAND ${WINDEPLOYQT}
#        --qmldir "${CMAKE_CURRENT_SOURCE_DIR}"
#        $<TARGET_FILE:${PROJECT_NAME}>
#        COMMENT "Running windeployqt on Tray.dll"
#)