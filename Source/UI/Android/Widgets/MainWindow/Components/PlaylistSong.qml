import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Rectangle {
    id: root

    property string songTitle: "Unknown"
    property string songPath: ""
    property int songIndex: 0

    signal clicked()

    width: parent ? parent.width : 300
    height: 60
    color: "#2A2A2A"
    radius: 12

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 12

        Rectangle {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            color: "#4A4A4A"
            radius: 8

            Text {
                anchors.centerIn: parent
                text: (songIndex + 1).toString()
                color: "#FFFFFF"
                font.pixelSize: 14
                font.bold: true
            }
        }

        Text {
            Layout.fillWidth: true
            text: songTitle
            color: "#FFFFFF"
            font.pixelSize: 15
            font.bold: true
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        Item {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30

            Text {
                anchors.centerIn: parent
                text: "▶"
                color: "#1DB954"
                font.pixelSize: 18
            }
        }
    }
}
