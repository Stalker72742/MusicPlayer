import QtQuick
import SoundLink 1.0

Item {
    id: root

    property string iconName: ""
    property real iconSize: 18
    property bool active: false
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

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }
}
