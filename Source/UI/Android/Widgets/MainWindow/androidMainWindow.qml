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

                anchors.bottom: currentSong.top
                anchors.bottomMargin: parent.height * 0.12
                anchors.left: parent.left
                anchors.leftMargin: 0
                anchors.right: parent.right
                anchors.rightMargin: 0
                anchors.top: parent.top
                anchors.topMargin: 0
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
                        anchors.fill: parent
                        anchors.margins: 16

                        ListView {
                            id: playlistView

                            model: playlistModel

                            spacing: 16

                            delegate: Item {
                                height: 90
                                width: parent.width

                                readonly property string itemPlaylistName: model.playlistName

                                PlaylistWidget{
                                    anchors.fill: parent
                                    anchors.margins: 5

                                    playlistName: itemPlaylistName
                                    color: "#2a2a2a"

                                    onClicked: {
                                        playlistPage.playlistName = itemPlaylistName

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

                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    anchors.fill: parent

                    ColumnLayout {
                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        anchors.fill: parent

                        Button {
                            anchors.left: parent.left
                            anchors.leftMargin: parent.width * 0.02
                            anchors.top: parent.top
                            anchors.topMargin: parent.height * 0.025
                            text: "Back"

                            onClicked: {
                                stackLayout.currentIndex = 1;
                            }
                        }
                        Rectangle {
                            id: playlistImage

                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: parent.height * 0.09
                            border.color: "#333333"
                            border.width: 1
                            color: "#2A2A2A"
                            height: 200
                            radius: 15
                            width: 200

                            Rectangle {
                                anchors.centerIn: parent
                                color: "#FFFFFF"
                                height: 175
                                radius: 15
                                width: 175
                            }
                        }
                        Text {
                            id: playlistNameLabel

                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: playlistImage.bottom
                            anchors.topMargin: parent.height * 0.015
                            color: "#FFFFFF"
                            text: playlistPage.playlistName
                        }
                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }

            CurrentPlayingSong{
                id: currentSong

                anchors.bottom: row.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: parent.height * 0.78
            }

            Row {
                id: row

                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: parent.height * 0.91
                spacing: 10
                topPadding: 0

                Rectangle {
                    anchors.fill: parent
                    color: "#1A1A1A"
                    z: -1
                }

                RowLayout {
                    anchors.fill: parent

                    Button {
                        id: searchButton

                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        //bottomPadding: 10
                        //height: 50
                        //leftPadding: 20
                        //rightPadding: 20
                        //topPadding: 10
                        transformOrigin: Item.Center
                        //width: 250

                        Layout.preferredWidth: parent.width * 0.14
                        Layout.preferredHeight: Layout.preferredWidth

                        background: ColumnLayout {

                            spacing: 5

                            Image {
                                id: searchIcon

                                source: "qrc:/Resources/Search.png"

                                fillMode: Image.PreserveAspectFit
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                visible: false
                            }

                            ColorOverlay {

                                    source: searchIcon
                                    color: stackLayout.currentIndex === 0 ? "#FFFFFF" : "#808080"

                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                            }

                            Label {

                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                                color: stackLayout.currentIndex === 0 ? "#FFFFFF" : "#808080"
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
                        //bottomPadding: 10
                        //height: 50
                        //leftPadding: 20
                        //rightPadding: 20
                        //topPadding: 10
                        //width: 250

                        Layout.preferredWidth: parent.width * 0.14
                        Layout.preferredHeight: Layout.preferredWidth

                        background: ColumnLayout {

                            spacing: 5

                            Image {
                                id: medialibIcon

                                source: "qrc:/Resources/Medialib.png"

                                fillMode: Image.PreserveAspectFit
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                visible: false
                            }

                            ColorOverlay {

                                    source: medialibIcon
                                    color: stackLayout.currentIndex === 1 || stackLayout.currentIndex === 2  ? "#FFFFFF" : "#808080"

                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                            }

                            Label {

                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                                color: stackLayout.currentIndex === 1 || stackLayout.currentIndex === 2  ? "#FFFFFF" : "#808080"
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
                        //bottomPadding: 10
                        //height: 50
                        //leftPadding: 20
                        //rightPadding: 20
                        //topPadding: 10
                        //width: 250

                        Layout.preferredWidth: parent.width * 0.14
                        Layout.preferredHeight: Layout.preferredWidth

                        background: ColumnLayout {

                            spacing: 5

                            Image {
                                id: settingsIcon

                                source: "qrc:/Resources/Settings.png"

                                fillMode: Image.PreserveAspectFit
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                visible: false
                            }

                            ColorOverlay {

                                    source: settingsIcon
                                    color: stackLayout.currentIndex === -5 ? "#FFFFFF" : "#808080"

                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                            }

                            Label {

                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                                color: stackLayout.currentIndex === -5 ? "#FFFFFF" : "#808080"
                                text: "Settings"
                            }
                        }

                        onClicked: {

                        }
                    }
                }
            }
        }
    }
}
