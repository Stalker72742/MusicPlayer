cmake_minimum_required(VERSION 3.16)
project(OldWindowsUI)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Old Windows UI Library - SHARED for dynamic loading
add_library(OldWindowsUI SHARED
    # UI Factory (zero dependency)
    OldWindowsFactory.h
    OldWindowsFactory.cpp

    # UI Components
    MainWindow/mainwindow.cpp
    MainWindow/mainwindow.h
    MediaLib/addtoplaylist.cpp
    MediaLib/addtoplaylist.h
    MediaLib/medialibitem.cpp
    MediaLib/medialibitem.h
    OldWidgets/Components/foundedSongButton.cpp
    OldWidgets/Components/foundedSongButton.h
    OldWidgets/Components/musicbutton.cpp
    OldWidgets/Components/musicbutton.h
    OldWidgets/Playlist/playlistitem.cpp
    OldWidgets/Playlist/playlistitem.h
    Subsystems/windowsmediaplayer.h
    Subsystems/windowsmediaplayer.cpp
)

target_link_libraries(OldWindowsUI
    PUBLIC
    Qt6::Core
    Qt6::Widgets
    MusicPlayerCore
)

target_include_directories(OldWindowsUI
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow
    ${CMAKE_CURRENT_SOURCE_DIR}/MediaLib
    ${CMAKE_CURRENT_SOURCE_DIR}/OldWidgets/Components
    ${CMAKE_CURRENT_SOURCE_DIR}/OldWidgets/Playlist
    ${CMAKE_CURRENT_SOURCE_DIR}/Subsystems
)

target_compile_definitions(OldWindowsUI PRIVATE UI_LIBRARY_EXPORT)

set_target_properties(OldWindowsUI PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
