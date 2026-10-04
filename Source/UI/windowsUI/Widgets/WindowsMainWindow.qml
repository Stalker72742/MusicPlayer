import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SoundLink 1.0

ApplicationWindow {
    id: root

    title: "SoundLink"
    width: 1600
    height: 900
    minimumWidth: 720
    minimumHeight: 460
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    // ─── Background gradient layer ───────────────────────
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.palette.windowTop }
            GradientStop { position: 1.0; color: Theme.palette.windowBottom }
        }
    }

    // ─── Title bar ───────────────────────────────────────
    AppTitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        window: root
        maximized: root.visibility === Window.Maximized
    }

    // ─── Bottom player bar ───────────────────────────────
    // Hidden until something has been played; a paused track keeps it visible.
    PlayerBar {
        id: playerBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: PlayerViewModel.hasTrack ? implicitHeight : 0
        visible: height > 0
        clip: true

        Behavior on height { NumberAnimation { duration: Theme.motion.durationNormal; easing.type: Theme.motion.easingType } }
    }

    // ─── Side bar ────────────────────────────────────────
    SideBar {
        id: sideBar
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.bottom: playerBar.top
        expanded: true
        onToggleRequested: expanded = !expanded
    }

    // ─── Main content ────────────────────────────────────
    // Children follow the order of Navigation.Page.
    StackLayout {
        id: contentArea
        anchors.top: titleBar.bottom
        anchors.bottom: playerBar.top
        anchors.left: sideBar.right
        anchors.right: parent.right
        currentIndex: Navigation.page

        HomeScreen {}
        LibraryView {
            readonly property string scanText: LibraryViewModel.scanning
                ? " · Scanning…"
                : (LibraryViewModel.pendingMetadata > 0 ? " · Reading tags, " + LibraryViewModel.pendingMetadata + " left" : "")

            title: "My library"
            subtitle: LibraryViewModel.tracks.count + " tracks · " + LibraryViewModel.albumCount + " albums" + scanText
            model: LibraryViewModel.tracks
            showTabs: true
            emptyTitle: LibraryViewModel.scanning ? "Scanning your music folders…" : "Your library is empty"
            emptyText: LibraryViewModel.scanning ? "" : "Add music folders in Settings › Library, or save tracks from YouTube."
        }
        PlaylistsScreen {}
        PlaylistScreen {}
        LibraryView {
            title: "Favorites"
            subtitle: LibraryViewModel.favorites.count + " tracks"
            model: LibraryViewModel.favorites
            emptyIcon: "heart"
            emptyTitle: "No favorites yet"
            emptyText: "Tap the heart on a track to keep it here."
        }
        LibraryView {
            title: "Recent"
            subtitle: "Recently played"
            model: LibraryViewModel.recent
            emptyIcon: "clock"
            emptyTitle: "Nothing played yet"
            emptyText: "Tracks you play will show up here."
        }
        DownloadsScreen {}
        SearchScreen {}
        SettingsScreen {}
        AboutScreen {}
        UpdatesScreen {}
    }

    // ─── Search from anywhere: Ctrl+K, Ctrl+F ────────────
    Shortcut {
        sequences: ["Ctrl+K", StandardKey.Find]
        onActivated: titleBar.focusSearch()
    }

    // ─── Back navigation: mouse back button, Alt+Left ────
    TapHandler {
        acceptedButtons: Qt.BackButton
        onTapped: Navigation.goBack()
    }
    Shortcut {
        sequences: [StandardKey.Back]
        onActivated: Navigation.goBack()
    }

    // ─── Resize handle ───────────────────────────────────
    ResizeHandle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 100
        window: root
    }

    // Hides to the tray, or quits when "Close to tray" is off.
    onClosing: function(close) {
        close.accepted = false
        if (SettingsModel.valueOf("general.closeToTray"))
            root.hide()
        else
            Qt.quit()
    }
}
