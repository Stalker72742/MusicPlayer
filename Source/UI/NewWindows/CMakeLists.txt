cmake_minimum_required(VERSION 3.16)
project(NewWindowsUI)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# NewWindows UI Library
add_library(NewWindowsUI STATIC
    MainWindow/MainWindow.h
    MainWindow/MainWindow.cpp
    TitleBar/TitleBar.h
    TitleBar/TitleBar.cpp
    BottomBar/BottomBar.h
    BottomBar/BottomBar.cpp
)

target_link_libraries(NewWindowsUI
    PUBLIC
    Qt6::Core
    Qt6::Widgets
    MusicPlayerCore
)

target_include_directories(NewWindowsUI
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow
    ${CMAKE_CURRENT_SOURCE_DIR}/TitleBar
    ${CMAKE_CURRENT_SOURCE_DIR}/BottomBar
)

# Platform-specific settings
if(WIN32)
    target_link_libraries(NewWindowsUI PRIVATE dwmapi)
endif()

set_target_properties(NewWindowsUI PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
