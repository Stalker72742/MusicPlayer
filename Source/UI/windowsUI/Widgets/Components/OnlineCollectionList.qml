pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
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
        ScrollBar.vertical: AppScrollBar {}
        model: SearchViewModel.collections

        delegate: Rectangle {
            id: row

            required property var modelData

            width: list.width - 12
            height: 64
            radius: Theme.metrics.radiusMd
            color: hover.hovered ? Theme.palette.hoverOverlay : "transparent"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    const url = row.modelData.url;
                    if (mouse.button === Qt.LeftButton) {
                        OnlinePageViewModel.openLink(url);
                        return;
                    }
                    collectionMenu.openAt(row.mapToItem(null, mouse.x, mouse.y), [
                        { text: "Open", icon: "list", action: () => OnlinePageViewModel.openLink(url) },
                        { text: "Open in browser", icon: "external", action: () => Qt.openUrlExternally(url) },
                        { text: "Copy link", icon: "link", action: () => LibraryViewModel.copyText(url) }
                    ]);
                }
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

    ContextMenu { id: collectionMenu }

    EmptyState {
        anchors.fill: parent
        visible: list.count === 0
        iconName: root.listState === SearchViewModel.Failed ? "info" : "globe"
        title: root.listState === SearchViewModel.Failed ? "Search failed"
             : root.listState === SearchViewModel.Done ? "Nothing found" : "Searching YouTube…"
        text: root.listState === SearchViewModel.Failed ? SearchViewModel.collectionsError : ""
    }
}
