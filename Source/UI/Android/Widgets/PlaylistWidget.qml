import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: 800
    height: 150

    signal clicked()

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
    }

    RowLayout {
        id: row
        anchors.fill: parent
        uniformCellSizes: false
        spacing: 0

        Item {
            id: item1
            width: 15

            height: 25
            anchors.verticalCenter: parent.verticalCenter
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillHeight: false
            Layout.columnSpan: 0
        }

        Rectangle{
            color: "#4A4A4A"
            width: 65
            height: 65
            anchors.verticalCenter: parent.verticalCenter
            Layout.fillWidth: false

            Image {
                id: name
                width: 65
                height: 65

            }
        }

        Item {
            width: 15

            height: 25
            anchors.verticalCenter: parent.verticalCenter
        }

        Column{
            anchors.verticalCenter: parent.verticalCenter
            Layout.fillWidth: false
            spacing: 5

            Label{
                text: "Liblary"
                font.pointSize: 18
                font.styleName: "Bold"

            }

            Label{
                text: "All songs : 247 songs"
                font.pointSize: 12

            }
        }

        Item {
            id: item2
            width: 15

            height: 25
            anchors.verticalCenter: parent.verticalCenter
            Layout.fillWidth: true
        }

        ToolBar{
            width: 50
            height: 50
            position: ToolBar.Header
            anchors.verticalCenter: parent.verticalCenter
        }

        Item {
            width: 5

            height: 25
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
