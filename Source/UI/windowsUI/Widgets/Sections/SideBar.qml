pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

Rectangle {
    id: root

    property bool expanded: true
    readonly property int expandedWidth:  Theme.metrics.sideBarExpanded
    readonly property int collapsedWidth: Theme.metrics.sideBarCollapsed
    readonly property bool compact: !expanded

    signal toggleRequested

    implicitWidth: expanded ? expandedWidth : collapsedWidth
    width: implicitWidth
    color: Theme.palette.sideBar
    clip: true

    Behavior on implicitWidth {
        NumberAnimation { duration: Theme.motion.durationNormal; easing.type: Theme.motion.easingType }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Theme.palette.hairline
    }

    // Top: collapse toggle row
    Item {
        id: toggleRow
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 12
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        height: 32

        IconButton {
            id: toggleBtn
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            implicitWidth: root.compact ? parent.width : 28
            implicitHeight: 28
            iconName: root.expanded ? "chevron-left" : "chevron-right"
            iconSize: 14
            onClicked: root.toggleRequested()

            Behavior on implicitWidth { NumberAnimation { duration: Theme.motion.durationNormal; easing.type: Theme.motion.easingType } }
        }

        Text {
            visible: !root.compact
            opacity: root.compact ? 0 : 1
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: toggleBtn.right
            anchors.leftMargin: 8
            text: "Collapse"
            color: Theme.palette.textSecondary
            font.family: Theme.typography.family
            font.pixelSize: Theme.typography.bodySize

            Behavior on opacity { NumberAnimation { duration: Theme.motion.durationFast } }
        }
    }

    // Sections content
    Flickable {
        id: scroller
        anchors.top: toggleRow.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 16
        anchors.bottomMargin: 12
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: column
            width: scroller.width
            spacing: 6

            // ─── Home (no section) ────────────────────
            NavSection {
                Layout.fillWidth: true
                title: "Home"
                iconName: "home"
                compact: root.compact
                selected: Navigation.page === Navigation.Home
                onClicked: Navigation.navigate(Navigation.Home)
            }

            Item { Layout.fillWidth: true; Layout.preferredHeight: 6; visible: !root.compact }

            // ─── My Library ───────────────────────────
            NavSection {
                Layout.fillWidth: true
                title: "My Library"
                compact: root.compact
                compactSeparator: false
                onClicked: Navigation.navigate(Navigation.Tracks)

                NavItem {
                    Layout.fillWidth: true
                    iconName: "library"
                    text: "All Tracks"
                    badge: String(LibraryViewModel.tracks.count)
                    compact: root.compact
                    selected: Navigation.page === Navigation.Tracks
                    onClicked: Navigation.navigate(Navigation.Tracks)
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "list"
                    text: "Playlists"
                    badge: String(PlaylistsViewModel.playlists.count)
                    compact: root.compact
                    selected: Navigation.page === Navigation.Playlists
                    onClicked: Navigation.navigate(Navigation.Playlists)
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "heart"
                    text: "Favorites"
                    badge: String(LibraryViewModel.favorites.count)
                    compact: root.compact
                    selected: Navigation.page === Navigation.Favorites
                    onClicked: Navigation.navigate(Navigation.Favorites)
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "download"
                    text: "Downloads"
                    badge: DownloadsViewModel.activeCount > 0 ? String(DownloadsViewModel.activeCount) : ""
                    compact: root.compact
                    selected: Navigation.page === Navigation.Downloads
                    onClicked: Navigation.navigate(Navigation.Downloads)
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "clock"
                    text: "Recent"
                    compact: root.compact
                    selected: Navigation.page === Navigation.Recent
                    onClicked: Navigation.navigate(Navigation.Recent)
                }
                // Back to the last YouTube results, once there were any.
                NavItem {
                    Layout.fillWidth: true
                    visible: SearchViewModel.onlineQuery.length > 0
                    iconName: "globe"
                    text: "YouTube results"
                    compact: root.compact
                    selected: Navigation.page === Navigation.Search
                    onClicked: Navigation.navigate(Navigation.Search)
                }
            }

            Item { Layout.fillWidth: true; Layout.preferredHeight: 6; visible: !root.compact }

            // ─── Playlists ────────────────────────────
            // Each playlist; the header opens the overview. Hidden while there are none.
            NavSection {
                id: playlistsSection
                visible: PlaylistsViewModel.playlists.count > 0
                Layout.fillWidth: true
                title: "Playlists"
                compact: root.compact
                selected: Navigation.page === Navigation.Playlist && !playlistsSection.expanded
                onClicked: Navigation.navigate(Navigation.Playlists)

                Repeater {
                    model: PlaylistsViewModel.playlists

                    NavItem {
                        required property string playlistId
                        required property string name
                        required property bool smart
                        required property int trackCount

                        Layout.fillWidth: true
                        iconName: smart ? "funnel" : "list"
                        text: name
                        badge: String(trackCount)
                        compact: root.compact
                        selected: Navigation.page === Navigation.Playlist && Navigation.playlistId === playlistId
                        onClicked: Navigation.openPlaylist(playlistId)
                    }
                }
            }

            Item { Layout.fillWidth: true; Layout.preferredHeight: 6; visible: !root.compact && playlistsSection.visible }

            // ─── Settings ─────────────────────────────
            // The header opens the last viewed category, an item jumps straight to its category.
            NavSection {
                id: settingsSection
                Layout.fillWidth: true
                title: "Settings"
                iconName: "settings"
                compact: root.compact
                // Highlight the header when the active category item is hidden.
                selected: Navigation.page === Navigation.Settings && !settingsSection.expanded
                onClicked: Navigation.openSettings()

                Repeater {
                    model: SettingsModel.categories

                    NavItem {
                        required property var modelData

                        Layout.fillWidth: true
                        iconName: modelData.icon
                        text: modelData.title
                        compact: root.compact
                        selected: Navigation.page === Navigation.Settings
                                  && Navigation.settingsCategory === modelData.id
                        onClicked: Navigation.openSettings(modelData.id)
                    }
                }
            }

            Item { Layout.fillWidth: true; Layout.preferredHeight: 6; visible: !root.compact }

            // ─── About / Updates ──────────────────────
            NavSection {
                Layout.fillWidth: true
                title: "About"
                iconName: "info"
                compact: root.compact
                selected: Navigation.page === Navigation.About
                onClicked: Navigation.navigate(Navigation.About)
            }
            NavSection {
                Layout.fillWidth: true
                title: "Updates"
                iconName: "download"
                compact: root.compact
                showDot: AppInfoViewModel.updateAvailable
                selected: Navigation.page === Navigation.Updates
                onClicked: Navigation.navigate(Navigation.Updates)
            }
        }
    }
}
