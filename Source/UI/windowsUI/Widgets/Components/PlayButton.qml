import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property bool playing: false
    property bool hovered: hover.hovered
    property real diameter: 44

    signal clicked

    implicitWidth: diameter
    implicitHeight: diameter
    width: diameter
    height: diameter
    radius: diameter / 2

    color: hovered ? "#FFFFFF" : "#ECECEC"
    scale: tap.pressed ? 0.95 : 1.0

    Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }
    Behavior on scale { NumberAnimation { duration: Theme.motion.durationFast } }

    AppIcon {
        anchors.centerIn: parent
        width: 18
        height: 18
        name: root.playing ? "pause" : "play"
        color: "#0E0E10"
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: root.clicked() }
}
