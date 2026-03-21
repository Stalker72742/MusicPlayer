cmake_minimum_required(VERSION 3.16...3.24)
set(PROJECT_NAME "SoundLink")
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Network)
endif()

# Core — MusicPlayerCore.dll + IModuleInterface живёт здесь
add_subdirectory(Source/Core)

if(WIN32)
    # UI модули — каждый отдельная .dll со своей точкой входа
    add_subdirectory(Source/UI/NewWindows)

    qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION
            main.cpp
            # ресурсы, qrc и тд — только здесь, никакого UI кода
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
            Source/Core/Public/Framework    # IModuleInterface, AppInstance
            Source/UI/NewWindows            # NewWindowsModule.h
    )

    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            MusicPlayerCore
            NewWindowsUI    # нужен чтобы NewWindowsModule.h был доступен и dll собралась
            Qt6::Core
            Qt6::Widgets
    )

    # Копируем .dll рядом с exe
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:NewWindowsUI>
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

    qt_finalize_executable(${PROJECT_NAME})
endif()