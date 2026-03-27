import QtQuick

Rectangle {
    id: root

    property bool hovered: false
    property bool isClose: false
    property string text: ""

    signal clicked

    color: hovered ? (isClose ? "#f38ba8" : "#313244") : "transparent"
    height: 36
    width: 46

    Text {
        anchors.centerIn: parent
        color: root.hovered && root.isClose ? "#1e1e2e" : "#cdd6f4"
        font.pixelSize: 14
        text: root.text
    }
    HoverHandler {
        onHoveredChanged: root.hovered = hovered
    }
    TapHandler {
        onTapped: root.clicked()
    }
}