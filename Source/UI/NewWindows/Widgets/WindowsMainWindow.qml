import QtQuick
import QtQuick.Layouts 1.15
import QtQuick.Controls

Window {
    id: root

    readonly property real baseHeight: 900
    readonly property real baseWidth: 1600
    readonly property real scaleX: width / baseWidth
    readonly property real scaleY: height / baseHeight

    color: "#121212"
    height: 900
    visible: true
    width: 1600

    Rectangle {
        id: bottomPanel

        color: "#161616"
        height: 90 * root.scaleY

        anchors {
            bottom: parent.bottom
            left: parent.left
            right: parent.right
        }
    }

    Rectangle {
        id: leftPanel

        color: "#111111"

        width: 220 * root.scaleX
        anchors {
            bottom: bottomPanel.top
            left: parent.left
            top: parent.top
        }

        ColumnLayout {

            RowLayout {
                id: buttonLayout

                property bool opened: true

                Button {

                    Text {
                        text: buttonLayout.opened ? "<" : ">"
                    }

                    onClicked: {

                        buttonLayout.opened = !buttonLayout.opened
                        leftPanel.width = buttonLayout.opened ? 220 * root.scaleX : 50 * root.scaleX
                    }
                }

                Text {
                    id: text1

                    text: buttonLayout.opened ? "Collapse" : ""
                }
            }

        }
    }

    Rectangle {
        id: middlePanel

        color: "#0A0A0A"

        anchors {
            bottom: bottomPanel.top
            left: leftPanel.right
            right: parent.right
            top: parent.top
        }
    }
}