import QtQuick

Rectangle {
    id: root

    property bool hovered: false
    property bool isClose: false
    property string text: ""

    signal clicked

    color: hovered ? (isClose ? "#CC0000" : "#303030") : "transparent"
    height: 36
    width: 46

    Text {
        anchors.centerIn: parent
        color: root.hovered ? "#EEEEEE" : "#BBB"
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