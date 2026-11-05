pragma ComponentBehavior: Bound

import QtQuick
import SoundLink 1.0

// Playlists or channels found by the search; a click opens the page.
Item {
    id: root

    readonly property int listState: SearchViewModel.collectionsState

    ListView {
        id: list
        anchors.fill: parent
        anchors.topMargin: 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: SearchViewModel.collections

        delegate: Rectangle {
            id: row

            required property var modelData

            width: list.width
            height: 64
            radius: Theme.metrics.radiusMd
            color: hover.hovered ? Theme.palette.hoverOverlay : "transparent"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: OnlinePageViewModel.openLink(row.modelData.url)
            }

            AlbumArt {
                id: art
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 48; height: 48
                source: row.modelData.thumbnail
            }

            Column {
                anchors.left: art.right
                anchors.leftMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    text: row.modelData.title
                    color: Theme.palette.textPrimary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.bodySize
                    font.weight: Font.DemiBold
                }
                Text {
                    width: parent.width
                    elide: Text.ElideRight
                    text: row.modelData.subtitle
                    color: Theme.palette.textSecondary
                    font.family: Theme.typography.family
                    font.pixelSize: Theme.typography.smallSize
                }
            }

            HoverHandler { id: hover }
        }
    }

    EmptyState {
        anchors.fill: parent
        visible: list.count === 0
        iconName: root.listState === SearchViewModel.Failed ? "info" : "globe"
        title: root.listState === SearchViewModel.Failed ? "Search failed"
             : root.listState === SearchViewModel.Done ? "Nothing found" : "Searching YouTube…"
        text: root.listState === SearchViewModel.Failed ? SearchViewModel.collectionsError : ""
    }
}
