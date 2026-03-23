cmake_minimum_required(VERSION 3.16...3.24)
set(PROJECT_NAME "SoundLink")
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Qml Quick QuickControls2 Network)

add_subdirectory(Source/Core)
add_subdirectory(Source/UI/NewWindows)

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
        NewWindowsUI
)

target_include_directories(${PROJECT_NAME}
        PUBLIC
        "${CMAKE_SOURCE_DIR}/Source/Core/Public/Framework"
)

qt_finalize_executable(${PROJECT_NAME})

# Install and dlls copying logic

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

# Core — MusicPlayerCore.dll + IModuleInterface живёт здесь
#

#if(WIN32)
#    #
#
#
#
#
#    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#            COMMAND ${CMAKE_COMMAND} -E copy_if_different
#            $<TARGET_FILE:NewWindowsUI>
#            $<TARGET_FILE_DIR:${PROJECT_NAME}>
#    )
#
#    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#            COMMAND ${CMAKE_COMMAND} -E copy_if_different
#            $<TARGET_FILE:MusicPlayerCore>
#            $<TARGET_FILE_DIR:${PROJECT_NAME}>
#    )
#
#    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
#
#    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#            COMMAND ${WINDEPLOYQT}
#            --qmldir "${CMAKE_SOURCE_DIR}/Source/UI/NewWindows"
#            $<TARGET_FILE:${PROJECT_NAME}>
#    )
#
#    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#            COMMAND ${WINDEPLOYQT}
#            --qmldir "${CMAKE_SOURCE_DIR}/Source/Core"
#            $<TARGET_FILE:${PROJECT_NAME}>
#    )
#
#    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
#    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
#    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
#
#    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#            COMMAND ${WINDEPLOYQT_EXECUTABLE}
#            --qmldir "${CMAKE_SOURCE_DIR}/Source/UI/NewWindows"
#            --qmlimport "${Qt6_DIR}/../../../qml"
#            --compiler-runtime
#            --no-translations
#            $<TARGET_FILE:${PROJECT_NAME}>
#    )
#
#    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#            COMMAND ${CMAKE_COMMAND} -E copy_directory
#            "${Qt6_DIR}/../../../qml/QtQuick"
#            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qml/QtQuick"
#            COMMAND ${CMAKE_COMMAND} -E copy_directory
#            "${Qt6_DIR}/../../../qml/QtQuick/Controls"
#            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qml/QtQuick/Controls"
#            COMMAND ${CMAKE_COMMAND} -E copy_directory
#            "${Qt6_DIR}/../../../qml/QtQuick/Layouts"
#            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/qml/QtQuick/Layouts"
#    )
#
#    get_target_property(QT6_MULTIMEDIA_LOCATION Qt6::Multimedia IMPORTED_LOCATION_RELEASE)
#    if(NOT QT6_MULTIMEDIA_LOCATION)
#        get_target_property(QT6_MULTIMEDIA_LOCATION Qt6::Multimedia IMPORTED_LOCATION)
#    endif()
#
#    if(QT6_MULTIMEDIA_LOCATION)
#        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#                COMMAND ${CMAKE_COMMAND} -E copy_if_different
#                ${QT6_MULTIMEDIA_LOCATION}
#                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
#                COMMENT "Принудительное копирование Qt6Multimedia.dll"
#        )
#    endif()
#
#    get_target_property(QT6_QML_LOCATION Qt6::Qml IMPORTED_LOCATION_RELEASE)
#    if(NOT QT6_QML_LOCATION)
#        get_target_property(QT6_QML_LOCATION Qt6::Qml IMPORTED_LOCATION)
#    endif()
#
#    if(QT6_MULTIMEDIA_LOCATION)
#        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#                COMMAND ${CMAKE_COMMAND} -E copy_if_different
#                ${QT6_QML_LOCATION}
#                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
#                COMMENT "Принудительное копирование Qt6Qml.dll"
#        )
#    endif()
#
#    get_target_property(QT6_QMLControls_LOCATION Qt6::Quick IMPORTED_LOCATION_RELEASE)
#    if(NOT QT6_QMLControls_LOCATION)
#        get_target_property(QT6_QMLControls_LOCATION Qt6::Quick IMPORTED_LOCATION)
#    endif()
#
#    if(QT6_QMLControls_LOCATION)
#        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#                COMMAND ${CMAKE_COMMAND} -E copy_if_different
#                ${QT6_QMLControls_LOCATION}
#                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
#        )
#    endif()
#
#    qt_finalize_executable(${PROJECT_NAME})
#endif()