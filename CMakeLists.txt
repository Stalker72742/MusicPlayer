cmake_minimum_required(VERSION 3.16...3.24)
set(PROJECT_NAME "SoundLink")
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

#Project settings and vars
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Qml Quick QuickControls2 Network)

#Core lib
add_subdirectory(Source/Core)

#Base libs and mods
add_subdirectory(Source/UI/NewWindows)

#Target creation and target settings
qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION
        main.cpp
)

target_link_libraries(${PROJECT_NAME}
        PUBLIC
        Qt6::Multimedia
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
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
        $<TARGET_FILE:${PROJECT_NAME}>
)

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND python ${CMAKE_SOURCE_DIR}/scripts/post_build.py
        --source "${CMAKE_BINARY_DIR}/Source"
        --dest   "${CMAKE_BINARY_DIR}"
        COMMENT "Copying DLLs..."
)