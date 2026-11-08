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

    Binding {
        target: Theme
        property: "pixelRatio"
        value: root.Screen.devicePixelRatio
    }

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
    // Only the screen of the current location is alive, like a browser tab. Leaving a location stores the
    // screen's saveState() with it; a screen made for a location reads Navigation.pageState back.
    Item {
        id: contentArea
        anchors.top: titleBar.bottom
        anchors.bottom: playerBar.top
        anchors.left: sideBar.right
        anchors.right: parent.right

        // In the order of Navigation.Page.
        readonly property list<Component> screens: [
            homeScreen, tracksScreen, playlistsScreen, playlistScreen, favoritesScreen, recentScreen,
            downloadsScreen, searchScreen, onlinePageScreen, settingsScreen, aboutScreen, updatesScreen,
            collectionScreen
        ]

        function showLocation() {
            pageLoader.sourceComponent = null;
            pageLoader.sourceComponent = screens[Navigation.page];
        }

        Component.onCompleted: showLocation()

        Connections {
            target: Navigation
            function onSaveStateRequested() {
                const screen = pageLoader.item;
                if (screen && typeof screen.saveState === "function")
                    Navigation.storeState(screen.saveState());
            }
            function onLocationChanged() { contentArea.showLocation(); }
        }

        Loader {
            id: pageLoader
            objectName: "pageLoader"
            anchors.fill: parent
        }

        Component { id: homeScreen; HomeScreen {} }
        Component { id: tracksScreen; LibraryView {
            id: tracksView
            readonly property string scanText: LibraryViewModel.scanning
                ? " · Scanning…"
                : (LibraryViewModel.pendingMetadata > 0 ? " · Reading tags, " + LibraryViewModel.pendingMetadata + " left" : "")

            title: "My library"
            subtitle: LibraryViewModel.tracks.count + " tracks · " + CatalogViewModel.artists.count + " artists · "
                      + CatalogViewModel.albums.count + " albums" + scanText
            model: LibraryViewModel.tracks
            showTabs: true
            // Tab ids are "albums", "artists", "genres"; the grid takes the singular.
            tabContent: CatalogGrid { kind: tracksView.currentTab.slice(0, -1) }
            emptyTitle: LibraryViewModel.scanning ? "Scanning your music folders…" : "Your library is empty"
            emptyText: LibraryViewModel.scanning ? "" : "Add music folders in Settings › Library, or save tracks from YouTube."
        } }
        Component { id: playlistsScreen; PlaylistsScreen {} }
        Component { id: playlistScreen; PlaylistScreen {} }
        Component { id: favoritesScreen; LibraryView {
            title: "Favorites"
            subtitle: LibraryViewModel.favorites.count + " tracks"
            model: LibraryViewModel.favorites
            emptyIcon: "heart"
            emptyTitle: "No favorites yet"
            emptyText: "Tap the heart on a track to keep it here."
        } }
        Component { id: recentScreen; LibraryView {
            title: "Recent"
            subtitle: "Recently played"
            model: LibraryViewModel.recent
            emptyIcon: "clock"
            emptyTitle: "Nothing played yet"
            emptyText: "Tracks you play will show up here."
        } }
        Component { id: downloadsScreen; DownloadsScreen {} }
        Component { id: searchScreen; SearchScreen {} }
        Component { id: onlinePageScreen; OnlinePageScreen {} }
        Component { id: settingsScreen; SettingsScreen {} }
        Component { id: aboutScreen; AboutScreen {} }
        Component { id: updatesScreen; UpdatesScreen {} }
        Component { id: collectionScreen; CollectionScreen {} }
    }

    // ─── Properties of tracks: a window of its own ───────
    TrackPropertiesWindow { id: propertiesWindow }

    Connections {
        target: LibraryViewModel
        function onPropertiesRequested(trackIds) { propertiesWindow.openFor(trackIds); }
    }

    // ─── Search from anywhere: Ctrl+K, Ctrl+F ────────────
    Shortcut {
        sequences: ["Ctrl+K", StandardKey.Find]
        onActivated: titleBar.focusSearch()
    }

    // ─── Back and forward: mouse side buttons, Alt+Left / Alt+Right ─
    TapHandler {
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        onTapped: (eventPoint, button) => button === Qt.ForwardButton ? Navigation.goForward() : Navigation.goBack()
    }
    Shortcut {
        sequences: [StandardKey.Back]
        onActivated: Navigation.goBack()
    }
    Shortcut {
        sequences: [StandardKey.Forward]
        onActivated: Navigation.goForward()
    }

    // ─── Resize handle ───────────────────────────────────
    ResizeHandle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        z: 100
        window: root
    }

    // ─── Resize by any edge or corner ────────────────────
    WindowResizeFrame {
        anchors.fill: parent
        z: 101
        window: root
        topRightWidth: titleBar.buttonsWidth
        topRightHeight: titleBar.height
    }

    // ─── A YouTube link dragged from a browser ───────────
    LinkDropOverlay {
        anchors.fill: parent
        z: 102
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
