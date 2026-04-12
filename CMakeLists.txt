cmake_minimum_required(VERSION 3.16...3.24)
set(PROJECT_NAME "SoundLink")
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

#Project settings and vars
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Network)

#Core lib
add_subdirectory(Source/Core)

#Base libs and mods
add_subdirectory(Source/UI/NewWindowsUI)

#Target creation and target settings
qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION
        main.cpp
        Resources/app.rc
)

target_link_libraries(${PROJECT_NAME}
        PUBLIC
        Qt6::Core
        Qt6::Widgets
        MusicPlayerCore
)

#Mods as target dependencies
add_dependencies(${PROJECT_NAME}
        NewWindowsUI
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        "${CMAKE_SOURCE_DIR}/Source/Core/Public/Framework"
)

qt_finalize_executable(${PROJECT_NAME})

#Install and dlls copying logic

find_program(WINDEPLOYQT windeployqt HINTS ${QT_PATH}/bin)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${WINDEPLOYQT}
        --dir $<TARGET_FILE_DIR:${PROJECT_NAME}>
        --qmldir ${CMAKE_SOURCE_DIR}
        --multimedia
        --quick
        --no-translations
        $<TARGET_FILE:${PROJECT_NAME}>
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_BINARY_DIR}/Source/Core/libMusicPlayerCore.dll"
        "${CMAKE_BINARY_DIR}/libMusicPlayerCore.dll"
)

set(PLUGINS "NewWindowsUI;Tray")

string(REPLACE ";" " " PLUGINS_STR "${PLUGINS}")

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND python ${CMAKE_SOURCE_DIR}/scripts/GeneratePluginsFiles.py
        --dest "${CMAKE_BINARY_DIR}/Plugins"
        --source   "${CMAKE_SOURCE_DIR}/Source"
        --binaryDir ${CMAKE_BINARY_DIR}
        --plugins ${PLUGINS}
)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)