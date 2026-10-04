import QtQuick
import SoundLink 1.0

Item {
    id: root

    property string iconName: ""
    property real iconSize: 18
    property bool active: false
    // A tiny mark next to the icon, e.g. "1" for repeat one.
    property string badge: ""
    property bool hovered: hover.hovered

    signal clicked

    implicitWidth: 32
    implicitHeight: 32

    AppIcon {
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        name: root.iconName
        color: root.active ? Theme.palette.iconActive
             : root.hovered ? Theme.palette.iconActive
                            : Theme.palette.iconDefault

        Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }
    }

    Text {
        visible: root.badge.length > 0
        x: parent.width / 2 + root.iconSize / 2 - 2
        y: parent.height / 2 - root.iconSize / 2 - 4
        text: root.badge
        color: Theme.palette.iconActive
        font.family: Theme.typography.family
        font.pixelSize: 9
        font.weight: Font.Bold
    }

    // Toggles show they are on with a dot under the icon.
    Rectangle {
        visible: root.active
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height / 2 + root.iconSize / 2 + 3
        width: 4; height: 4
        radius: 2
        color: Theme.palette.iconActive
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }
}
