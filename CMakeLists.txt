cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "MusicPlayer")
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(ANDROID)
    set(PLATFORM_ANDROID TRUE)
    set(PLATFORM_WINDOWS FALSE)
elseif(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    set(PLATFORM_ANDROID FALSE)
else()
    set(PLATFORM_WINDOWS FALSE)
    set(PLATFORM_ANDROID FALSE)
endif()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets Multimedia)
find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)

if(PLATFORM_ANDROID)
    set(ANDROID_PLATFORM android-24)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fexceptions")

    set_property(TARGET Qt6::Core PROPERTY INTERFACE_QT_ANDROID_DEPLOYMENT_DEPENDENCIES "")
endif()

add_subdirectory(Core)

if(PLATFORM_ANDROID)

    add_subdirectory(Platforms/Android)
elseif(PLATFORM_WINDOWS)

    add_subdirectory(Platforms/Desktop)
else()

    message("Update cmake code for new platform")
endif ()

if (PLATFORM_WINDOWS)

    qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION main.cpp)

    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            MusicPlayerCore
            DesktopMusicPlayerUI
            Qt6::Core
            Qt6::Multimedia
    )
elseif (PLATFORM_ANDROID)

    qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION main.cpp)

    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            MusicPlayerCore
            AndroidMusicPlayerUI
            Qt6::Core
            Qt6::Multimedia
    )
else ()

endif ()


include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(PLATFORM_WINDOWS)

    get_target_property(QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
            --verbose 2
            --no-translations
            --no-system-d3d-compiler
            --no-opengl-sw
            $<TARGET_FILE:${PROJECT_NAME}>
            COMMENT "Развертывание Qt библиотек рядом с exe"
    )

    set(DLL_FILES
            "D:/Qt/Tools/mingw1310_64/bin/libgcc_s_seh-1.dll"
            "D:/Qt/Tools/mingw1310_64/bin/libstdc++-6.dll"
            "D:/Qt/Tools/mingw1310_64/bin/libwinpthread-1.dll"
            "$<TARGET_FILE:MusicPlayerCore>"
            "$<TARGET_FILE:DesktopMusicPlayerUI>"
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_FILES}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )

    get_target_property(QT6_MULTIMEDIA_LOCATION Qt6::Multimedia IMPORTED_LOCATION_RELEASE)
    if(NOT QT6_MULTIMEDIA_LOCATION)
        get_target_property(QT6_MULTIMEDIA_LOCATION Qt6::Multimedia IMPORTED_LOCATION)
    endif()

    if(QT6_MULTIMEDIA_LOCATION)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_MULTIMEDIA_LOCATION}
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
                COMMENT "Принудительное копирование Qt6Multimedia.dll"
        )
    endif()

    set(DEPLOY_DIR "${CMAKE_CURRENT_BINARY_DIR}/Release")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${PROJECT_NAME}>" "${DEPLOY_DIR}/"
            COMMAND ${WINDEPLOYQT_EXECUTABLE}
            "${DEPLOY_DIR}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
            --no-translations
            --no-compiler-runtime
            --verbose 2
    )

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DLL_FILES}
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>" "${DEPLOY_DIR}/"
    )

    if(QT6_MULTIMEDIA_LOCATION)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QT6_MULTIMEDIA_LOCATION}
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>" "${DEPLOY_DIR}/"
        )
    endif()

elseif(PLATFORM_ANDROID)

else ()
    message("Update cmake code for new platform")
endif ()

qt_finalize_executable(${PROJECT_NAME})
