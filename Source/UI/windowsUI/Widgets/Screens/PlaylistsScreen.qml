pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import SoundLink 1.0

// All playlists as cards; new ones are made here.
Item {
    id: root

    readonly property PlaylistListModel playlists: PlaylistsViewModel.playlists

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 56
        anchors.rightMargin: 32
        anchors.topMargin: 32
        spacing: 16

        ScreenHeader {
            Layout.fillWidth: true
            title: "Playlists"
            subtitle: root.playlists.count === 1 ? "1 playlist" : root.playlists.count + " playlists"
        }

        Row {
            spacing: 8

            TextButton {
                primary: true
                text: "New playlist"
                onClicked: newDialog.ask("New playlist", "", (name) => PlaylistsViewModel.open(PlaylistsViewModel.createPlaylist(name)))
            }
            TextButton {
                text: "New smart playlist"
                onClicked: smartDialog.openNew()
            }
        }

        GridView {
            id: grid
            visible: root.playlists.count > 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 8
            clip: true
            cellWidth: 196
            cellHeight: 248
            boundsBehavior: Flickable.StopAtBounds
            model: root.playlists

            delegate: Item {
                id: card

                required property string playlistId
                required property string name
                required property bool smart
                required property int trackCount
                required property color artTint
                required property string artUrl

                width: grid.cellWidth
                height: grid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 6
                    radius: Theme.metrics.radiusLg
                    color: hover.hovered ? Theme.palette.hoverOverlay : "transparent"

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        AlbumArt {
                            width: parent.width
                            height: width
                            cornerRadius: Theme.metrics.radiusMd
                            tint: card.artTint
                            source: card.artUrl

                            // Smart and empty playlists show their kind instead of a cover.
                            AppIcon {
                                anchors.centerIn: parent
                                visible: card.artUrl.length === 0
                                width: 40; height: 40
                                name: card.smart ? "funnel" : "list"
                                color: Qt.rgba(1, 1, 1, 0.7)
                            }
                            Rectangle {
                                visible: card.smart && card.artUrl.length > 0
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 8
                                width: 26; height: 26
                                radius: 13
                                color: Qt.rgba(0, 0, 0, 0.6)

                                AppIcon {
                                    anchors.centerIn: parent
                                    width: 14; height: 14
                                    name: "funnel"
                                    color: "white"
                                }
                            }
                        }
                        Text {
                            width: parent.width
                            elide: Text.ElideRight
                            text: card.name
                            color: Theme.palette.textPrimary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.bodySize
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: (card.smart ? "Smart · " : "") + (card.trackCount === 1 ? "1 track" : card.trackCount + " tracks")
                            color: Theme.palette.textSecondary
                            font.family: Theme.typography.family
                            font.pixelSize: Theme.typography.smallSize
                        }
                    }

                    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: PlaylistsViewModel.open(card.playlistId) }
                }
            }
        }

        EmptyState {
            visible: root.playlists.count === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            iconName: "list"
            title: "No playlists yet"
            text: "A playlist holds the tracks you add to it. A smart playlist fills itself by rules: artist, tags, plays and more."
        }
    }

    SmartPlaylistDialog { id: smartDialog }
    TextPromptDialog {
        id: newDialog
        acceptText: "Create"
        placeholder: "Playlist name"
    }
}
