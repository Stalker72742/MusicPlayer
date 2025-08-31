import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    color: "#212121"
    anchors.fill: parent

    ListView {
        id: listView
        anchors.fill: parent
        model: musicScanner.files

        delegate: Rectangle {
            height: 50
            color: index % 2 === 0 ? "#2c2c2c" : "#1e1e1e"
            width: parent.width

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Label {
                    text: modelData.split("/").pop() // имя файла
                    color: "white"
                    width: parent.width - 100
                    elide: Text.ElideRight
                }

                Button {
                    text: "▶"
                    onClicked: {
                        audio.source = modelData
                        audio.play()
                    }
                }
            }
        }
    }
}
