import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.15
import "Components"
import Qt5Compat.GraphicalEffects

Item {
    id: selectedPlaylistWidget

    property string playlistName: "Playlist Name"
    property var songsModel: []

    signal backButtonClicked()

    Rectangle {
        anchors.fill: parent
        color: "#121212"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Back button in top left corner
            Button {
                id: backButton

                Layout.preferredWidth: 60
                Layout.preferredHeight: 60
                anchors.left: parent.left
                anchors.leftMargin: parent.width * 0.02
                anchors.top: parent.top
                anchors.topMargin: parent.height * 0.025

                background: Rectangle {
                    color: "transparent"

                    // TODO: Add back arrow icon here
                    // Need: back_arrow.png or similar asset
                    // Purpose: Navigation back to playlist library
                    Rectangle {
                        anchors.centerIn: parent
                        width: 40
                        height: 40
                        color: "#333333"
                        radius: 20

                        Text {
                            anchors.centerIn: parent
                            text: "←"
                            color: "#FFFFFF"
                            font.pixelSize: 24
                        }
                    }
                }

                onClicked: {
                    selectedPlaylistWidget.backButtonClicked();
                }
            }

            // Playlist cover image
            Rectangle {
                id: playlistCoverImage

                Layout.preferredWidth: 250
                Layout.preferredHeight: 250
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: parent.height * 0.09

                border.color: "#333333"
                border.width: 2
                color: "#2A2A2A"
                radius: 15

                // Inner white/gray circle design (similar to screenshot)
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.85
                    height: parent.height * 0.85
                    color: "#E5E5E5"
                    radius: 12

                    // Outer gray ring
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.7
                        height: parent.height * 0.7
                        color: "#CCCCCC"
                        radius: width / 2

                        // Inner darker circle
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width * 0.4
                            height: parent.height * 0.4
                            color: "#666666"
                            radius: width / 2
                        }
                    }
                }

                // TODO: Replace with actual album/playlist cover image
                // Need: Ability to load playlist cover art
                // Purpose: Display playlist artwork/thumbnail
            }

            // Playlist name
            Text {
                id: playlistNameLabel

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 20

                color: "#FFFFFF"
                font.bold: true
                font.pixelSize: 24
                text: selectedPlaylistWidget.playlistName
            }

            // Control buttons row (shuffle, play, menu)
            RowLayout {
                id: controlButtonsRow

                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 30
                spacing: 30

                // Shuffle button
                Button {
                    id: shuffleButton

                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 60

                    background: Rectangle {
                        color: "#333333"
                        radius: 30

                        // TODO: Add shuffle icon
                        // Need: shuffle_icon.png
                        // Purpose: Shuffle playlist playback
                        Text {
                            anchors.centerIn: parent
                            text: "🔀"
                            font.pixelSize: 24
                        }
                    }

                    onClicked: {
                        console.log("Shuffle clicked");
                    }
                }

                // Play button (larger, centered)
                Button {
                    id: playButton

                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 80

                    background: Rectangle {
                        color: "#404040"
                        radius: 40

                        // TODO: Add play/pause icon
                        // Need: play_icon.png and pause_icon.png
                        // Purpose: Play/pause playlist
                        Text {
                            anchors.centerIn: parent
                            text: "▶"
                            color: "#FFFFFF"
                            font.pixelSize: 32
                        }
                    }

                    onClicked: {
                        console.log("Play clicked");
                    }
                }

                // Menu button (three dots)
                Button {
                    id: menuButton

                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 60

                    background: Rectangle {
                        color: "#333333"
                        radius: 30

                        // Three dots menu icon
                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Repeater {
                                model: 3
                                Rectangle {
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: "#FFFFFF"
                                }
                            }
                        }
                    }

                    onClicked: {
                        console.log("Menu clicked");
                    }
                }
            }

            // Songs list
            ScrollView {
                id: songsScrollView

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 30
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16

                clip: true

                ListView {
                    id: songsListView

                    model: selectedPlaylistWidget.songsModel
                    spacing: 10

                    delegate: PlaylistSong {
                        songIndex: index + 1
                        songPath: modelData.path || ""
                        songTitle: modelData.title || "Unknown"
                        width: songsListView.width

                        onClicked: {
                            console.log("Playing: " + songTitle + " from " + songPath);
                        }
                    }
                }
            }
        }
    }
}
