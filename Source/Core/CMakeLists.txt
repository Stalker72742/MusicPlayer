cmake_minimum_required(VERSION 3.16)
project(MusicPlayerCore)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

#find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core Multimedia)
set(CMAKE_PREFIX_PATH "D:/Qt/6.8.3/android_arm64_v8a")
set(Qt6_DIR "D:/Qt/6.8.3/android_arm64_v8a/lib/cmake/Qt6")
find_package(Qt6 COMPONENTS Core)

add_library(MusicPlayerCore SHARED Public/Data/songPath.h
        Public/Framework/AppInstance.h
        Private/Framework/AppInstance.cpp
        Public/Subsystems/ytSearcher.h
        Private/Subsystems/ytSearcher.cpp
        Public/Subsystems/Player/PlayerSubsystem.h
        Private/Subsystems/Player/PlayerSubsystem.cpp
        Public/Subsystems/Player/song.h
        Private/Subsystems/Player/song.cpp
        Private/Subsystems/Player/PlayerBackend/playerbase.cpp
        Private/Subsystems/Player/PlayerBackend/playerbase.h
        Public/Subsystems/Player/androidjavaplayer.h Public/Subsystems/Player/androidjavaplayer.cpp
)

target_link_libraries(MusicPlayerCore
        PUBLIC
        Qt6::Core
        Qt6::Multimedia
        Qt6::CorePrivate
)

target_include_directories(MusicPlayerCore
        PUBLIC
        Private/Framework
        Private/Libs
        Private/Subsystems
        Private/Subsystems/Player
        Public/Data
        Public/Framework
        Public/Libs
        Public/Subsystems
        Public/Subsystems/Player
)

if(PLATFORM_WINDOWS)
    target_link_libraries(MusicPlayerCore PRIVATE
            winmm
            ole32
            user32
            advapi32
    )

    target_compile_definitions(MusicPlayerCore
            PRIVATE
            MusicPlayerCore_EXPORTS
            QT_WIDGETS_LIB
            INTERFACE
            MusicPlayerCore_IMPORTS
    )

    set_target_properties(MusicPlayerCore PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS ON
    )

elseif(PLATFORM_ANDROID)
    target_link_libraries(MusicPlayerCore PRIVATE
            Qt6::Core
            Qt6::Multimedia
            android
            log
            OpenSLES
    )

    target_compile_definitions(MusicPlayerCore PRIVATE
            ANDROID_PLATFORM
            QT_ANDROID
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)
