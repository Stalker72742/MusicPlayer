import QtQuick
import SoundLink 1.0

Rectangle {
    id: root

    property string text: ""
    property bool selected: false
    property bool hovered: hover.hovered

    signal clicked

    implicitWidth: label.implicitWidth + 28
    implicitHeight: 30
    radius: Theme.metrics.radiusMd

    color: selected ? Theme.palette.pillSelected
                    : (hovered ? Theme.palette.hoverOverlay : "transparent")

    Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.selected ? Theme.palette.textPrimary : Theme.palette.textSecondary
        font.family: Theme.typography.family
        font.pixelSize: Theme.typography.bodySize
        font.weight: root.selected ? Font.DemiBold : Font.Normal

        Behavior on color { ColorAnimation { duration: Theme.motion.durationFast } }
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }
}
