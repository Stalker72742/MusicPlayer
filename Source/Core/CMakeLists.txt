cmake_minimum_required(VERSION 3.16)
project(MusicPlayerCore)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Multimedia Widgets)

# Git submodule. Must stay a shared library so core and all plugins see one config registry.
set(QTOMLUTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/QTomlUtils" CACHE PATH "Path to QTomlUtils sources")
if(NOT EXISTS "${QTOMLUTILS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "QTomlUtils not found in ${QTOMLUTILS_DIR}. Run: git submodule update --init")
endif()
set(QTOMLUTILS_SHARED ON)
add_subdirectory(${QTOMLUTILS_DIR} "${CMAKE_BINARY_DIR}/QTomlUtils")

add_library(MusicPlayerCore SHARED Public/Subsystems/Player/Data/songPath.h
        # Framework
        Public/Framework/AppInstance.h
        Private/Framework/AppInstance.cpp
        Public/Framework/SubsystemBase.h
        Private/Framework/SubsystemBase.cpp
        public/framework/AppConfigs.h
        private/framework/AppConfigs.cpp

        # Player Subsystem
        Public/Subsystems/Player/PlayerSubsystem.h
        Private/Subsystems/Player/PlayerSubsystem.cpp
        Public/Subsystems/Player/PlayerBackend.h
        Private/Subsystems/Player/PlayerBackend/PlayerBackend.cpp
        Public/Subsystems/Player/SubObjects/song.h
        Private/Subsystems/Player/song.cpp
        Public/Subsystems/Player/SubObjects/playlist.cpp
        Public/Subsystems/Player/SubObjects/playlist.h

        # Other Subsystems
        Public/Subsystems/FileManager/FileManager.cpp
        Public/Subsystems/FileManager/FileManager.h
        Public/Subsystems/Settings/SettingsSubsystem.cpp
        Public/Subsystems/Settings/SettingsSubsystem.h
        Public/Subsystems/Settings/Data/SettingData.h
        Public/Subsystems/Settings/Data/SettingType.h

        # Data
        Public/Framework/PluginLoader.cpp
        Public/Framework/PluginLoader.h
        public/subsystems/eventDispatcher/EventDispatcher.cpp
        public/subsystems/eventDispatcher/EventDispatcher.h
        public/subsystems/modSubsystem/PluginFactory.cpp
        public/subsystems/modSubsystem/PluginFactory.h
        public/data/trackInfo.h
)

target_link_libraries(MusicPlayerCore
        PUBLIC
        QTomlUtils::QTomlUtils
        PRIVATE
        Qt6::Core
        Qt6::Multimedia
        Qt6::CorePrivate
        Qt6::Network
        Qt6::Widgets
)

target_include_directories(MusicPlayerCore
        PUBLIC
        Public/Data
        Public/Framework
        Public/Libs
        Public/Subsystems
        Public/Subsystems/Player
)

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
        MYLIB_BUILD
        INTERFACE
        MusicPlayerCore_IMPORTS
)

set_target_properties(MusicPlayerCore PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)
