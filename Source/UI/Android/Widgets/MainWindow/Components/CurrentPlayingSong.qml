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

                source: "qrc:/Resources/EmptyImage.png"

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
                    elide: Text.ElideRight
                }

                Text {
                    id: playingSongAuthor
                    width: parent.width

                    color: "#AAAAAA"
                    text: ""
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

                    source: "qrc:/Resources/next.png"
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

                    source: "qrc:/Resources/next.png"
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
            anchors.leftMargin: 0
            anchors.topMargin: parent.height * 0.80
            anchors.rightMargin: 0
            anchors.bottomMargin: 0

            handle: Rectangle {

                x: currentSongDuration.leftPadding + currentSongDuration.visualPosition * (currentSongDuration.availableWidth - width)
                y: currentSongDuration.topPadding + currentSongDuration.availableHeight / 2 - height / 2

                implicitWidth: 14
                implicitHeight: 14
                radius: 7

                color: currentSongDuration.pressed ? "#f0f0f0" : "#f6f6f6"
                border.color: "#bdbebf"
            }
        }
    }
}
