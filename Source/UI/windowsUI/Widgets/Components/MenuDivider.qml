import QtQuick
import SoundLink 1.0

// A line between groups of a menu popup.
Item {
    width: 232
    height: visible ? 9 : 0

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        height: 1
        color: Theme.palette.hairline
    }
}
