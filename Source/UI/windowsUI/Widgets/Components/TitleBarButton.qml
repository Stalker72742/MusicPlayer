import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property string iconName: ""
    property bool   isClose: false
    property bool   hovered: hover.hovered

    signal clicked

    implicitWidth: 46
    implicitHeight: Theme.metrics.titleBarHeight

    color: hovered
        ? (isClose ? Theme.palette.danger : Theme.palette.hoverOverlay)
        : "transparent"

    Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

    AppIcon {
        anchors.centerIn: parent
        width: 14
        height: 14
        name: root.iconName
        color: root.hovered
            ? (root.isClose ? "#FFFFFF" : Theme.palette.iconActive)
            : Theme.palette.iconDefault
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked() }
}
