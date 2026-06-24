import QtQuick
import QtQuick.Layouts
import SoundLink 1.0
import "../Components"

Rectangle {
    id: root

    property bool expanded: true
    readonly property int expandedWidth:  Theme.metrics.sideBarExpanded
    readonly property int collapsedWidth: Theme.metrics.sideBarCollapsed
    readonly property bool compact: !expanded

    property string currentItem: "all_tracks"

    signal itemSelected(string id)
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

            // ─── My Library ───────────────────────────
            NavSection {
                id: librarySection
                Layout.fillWidth: true
                title: "My Library"
                iconName: ""
                compact: root.compact
                expanded: true

                NavItem {
                    Layout.fillWidth: true
                    iconName: "library"
                    text: "All Tracks"
                    compact: root.compact
                    selected: root.currentItem === "all_tracks"
                    onClicked: { root.currentItem = "all_tracks"; root.itemSelected("all_tracks") }
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "list"
                    text: "Playlists"
                    badge: "0"
                    compact: root.compact
                    selected: root.currentItem === "playlists"
                    onClicked: { root.currentItem = "playlists"; root.itemSelected("playlists") }
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "heart"
                    text: "Favorites"
                    badge: "0"
                    compact: root.compact
                    selected: root.currentItem === "favorites"
                    onClicked: { root.currentItem = "favorites"; root.itemSelected("favorites") }
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "clock"
                    text: "Recent"
                    compact: root.compact
                    selected: root.currentItem === "recent"
                    onClicked: { root.currentItem = "recent"; root.itemSelected("recent") }
                }
            }

            // Items rendered directly in compact mode (so users still see them)
            Item { Layout.fillWidth: true; height: 6; visible: !root.compact }

            // ─── Settings ─────────────────────────────
            NavSection {
                id: settingsSection
                Layout.fillWidth: true
                title: "Settings"
                iconName: "settings"
                compact: root.compact
                expanded: true

                NavItem {
                    Layout.fillWidth: true
                    iconName: "settings"
                    text: "General"
                    compact: root.compact
                    selected: root.currentItem === "general"
                    onClicked: { root.currentItem = "general"; root.itemSelected("general") }
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "audio"
                    text: "Audio"
                    compact: root.compact
                    selected: root.currentItem === "audio"
                    onClicked: { root.currentItem = "audio"; root.itemSelected("audio") }
                }
                NavItem {
                    Layout.fillWidth: true
                    iconName: "list"
                    text: "Mods"
                    compact: root.compact
                    selected: root.currentItem === "mods"
                    onClicked: { root.currentItem = "mods"; root.itemSelected("mods") }
                }
            }

            Item { Layout.fillWidth: true; height: 6; visible: !root.compact }

            // ─── About / Updates ──────────────────────
            NavSection {
                Layout.fillWidth: true
                title: "About"
                iconName: "info"
                compact: root.compact
                expanded: false
                onClicked: if (root.compact) { root.currentItem = "about"; root.itemSelected("about") }
            }
            NavSection {
                Layout.fillWidth: true
                title: "Updates"
                iconName: "download"
                compact: root.compact
                expanded: false
                showDot: true
                onClicked: if (root.compact) { root.currentItem = "updates"; root.itemSelected("updates") }
            }
        }
    }
}
