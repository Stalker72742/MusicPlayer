cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "NewWindowsUI")
project(NewWindowsUI)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Qml Quick QuickControls2)

add_library(NewWindowsUI SHARED
        NewWindowsModule.h
        NewWindowsModule.cpp
        Resources/Resources.qrc
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/app.rc"
        qml.qrc
)

set_target_properties(NewWindowsUI PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        PREFIX ""
        OUTPUT_NAME "NewWindowsUI"
)

target_link_libraries(NewWindowsUI
        PRIVATE
        Qt6::Core Qt6::Widgets Qt6::Qml Qt6::Quick Qt6::QuickControls2 MusicPlayerCore
)

target_include_directories(NewWindowsUI
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Public/Framework
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Public/Subsystems
        ${CMAKE_CURRENT_SOURCE_DIR}/../../Core/Public/Data
)

if(WIN32)
    target_link_libraries(NewWindowsUI PRIVATE dwmapi)
    set_target_properties(NewWindowsUI PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")

add_custom_command(TARGET NewWindowsUI POST_BUILD
        COMMAND ${WINDEPLOYQT}
        --qmldir "${CMAKE_CURRENT_SOURCE_DIR}"
        $<TARGET_FILE:NewWindowsUI>
        COMMENT "Running windeployqt on NewWindowsUI.dll"
)