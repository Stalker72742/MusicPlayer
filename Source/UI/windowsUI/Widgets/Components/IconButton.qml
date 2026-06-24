import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property string iconName: ""
    property color  iconColor: Theme.palette.iconDefault
    property color  iconHoverColor: Theme.palette.iconActive
    property real   iconSize: Theme.metrics.iconSize
    property bool   active: false
    property real   cornerRadius: Theme.metrics.radiusMd
    property bool   hovered: hover.hovered
    property bool   pressed: tap.pressed

    signal clicked

    implicitWidth: 32
    implicitHeight: 32

    color: pressed ? Theme.palette.pressedOverlay
                   : (hovered || active ? Theme.palette.hoverOverlay : "transparent")
    radius: cornerRadius

    Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

    AppIcon {
        anchors.centerIn: parent
        width: root.iconSize
        height: root.iconSize
        name: root.iconName
        color: (root.hovered || root.active) ? root.iconHoverColor : root.iconColor

        Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler   { id: tap; onTapped: root.clicked() }
}
