import QtQuick
import QtQuick.Controls
import QtQuick.Layouts 1.15
import "Components"
import Qt5Compat.GraphicalEffects

Window {
    color: "#121212"
    height: 960
    visible: true
    width: 540

    Rectangle {
        id: rectangle1

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.top: parent.top
        anchors.topMargin: 0
        color: "#121212"

        ColumnLayout {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: 0
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.top: parent.top
            anchors.topMargin: 0

            StackLayout {
                id: stackLayout

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: parent.height * 0.22

                currentIndex: 0

                Item {
                    id: search

                    Text {
                        id: searchMenuText

                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 18
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("What do you want to listen to?")
                        wrapMode: Text.WordWrap

                        anchors {
                            left: parent.left
                            leftMargin: 15
                            right: parent.right
                            rightMargin: 15
                            top: parent.top
                            topMargin: parent.height * 0.1
                        }
                    }
                    TextField {
                        id: textField

                        font.pixelSize: Math.min(parent.width, parent.height) * 0.04
                        height: Math.max(40, parent.height * 0.06)
                        placeholderText: qsTr("Search...")
                        placeholderTextColor: "#999999"

                        background: Rectangle {
                            border.color: "#282828"
                            border.width: 1
                            color: "#282828"
                            radius: 24
                        }

                        anchors {
                            left: parent.left
                            leftMargin: parent.width * 0.053
                            right: parent.right
                            rightMargin: parent.width * 0.053
                            top: searchMenuText.bottom
                            topMargin: parent.height * 0.04
                        }
                    }
                }
                Item {
                    id: mediaLibPage

                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    ScrollView {
                        anchors.bottomMargin: parent.height * 0.22
                        anchors.fill: parent
                        anchors.margins: 16

                        ListView {
                            id: playlistView

                            model: playlistModel
                            spacing: 16

                            delegate: Item {
                                readonly property string itemPlaylistName: model.playlistName

                                height: 90
                                width: parent.width

                                PlaylistWidget {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    color: "#2a2a2a"
                                    playlistName: itemPlaylistName

                                    onClicked: {
                                        playlistPage.playlistName = itemPlaylistName;

                                        stackLayout.currentIndex = 2;
                                    }
                                }
                            }

                            Component.onCompleted: {
                                playlistModel.refreshPlaylists();
                            }
                        }
                    }
                }   
                Item {
                    id: playlistPage

                    property string playlistName: "None"
                    property var songsModel: []

                    Layout.fillHeight: true
                    Layout.fillWidth: true

                    onPlaylistNameChanged: {
                        if (playlistName !== "None") {
                            songsModel = playlistModel.getTracksByPlaylistName(playlistName);
                        }
                    }

                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        anchors.fill: parent
                        spacing: 0

                        Button {
                            id: backButton

                            anchors.left: parent.left
                            anchors.leftMargin: parent.width * 0.02
                            anchors.top: parent.top
                            anchors.topMargin: parent.height * 0.025
                            text: "Back"

                            onClicked: {
                                playlistPage.playlistName = "None";
                                playlistPage.songsModel = [];
                                stackLayout.currentIndex = 1;
                            }
                        }

                        Rectangle {
                            id: playlistImage

                            Layout.preferredWidth: parent.width * 0.53
                            Layout.preferredHeight: parent.width * 0.53

                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: parent.height * 0.05

                            border.color: "#333333"
                            border.width: 1
                            color: "#2A2A2A"
                            radius: 15

                            Rectangle {
                                anchors.fill: parent

                                color: "#FFFFFF"

                                radius: 15
                                anchors.margins: parent.width * 0.05

                                Image {
                                    id: playingSongImage

                                    source: "qrc:/SharedResources/EmptyImage.png"
                                    anchors.fill: parent

                                    sourceSize.width: 130
                                    sourceSize.height: 130
                                }
                            }
                        }

                        Text {
                            id: playlistNameLabel

                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: playlistImage.bottom
                            anchors.topMargin: parent.height * 0.01
                            color: "#FFFFFF"
                            font.bold: true
                            font.pixelSize: 22
                            text: playlistPage.playlistName
                        }
                        Text {
                            id: trackCountLabel

                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: playlistNameLabel.bottom
                            anchors.topMargin: 5
                            color: "#999999"
                            font.pixelSize: 18
                            text: playlistPage.songsModel.length + " tracks"
                        }
                        ScrollView {
                            id: songsScrollView

                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: parent.height * 0.01
                            anchors.left: parent.left
                            anchors.margins: 16
                            anchors.right: parent.right
                            anchors.top: trackCountLabel.bottom
                            anchors.topMargin: 15
                            clip: true

                            ListView {
                                id: songsListView

                                model: playlistPage.songsModel
                                spacing: 10

                                delegate: PlaylistSong {
                                    songIndex: index
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
            CurrentPlayingSong {
                id: currentSong

                anchors.bottom: bottomButtonsRow.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: parent.height * 0.78
            }
            Row {
                id: bottomButtonsRow

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: parent.height * 0.91
                spacing: 10
                topPadding: 0

                Rectangle {
                    anchors.fill: parent
                    color: "#000000"
                    opacity: 1
                    z: -1
                }

                RowLayout {
                    anchors.fill: parent

                    Button {
                        id: searchButton

                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Layout.preferredHeight: Layout.preferredWidth
                        Layout.preferredWidth: parent.width * 0.12

                        background: ColumnLayout {
                            spacing: 5

                            Image {
                                id: searchIcon

                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 26
                                fillMode: Image.PreserveAspectFit
                                source: "qrc:/SharedResources/SearchSVG.svg"
                                visible: false

                                sourceSize.width: 26
                                sourceSize.height: 26

                                smooth: true
                                antialiasing: true
                                mipmap: true
                            }
                            ColorOverlay {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 26
                                color: stackLayout.currentIndex === 0 ? "#FFFFFF" : "#808080"
                                source: searchIcon
                            }
                            Label {
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                color: stackLayout.currentIndex === 0 ? "#FFFFFF" : "#808080"
                                font.pixelSize: 12
                                text: "Search"
                            }
                        }

                        onClicked: {
                            stackLayout.currentIndex = 0;
                        }
                    }
                    Button {
                        id: medialib

                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Layout.preferredHeight: Layout.preferredWidth
                        Layout.preferredWidth: parent.width * 0.12

                        background: ColumnLayout {
                            spacing: 5

                            Image {
                                id: medialibIcon

                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 26
                                fillMode: Image.PreserveAspectFit
                                source: "qrc:/SharedResources/Medialib.png"
                                visible: false

                                smooth: true
                                antialiasing: true
                                mipmap: true
                            }
                            ColorOverlay {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 26
                                color: stackLayout.currentIndex === 1 || stackLayout.currentIndex === 2 ? "#FFFFFF" : "#808080"
                                source: medialibIcon
                            }
                            Label {
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                color: stackLayout.currentIndex === 1 || stackLayout.currentIndex === 2 ? "#FFFFFF" : "#808080"
                                font.pixelSize: 12
                                text: "Medialib"
                            }
                        }

                        onClicked: {
                            stackLayout.currentIndex = 1;
                        }
                    }
                    Button {
                        id: settings

                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Layout.preferredHeight: Layout.preferredWidth
                        Layout.preferredWidth: parent.width * 0.12

                        background: ColumnLayout {
                            spacing: 5

                            Image {
                                id: settingsIcon

                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 26
                                fillMode: Image.PreserveAspectFit
                                source: "qrc:/SharedResources/Settings.png"
                                visible: false

                                smooth: true
                                antialiasing: true
                                mipmap: true
                            }
                            ColorOverlay {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 26
                                color: stackLayout.currentIndex === -5 ? "#FFFFFF" : "#808080"
                                source: settingsIcon
                            }
                            Label {
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                                color: stackLayout.currentIndex === -5 ? "#FFFFFF" : "#808080"
                                font.pixelSize: 12
                                text: "Settings"
                            }
                        }

                        onClicked: {}
                    }
                }
            }
        }
    }
}
