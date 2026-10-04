cmake_minimum_required(VERSION 3.22)

project(SoundLink VERSION 0.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Concurrent Gui Widgets Multimedia Network Qml Quick QuickControls2 WebSockets)

qt_standard_project_setup(REQUIRES 6.5)

# QTomlUtils (git submodule)
set(QTOMLUTILS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/QTomlUtils" CACHE PATH "Path to QTomlUtils sources")
if(NOT EXISTS "${QTOMLUTILS_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "QTomlUtils not found in ${QTOMLUTILS_DIR}. Run: git submodule update --init")
endif()
set(QTOMLUTILS_SHARED OFF)
add_subdirectory(${QTOMLUTILS_DIR} "${CMAKE_BINARY_DIR}/QTomlUtils")

qt_add_executable(SoundLink
        main.cpp

        # Framework
        core/public/framework/AppInstance.h
        core/private/framework/AppInstance.cpp
        core/public/framework/SubsystemBase.h
        core/private/framework/SubsystemBase.cpp
        core/public/framework/AppConfigs.h
        core/private/framework/AppConfigs.cpp
        core/public/framework/SystemMediaControls.h
        core/private/framework/SystemMediaControls.cpp
        core/private/framework/SmtcInterfaces.h

        # Library
        core/public/subsystems/Library/Track.h
        core/public/subsystems/Library/LibrarySubsystem.h
        core/private/subsystems/Library/LibrarySubsystem.cpp
        core/private/subsystems/Library/MetadataReader.h
        core/private/subsystems/Library/MetadataReader.cpp

        # Discord Rich Presence
        core/public/subsystems/Discord/DiscordSubsystem.h
        core/private/subsystems/Discord/DiscordSubsystem.cpp

        # Downloads
        core/public/subsystems/Downloads/DownloadSubsystem.h
        core/private/subsystems/Downloads/DownloadSubsystem.cpp

        # Playlists
        core/public/subsystems/Playlists/PlaylistSubsystem.h
        core/private/subsystems/Playlists/PlaylistSubsystem.cpp

        # Player
        core/public/subsystems/Player/PlayerSubsystem.h
        core/private/subsystems/Player/PlayerSubsystem.cpp

        # Online (yt-dlp search and streams)
        core/public/subsystems/Online/OnlineTypes.h
        core/public/subsystems/Online/OnlineSubsystem.h
        core/private/subsystems/Online/OnlineSubsystem.cpp
        core/private/subsystems/Online/OnlineBackend.h
        core/private/subsystems/Online/OnlineBackend.cpp
        core/private/subsystems/Online/YtDlpBackend.h
        core/private/subsystems/Online/YtDlpBackend.cpp
        core/private/subsystems/Online/InnerTubeBackend.h
        core/private/subsystems/Online/InnerTubeBackend.cpp
        core/private/subsystems/Online/BrowserAssistedBackend.h
        core/private/subsystems/Online/BrowserAssistedBackend.cpp
        core/public/subsystems/Online/BrowserBridge.h
        core/private/subsystems/Online/BrowserBridge.cpp

        # yt-dlp
        core/public/subsystems/YtDlp/YtDlpSubsystem.h
        core/private/subsystems/YtDlp/YtDlpSubsystem.cpp
        core/public/subsystems/YtDlp/FfmpegSubsystem.h
        core/private/subsystems/YtDlp/FfmpegSubsystem.cpp
        core/public/subsystems/YtDlp/DenoSubsystem.h
        core/private/subsystems/YtDlp/DenoSubsystem.cpp
        core/private/subsystems/YtDlp/ReleaseAssetDownload.h
        core/private/subsystems/YtDlp/ReleaseAssetDownload.cpp

        # UI
        ui/tray/TrayIcon.h
        ui/tray/TrayIcon.cpp
)

# No console window outside Debug builds; Debug keeps it for qDebug output.
set_target_properties(SoundLink PROPERTIES WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)

target_compile_definitions(SoundLink PRIVATE SOUNDLINK_VERSION="${PROJECT_VERSION}")

target_include_directories(SoundLink PRIVATE
        core/public/framework
        core/public/subsystems
        ui/tray
        ui/windowsUI/ViewModels
)

target_link_libraries(SoundLink PRIVATE
        Qt6::Core
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::WebSockets
        QTomlUtils::QTomlUtils
)

if(WIN32)
    target_link_libraries(SoundLink PRIVATE winmm ole32 user32 advapi32 runtimeobject)
endif()

# QML module "SoundLink": every file is a type of the module, Theme is its singleton
set(QML_DIR ui/windowsUI/Widgets)

qt_policy(SET QTP0004 NEW)

set_source_files_properties(${QML_DIR}/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(SoundLink
        URI SoundLink
        VERSION 1.0
        QML_FILES
        ${QML_DIR}/WindowsMainWindow.qml
        ${QML_DIR}/Theme.qml
        ${QML_DIR}/Screens/HomeScreen.qml
        ${QML_DIR}/Screens/SearchScreen.qml
        ${QML_DIR}/Screens/PlaylistsScreen.qml
        ${QML_DIR}/Screens/PlaylistScreen.qml
        ${QML_DIR}/Screens/DownloadsScreen.qml
        ${QML_DIR}/Screens/SettingsScreen.qml
        ${QML_DIR}/Screens/AboutScreen.qml
        ${QML_DIR}/Screens/UpdatesScreen.qml
        ${QML_DIR}/Sections/AppTitleBar.qml
        ${QML_DIR}/Sections/LibraryView.qml
        ${QML_DIR}/Sections/PlayerBar.qml
        ${QML_DIR}/Sections/ResizeHandle.qml
        ${QML_DIR}/Sections/SideBar.qml
        ${QML_DIR}/Sections/SearchSuggestions.qml
        ${QML_DIR}/Sections/TrackShelf.qml
        ${QML_DIR}/Components/AlbumArt.qml
        ${QML_DIR}/Components/AppIcon.qml
        ${QML_DIR}/Components/IconButton.qml
        ${QML_DIR}/Components/NavItem.qml
        ${QML_DIR}/Components/NavSection.qml
        ${QML_DIR}/Components/PlaybackButton.qml
        ${QML_DIR}/Components/PlayButton.qml
        ${QML_DIR}/Components/ProgressSlider.qml
        ${QML_DIR}/Components/SearchField.qml
        ${QML_DIR}/Components/TabPill.qml
        ${QML_DIR}/Components/TitleBarButton.qml
        ${QML_DIR}/Components/TrackRow.qml
        ${QML_DIR}/Components/TrackCard.qml
        ${QML_DIR}/Components/TrackMenu.qml
        ${QML_DIR}/Components/InputField.qml
        ${QML_DIR}/Components/Dropdown.qml
        ${QML_DIR}/Dialogs/ModalDialog.qml
        ${QML_DIR}/Dialogs/TextPromptDialog.qml
        ${QML_DIR}/Dialogs/ConfirmDialog.qml
        ${QML_DIR}/Dialogs/SmartPlaylistDialog.qml
        ${QML_DIR}/Components/ScreenHeader.qml
        ${QML_DIR}/Components/EmptyState.qml
        ${QML_DIR}/Components/TextButton.qml
        ${QML_DIR}/Components/ToggleSwitch.qml
        ${QML_DIR}/Components/SettingRow.qml
        SOURCES
        ui/windowsUI/ViewModels/Navigation.h
        ui/windowsUI/ViewModels/Navigation.cpp
        ui/windowsUI/ViewModels/SettingsCategories.h
        ui/windowsUI/ViewModels/SettingsModel.h
        ui/windowsUI/ViewModels/SettingsModel.cpp
        ui/windowsUI/ViewModels/TrackListModel.h
        ui/windowsUI/ViewModels/TrackListModel.cpp
        ui/windowsUI/ViewModels/PlaylistListModel.h
        ui/windowsUI/ViewModels/PlaylistListModel.cpp
        ui/windowsUI/ViewModels/LibraryViewModel.h
        ui/windowsUI/ViewModels/LibraryViewModel.cpp
        ui/windowsUI/ViewModels/PlayerViewModel.h
        ui/windowsUI/ViewModels/PlayerViewModel.cpp
        ui/windowsUI/ViewModels/AppInfoViewModel.h
        ui/windowsUI/ViewModels/AppInfoViewModel.cpp
        ui/windowsUI/ViewModels/SuggestionListModel.h
        ui/windowsUI/ViewModels/SuggestionListModel.cpp
        ui/windowsUI/ViewModels/SearchViewModel.h
        ui/windowsUI/ViewModels/SearchViewModel.cpp
        ui/windowsUI/ViewModels/OnlineViewModel.h
        ui/windowsUI/ViewModels/OnlineViewModel.cpp
        ui/windowsUI/ViewModels/PlaylistsViewModel.h
        ui/windowsUI/ViewModels/PlaylistsViewModel.cpp
        ui/windowsUI/ViewModels/DownloadsViewModel.h
        ui/windowsUI/ViewModels/DownloadsViewModel.cpp
)

qt_add_resources(SoundLink "icons"
        PREFIX "/icons"
        BASE ui/windowsUI/Resources
        FILES
        ui/windowsUI/Resources/ApplicationIcon/tray_16.png
        ui/windowsUI/Resources/ApplicationIcon/tray_20.png
        ui/windowsUI/Resources/ApplicationIcon/tray_24.png
        ui/windowsUI/Resources/ApplicationIcon/tray_32.png
        ui/windowsUI/Resources/ApplicationIcon/tray_40.png
        ui/windowsUI/Resources/ApplicationIcon/tray_48.png
        ui/windowsUI/Resources/ApplicationIcon/taskbar_20.png
        ui/windowsUI/Resources/ApplicationIcon/taskbar_24.png
        ui/windowsUI/Resources/ApplicationIcon/taskbar_32.png
        ui/windowsUI/Resources/ApplicationIcon/taskbar_40.png
        ui/windowsUI/Resources/ApplicationIcon/taskbar_48.png
        ui/windowsUI/Resources/ApplicationIcon/icon_512.png
)

# The browser extension (MusicPlayer/extension), unpacked on demand by OnlineViewModel::openExtensionFolder.
qt_add_resources(SoundLink "extension"
        PREFIX "/extension"
        BASE ../extension
        FILES
        ../extension/manifest.json
        ../extension/background.js
        ../extension/page.js
        ../extension/relay.js
        ../extension/popup.html
        ../extension/popup.js
)

# Updates, release packages and the bin/ runtime layout (QAppUpdater, git submodule).
# Builds Updater.exe next to SoundLink.exe; targets SoundLink_deploy and SoundLink_package make a standalone
# copy in MusicPlayer/dist and a release zip in MusicPlayer/packages (presets release-dist / release-package).
set(QAPPUPDATER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/QAppUpdater" CACHE PATH "Path to QAppUpdater sources")
if(NOT EXISTS "${QAPPUPDATER_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "QAppUpdater not found in ${QAPPUPDATER_DIR}. Run: git submodule update --init")
endif()
add_subdirectory(${QAPPUPDATER_DIR} "${CMAKE_BINARY_DIR}/QAppUpdater")

qau_setup(SoundLink
        NAME SoundLink
        GITHUB_REPO Stalker72742/MusicPlayer
        ICON ui/windowsUI/Resources/ApplicationIcon/AppIcon.ico
        ACCENT_COLOR "#ECECEC"
        DESCRIPTION "SoundLink music player"
        PROTECTED_PATHS Saved
        QML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${QML_DIR}"
        DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dist"
        PACKAGE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../packages"
)
