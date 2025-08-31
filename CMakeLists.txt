cmake_minimum_required(VERSION 3.16...3.24)
set(PROJECT_NAME "MusicPlayer")
project(${PROJECT_NAME} VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

set(Qt6_DIR "C:/Qt/6.8.3/android_arm64_v8a/lib/cmake/Qt6")
set(CMAKE_PREFIX_PATH "D:/Qt/6.8.3/android_arm64_v8a")
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Gui Qml Quick)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_subdirectory(Source/Core)

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

    qt_add_executable(${PROJECT_NAME} MANUAL_FINALIZATION main.cpp
            android/AndroidManifest.xml
            android/build.gradle
            android/res/values/libs.xml
            android/res/xml/qtprovider_paths.xml
            android/gradle/wrapper/gradle-wrapper.jar
            android/gradle/wrapper/gradle-wrapper.properties
            android/gradle.properties android/gradlew android/gradlew.bat
            android/src/com/example/MusicPlayer/MainActivity.java
            android/src/com/example/MusicPlayer/MediaButtonReceiver.java
            android/src/com/example/MusicPlayer/MediaSessionHandler.java
            Source/UI/Android/androidmainwindow.cpp
            Source/UI/Android/androidmainwindow.h
            Source/UI/Android/androidmainwindow.ui
            Source/UI/Android/CurrentPlayingSong/currentplayingsong.cpp
            Source/UI/Android/CurrentPlayingSong/currentplayingsong.h
            Source/UI/Android/CurrentPlayingSong/currentplayingsong.ui
            Source/UI/Android/MediaKeyHandler/mediakeyhandler.cpp
            Source/UI/Android/MediaKeyHandler/mediakeyhandler.h
            Source/UI/Android/MediaLib/medialibitemwidget.cpp
            Source/UI/Android/MediaLib/medialibitemwidget.h
            Source/UI/Android/MediaLib/medialibitemwidget.ui
            Source/UI/Android/MediaLib/playlistsong.cpp
            Source/UI/Android/MediaLib/playlistsong.h
            Source/UI/Android/MediaLib/playlistsong.ui
            Source/UI/Android/PermissionsHandler/permissionHandler.cpp
            Source/UI/Android/PermissionsHandler/permissionHandler.h
            qml.qrc
            android/src/com/example/MusicPlayer/NativeAudioPlayer.java
            android/src/com/example/MusicPlayer/JavaMusicPlayer.java
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
            QT_ANDROID_MIN_SDK_VERSION 21
            QT_ANDROID_TARGET_SDK_VERSION 34
    )

    target_include_directories(${PROJECT_NAME}
            PRIVATE
            Source/Core/Private/Framework
            Source/Core/Private/Libs
            Source/Core/Private/Subsystems
            Source/Core/Private/Subsystems/Player
            Source/Core/Public/Data
            Source/Core/Public/Framework
            Source/Core/Public/Libs
            Source/Core/Public/Subsystems
            Source/Core/Public/Subsystems/Player
            PUBLIC
            "Source/Libs/ffmpeg/build/arm64-v8a/include"
            "Source/Libs/Oboe/include"
            "Source/UI/Platforms/Android"
            "Source/UI/Platforms/Android/CurrentPlayingSong"
            "Source/UI/Platforms/Android/MediaKeyHandler"
            "Source/UI/Platforms/Android/MediaLib"
            "Source/UI/Platforms/Android/Permissions"
    )

    target_link_libraries(${PROJECT_NAME}
            PRIVATE
            MusicPlayerCore
            Qt::Core
            Qt6::CorePrivate
            Qt6::Widgets
            Qt6::Gui
            Qt6::Qml
            Qt6::Quick
    )
endif ()

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(${PROJECT_NAME})

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

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/android-build/libs/${ANDROID_ABI}"

            COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_BINARY_DIR}/lib${PROJECT_NAME}_${ANDROID_ABI}.so"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>" "${CMAKE_CURRENT_BINARY_DIR}/android-build/libs/${ANDROID_ABI}"

            COMMAND D:/Qt/6.8.3/mingw_64/bin/androiddeployqt.exe
            --input ${CMAKE_BINARY_DIR}/android-${PROJECT_NAME}-deployment-settings.json
            --output ${CMAKE_BINARY_DIR}/android-build
            --android-platform android-34
            --jdk "C:/Program Files/Java/jdk-23"
            --gradle
            --install
            --device RF8X20TNVCR
            --apk
            --verbose
            DEPENDS ${PROJECT_NAME}
    )

else ()
    message("Update cmake code for new platform")
endif ()