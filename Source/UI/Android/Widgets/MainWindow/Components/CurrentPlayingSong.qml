import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    anchors.bottom: parent.bottom
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.leftMargin: 0
    anchors.topMargin: 0
    anchors.rightMargin: 0
    anchors.bottomMargin: 0

    color: "#2A2A2A"

    Column{
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 0
        anchors.topMargin: 0
        anchors.rightMargin: 0
        anchors.bottomMargin: 0

        spacing: 10

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: parent.width * 0.05
            anchors.rightMargin: parent.width * 0.02
            spacing: parent.width * 0.02

            Image {
                id: playingSongImage

                source: "qrc:/SharedResources/EmptyImage.png"

                Layout.alignment: Qt.AlignVCenter

                Layout.preferredHeight: (root.height * 0.315) * 2
                Layout.preferredWidth: Layout.preferredHeight
            }

            Column {
                id: playingSongTexts
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter

                Text {
                    id: playingSongName
                    width: parent.width

                    color: "#FFFFFF"
                    text: playerData.currentTrack
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }

                Text {
                    id: playingSongAuthor
                    width: parent.width

                    color: "#AAAAAA"
                    text: playerData.currentArtist
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }

            Button {
                id: previousSong
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: parent.width * 0.12
                Layout.preferredHeight: Layout.preferredWidth

                background: Image {
                    anchors.fill: parent

                    source: "qrc:/PlayerIcons/next.png"
                    rotation: 180
                    fillMode: Image.PreserveAspectFit
                }

                onClicked: {
                    playerData.previous()
                }
            }

            Button {
                id: playPause
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: parent.width * 0.12
                Layout.preferredHeight: Layout.preferredWidth

                background: Rectangle{

                    anchors.fill: parent
                    radius: width / 2

                    color: "#555555"

                    Image {

                        width: 20
                        height: 20
                        anchors.centerIn: parent
                        fillMode: Image.PreserveAspectFit

                        source: playerData.isPlaying ? "qrc:/PlayerIcons/Pause.png" : "qrc:/PlayerIcons/Play.png"
                    }
                }

                onClicked: {
                    playerData.playPause()
                }
            }

            Button {
                id: nextSong
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: parent.width * 0.12
                Layout.preferredHeight: Layout.preferredWidth

                background: Image {
                    anchors.fill: parent

                    source: "qrc:/PlayerIcons/next.png"
                    fillMode: Image.PreserveAspectFit
                }

                onClicked: {
                    playerData.next()
                }
            }
        }

        Slider {
            id: currentSongDuration

            anchors.fill: parent

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: parent.width * 0.05
            anchors.topMargin: parent.height * 0.80
            anchors.rightMargin: parent.width * 0.05
            anchors.bottomMargin: 0

            background: Rectangle {
                x: currentSongDuration.leftPadding
                y: currentSongDuration.topPadding + currentSongDuration.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 4
                width: currentSongDuration.availableWidth
                height: implicitHeight
                radius: 2
                color: "#444444"

                Rectangle {
                    width: currentSongDuration.visualPosition * parent.width
                    height: parent.height
                    color: "#FFFFFF"
                    radius: 2
                }
            }

            handle: Rectangle {

                x: currentSongDuration.leftPadding + currentSongDuration.visualPosition * (currentSongDuration.availableWidth - width)
                y: currentSongDuration.topPadding + currentSongDuration.availableHeight / 2 - height / 2

                implicitWidth: 14
                implicitHeight: 14
                radius: 7

                color: "#FFFFFF"
                border.color: "#FFFFFF"
            }
        }
    }
}
